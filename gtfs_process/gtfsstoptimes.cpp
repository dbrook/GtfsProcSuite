/*
 * GtfsProc_Server
 * Copyright (C) 2018-2019, Daniel Brook
 *
 * This file is part of GtfsProc.
 *
 * GtfsProc is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * GtfsProc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with GtfsProc.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 * See included LICENSE.txt file for full license.
 */

#include "gtfsstoptimes.h"
#include "csvprocessor.h"

#include <QDebug>
#include <algorithm>

namespace GTFS {

const qint32 StopTimes::s_localNoonSec = 43200; // 12 * 60 * 60

StopTimes::StopTimes(const QString dataRootPath, QObject *parent) : QObject(parent)
{
    QVector<QVector<QString>> dataStore;

    // Read in the feed information
    qDebug() << "Starting Stop-Time Process";
    CsvProcess((dataRootPath + "/stop_times.txt").toUtf8(), &dataStore);
    qint8 tripIdPos, stopSeqPos, stopIdPos, arrTimePos, depTimePos, dropOffPos, pickupPos, stopHeadsignPos;
    stopTimesCSVOrder(dataStore.at(0),
                      tripIdPos, stopSeqPos, stopIdPos, arrTimePos, depTimePos, dropOffPos, pickupPos, stopHeadsignPos);

    // Ingest the data, organize by trip_id
    for (int l = 1; l < dataStore.size(); ++l) {
        StopTimeRec stopTime;
        stopTime.stop_sequence  = dataStore.at(l).at(stopSeqPos).toInt();
        stopTime.stop_id        = dataStore.at(l).at(stopIdPos);
        stopTime.arrival_time   = computeSecondsLocalNoonOffset(dataStore.at(l).at(arrTimePos));
        stopTime.departure_time = computeSecondsLocalNoonOffset(dataStore.at(l).at(depTimePos));
        stopTime.drop_off_type  = (dropOffPos != -1)      ? dataStore.at(l).at(dropOffPos).toShort() : 0;
        stopTime.pickup_type    = (pickupPos != -1)       ? dataStore.at(l).at(pickupPos).toShort()  : 0;
        stopTime.stop_headsign  = (stopHeadsignPos != -1) ? dataStore.at(l).at(stopHeadsignPos)    : "";

        this->stopTimeDb[dataStore.at(l).at(tripIdPos)].push_back(stopTime);
    }

    // The stop times aren't always sorted by the squence number (stop_sequence)
    qDebug() << "Sorting StopTimes by Sequence within each TripID";
    for (auto entry : this->stopTimeDb.keys()) {
        std::sort(this->stopTimeDb[entry].begin(), this->stopTimeDb[entry].end(), StopTimes::compareByStopSequence);
    }
}

qint64 StopTimes::getStopTimesDBSize() const
{
    qint64 items = 0;

    for (const QVector<StopTimeRec> &entry : this->stopTimeDb) {
        items += entry.size();
    }

    return items;
}

const StopTimeData &StopTimes::getStopTimesDB() const
{
    return this->stopTimeDb;
}

qint64 StopTimes::duplicateTripWithTimeRange(const QString &trip_id,
                                             const QString &uniqueChar,
                                             qint32         startRange,
                                             qint32         endRange,
                                             qint32         headway,
                                             QVector<QString> &generatedTrips)
{
    qint64 stopTimesAdded = 0;

    // Duplicate an entire trip until we have used up all headways which can fall in the time range.
    // NOTE: The GTFS Specification says we use the endRange time as a valid trip


    const QVector<StopTimeRec> &baseRecord = stopTimeDb[trip_id];
    qint32                      maxHeadway = endRange - startRange;

//    for (qint32 currentHeadway = headway; currentHeadway <= maxHeadway; currentHeadway += headway) {
    for (qint32 currentHeadway = 0; currentHeadway <= maxHeadway; currentHeadway += headway) {
        // We need to make a dynamic trip_id
        QString newTripId = trip_id + "_D" + uniqueChar + QString::number(currentHeadway);
        generatedTrips.push_back(newTripId);

        // For each stop_time in the trip, we need to generate new times:
        for (const StopTimeRec &br : baseRecord) {
            StopTimeRec nr;
            nr.drop_off_type  = br.drop_off_type;
            nr.pickup_type    = br.pickup_type;
            nr.stop_id        = br.stop_id;
            nr.stop_sequence  = br.stop_sequence;
            nr.stop_headsign  = br.stop_headsign;
            nr.arrival_time   = br.arrival_time   + (startRange - s_localNoonSec) + currentHeadway;
            nr.departure_time = br.departure_time + (startRange - s_localNoonSec) + currentHeadway;

            ++stopTimesAdded;
            stopTimeDb[newTripId].push_back(nr);
        }
    }

    // Tell how many records were added
    return stopTimesAdded;
}

void StopTimes::stopTimesCSVOrder(const QVector<QString> csvHeader,
                                  qint8 &tripIdPos,
                                  qint8 &stopSeqPos,
                                  qint8 &stopIdPos,
                                  qint8 &arrTimePos,
                                  qint8 &depTimePos,
                                  qint8 &dropOffPos,
                                  qint8 &pickupPos,
                                  qint8 &stopHeadsignPos)
{
    tripIdPos = stopSeqPos = stopIdPos = arrTimePos = depTimePos = dropOffPos = pickupPos = stopHeadsignPos = -1;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "trip_id") {
            tripIdPos = position;
        } else if (item == "stop_sequence") {
            stopSeqPos = position;
        } else if (item == "stop_id") {
            stopIdPos = position;
        } else if (item == "arrival_time") {
            arrTimePos = position;
        } else if (item == "departure_time") {
            depTimePos = position;
        } else if (item == "drop_off_type") {
            dropOffPos = position;
        } else if (item == "pickup_type") {
            pickupPos = position;
        } else if (item == "stop_headsign") {
            stopHeadsignPos = position;
        }
        ++position;
    }
}

qint32 StopTimes::computeSecondsLocalNoonOffset(const QString &hhmmssTime)
{
    /*
     * Time comes in in the format of hh:mm:ss but with a 24+ hour format to deal with trip times that
     * occur past midnight the following day but for operational trips that fall on the previous day's schedule
     * It is not unusual to see times posted like 25:14 for a stop at 1:14 AM the following day, for instance
     */
    if (hhmmssTime == "") {
        return -1;
    }

    qint32 firstColon = hhmmssTime.indexOf(":");
    qint32 hours      = hhmmssTime.left(firstColon).toInt();
    qint32 seconds    = hhmmssTime.right(2).toInt();
    qint32 minutes    = hhmmssTime.midRef(firstColon + 1, 2).toInt();

    return (hours * 3600 + minutes * 60 + seconds) - s_localNoonSec;
}

bool StopTimes::compareByStopSequence(const StopTimeRec &a, const StopTimeRec &b)
{
    return a.stop_sequence < b.stop_sequence;
}

} // Namespace GTFS
