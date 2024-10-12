/*
 * GtfsProc_Server
 * Copyright (C) 2018-2024, Daniel Brook
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
#include <limits>

namespace GTFS {

const qint32 StopTimes::s_localNoonSec = 43200; // 12 * 60 * 60
const qint32 StopTimes::kNoTime        = std::numeric_limits<qint32>::max();
const double StopTimes::s_noDistance   = -10000;

StopTimes::StopTimes(const QString dataRootPath, QObject *parent) : QObject(parent)
{
    QVector<QVector<QString>> dataStore;

    // Read in the feed information
    qDebug() << "Starting Stop-Time Process ...";
    CsvProcess((dataRootPath + "/stop_times.txt").toUtf8(), &dataStore);
    qint8 tripIdPos, stopSeqPos, stopIdPos, arrTimePos, depTimePos, dropOffPos, pickupPos, stopHeadsignPos, sdtPos;
    stopTimesCSVOrder(dataStore.at(0),
                      tripIdPos, stopSeqPos, stopIdPos, arrTimePos, depTimePos,
                      dropOffPos, pickupPos, stopHeadsignPos, sdtPos);

    // Ingest the data, organize by trip_id
    for (int l = 1; l < dataStore.size(); ++l) {
        StopTimeRec stopTime;
        stopTime.stop_sequence  = dataStore.at(l).at(stopSeqPos).toInt();
        stopTime.stop_id        = dataStore.at(l).at(stopIdPos);
        stopTime.arrival_time   = computeSecondsLocalNoonOffset(dataStore.at(l).at(arrTimePos));
        stopTime.departure_time = computeSecondsLocalNoonOffset(dataStore.at(l).at(depTimePos));
        stopTime.drop_off_type  = (dropOffPos != -1)      ? dataStore.at(l).at(dropOffPos).toShort() :  0;
        stopTime.pickup_type    = (pickupPos != -1)       ? dataStore.at(l).at(pickupPos).toShort()  :  0;
        stopTime.stop_headsign  = (stopHeadsignPos != -1) ? dataStore.at(l).at(stopHeadsignPos)      : "";
        stopTime.distance       = (sdtPos != -1)          ? dataStore.at(l).at(sdtPos).toDouble()    : s_noDistance;
        stopTime.interpolated   = false;

        this->stopTimeDb[dataStore.at(l).at(tripIdPos)].push_back(stopTime);
    }

    // The stop times aren't always sorted by the squence number (stop_sequence)
    qDebug() << "  Sort StopTimes by sequence within each trip ...";
    for (const QString &tripId : this->stopTimeDb.keys()) {
        std::sort(this->stopTimeDb[tripId].begin(), this->stopTimeDb[tripId].end(), StopTimes::compareByStopSequence);
    }

    // Identify trips that have distances and times that require interpolation
    qDebug() << "  Interpolate schedules (as needed) for each trip ...";
    for (const QString &tripId : this->stopTimeDb.keys()) {
        quint32 tripHasInterp  = 0;
        bool    tripAllHasDist = true;
        for (const StopTimeRec &stopTime : this->stopTimeDb[tripId]) {
            if (stopTime.arrival_time == kNoTime && stopTime.departure_time == kNoTime) {
                ++tripHasInterp;
            }
            if (stopTime.distance == s_noDistance) {
                tripAllHasDist = false;
            }
        }
        // qDebug() << "    TripID" << tripId << " interp[" << tripHasInterp << "]  distance[" << tripAllHasDist << "]";

        if (!tripAllHasDist || tripHasInterp == 0) continue;

        // Find range to fill in / interpolate times for
        bool startIdx = 0;
        while (true) {
            qint32 begTimeIdx   = -1;
            qint32 begInterpIdx = -1;
            qint32 endInterpIdx = -1;
            qint32 endTimeIdx   = -1;
            for (qint32 idx = startIdx; idx < this->stopTimeDb[tripId].length(); ++idx) {
                if (this->stopTimeDb[tripId][idx].arrival_time == kNoTime  &&
                    this->stopTimeDb[tripId][idx].departure_time == kNoTime  ) {
                    begTimeIdx   = idx - 1;
                    begInterpIdx = idx;
                    startIdx     = idx;  // So it's known where to start on the next loop
                    break;
                }
            }
            if (begTimeIdx == -1) break;

            for (qint32 idx = begInterpIdx; idx < this->stopTimeDb[tripId].length(); ++idx) {
                if (!(this->stopTimeDb[tripId][idx].arrival_time == kNoTime   &&
                      this->stopTimeDb[tripId][idx].departure_time == kNoTime)  ) {
                    endTimeIdx   = idx;
                    endInterpIdx = idx - 1;
                    startIdx     = idx;  // So it's known where to start on the next loop
                    break;
                }
            }
            if (endTimeIdx == -1) break;

            float begDist = this->stopTimeDb[tripId][begTimeIdx].distance;
            float begTime = this->stopTimeDb[tripId][begTimeIdx].departure_time == kNoTime
                                ? this->stopTimeDb[tripId][begTimeIdx].arrival_time
                                : this->stopTimeDb[tripId][begTimeIdx].departure_time;
            float endTime = this->stopTimeDb[tripId][endTimeIdx].arrival_time == kNoTime
                                ? this->stopTimeDb[tripId][endTimeIdx].departure_time
                                : this->stopTimeDb[tripId][endTimeIdx].arrival_time;
            float avrgVel = (this->stopTimeDb[tripId][endTimeIdx].distance - begDist) / (endTime - begTime);

            for (qint32 idx = begInterpIdx; idx <= endInterpIdx; ++idx) {
                this->stopTimeDb[tripId][idx].arrival_time =
                    (this->stopTimeDb[tripId][idx].distance - begDist) / avrgVel + begTime;
                this->stopTimeDb[tripId][idx].departure_time = this->stopTimeDb[tripId][idx].arrival_time;
                this->stopTimeDb[tripId][idx].interpolated = true;
            }
        }
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

void StopTimes::stopTimesCSVOrder(const QVector<QString> csvHeader,
                                  qint8 &tripIdPos,
                                  qint8 &stopSeqPos,
                                  qint8 &stopIdPos,
                                  qint8 &arrTimePos,
                                  qint8 &depTimePos,
                                  qint8 &dropOffPos,
                                  qint8 &pickupPos,
                                  qint8 &stopHeadsignPos,
                                  qint8 &sdtPos)
{
    qint8 position = 0;
    tripIdPos = stopSeqPos = stopIdPos = arrTimePos = depTimePos = dropOffPos = pickupPos = stopHeadsignPos = sdtPos
        = -1;

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
        } else if (item == "shape_dist_traveled") {
            sdtPos = position;
        }
        ++position;
    }
}

qint32 StopTimes::computeSecondsLocalNoonOffset(QStringView hhmmssTime)
{
    /*
     * Time comes in in the format of hh:mm:ss but with a 24+ hour format to deal with trip times that occur past
     * midnight the following day but for operational trips that fall on the previous calendar day's schedule.
     * It is not unusual to see times posted like 25:14 for a stop at 1:14 AM the following day, for instance.
     */
    if (hhmmssTime.isEmpty()) {
        return kNoTime;
    }

    qint32 firstColon = hhmmssTime.indexOf(':');
    qint32 hours      = hhmmssTime.left(firstColon).toInt();
    qint32 seconds    = hhmmssTime.right(2).toInt();
    qint32 minutes    = hhmmssTime.mid(firstColon + 1, 2).toInt();

    return (hours * 3600 + minutes * 60 + seconds) - s_localNoonSec;
}

bool StopTimes::compareByStopSequence(const StopTimeRec &a, const StopTimeRec &b)
{
    return a.stop_sequence < b.stop_sequence;
}

} // Namespace GTFS
