/*
 * GtfsProc_Server
 * Copyright (C) 2018-2022, Daniel Brook
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

#include "gtfsstops.h"
#include "csvprocessor.h"

#include <algorithm>
#include <QDebug>

namespace GTFS {

Stops::Stops(const QString dataRootPath, QObject *parent) : QObject(parent)
{
    QVector<QVector<QString>> dataStore;

    // Read feed information
    qDebug() << "Starting Stops Information Process ...";
    CsvProcess((dataRootPath + "/stops.txt").toUtf8(), &dataStore);
    qint8 stopIdPos, stopNamePos, stopDescPos, stopLatPos, stopLonPos, parentStationPos;
    stopsCSVOrder(dataStore.at(0), stopIdPos, stopDescPos, stopNamePos, stopLatPos, stopLonPos, parentStationPos);

    // Ingest the data and store it by stop_id
    for (int l = 1; l < dataStore.size(); ++l) {
        StopRec stop;
        stop.stop_name      = dataStore.at(l).at(stopNamePos);
        stop.stop_desc      = dataStore.at(l).at(stopDescPos);
        stop.stop_lat       = dataStore.at(l).at(stopLatPos);
        stop.stop_lon       = dataStore.at(l).at(stopLonPos);
        stop.parent_station = dataStore.at(l).at(parentStationPos);

        this->stopsDb[dataStore.at(l).at(stopIdPos)] = stop;

        if (stop.parent_station != "") {
            this->parentStopDb[stop.parent_station].append(dataStore.at(l).at(stopIdPos));
        }
    }
}

qint64 Stops::getStopsDBSize() const
{
    // Stop database size
    qint64 itemCount = this->stopsDb.size();

    // ... and the parent-station mapping supplementary database
    for (const QVector<QString> &entry : this->parentStopDb) {
        itemCount += entry.size();
    }

    return itemCount;
}

const StopData &Stops::getStopDB() const
{
    return this->stopsDb;
}

const ParentStopData &Stops::getParentStationDB() const
{
    return this->parentStopDb;
}

void Stops::connectTripRoute(const QString &StopID,
                             const QString &TripID,
                             const QString &RouteID,
                             qint32         TripSequence,
                             qint32         sortTime)
{
    tripStopSeqInfo tssi;
    tssi.sortTime    = sortTime;
    tssi.tripID      = TripID;
    tssi.tripStopIndex = TripSequence;
    this->stopsDb[StopID].stopTripsRoutes[RouteID].push_back(tssi);
}

void Stops::sortStopTripTimes()
{
    for (const QString &stopID : this->stopsDb.keys()) {
        for (const QString &routeID : this->stopsDb[stopID].stopTripsRoutes.keys()) {
            std::sort(this->stopsDb[stopID].stopTripsRoutes[routeID].begin(),
                      this->stopsDb[stopID].stopTripsRoutes[routeID].end(),
                      Stops::compareStopTrips);
        }
    }
}

bool Stops::compareStopTrips(tripStopSeqInfo &tripStop1, tripStopSeqInfo &tripStop2)
{
    return tripStop1.sortTime < tripStop2.sortTime;
}

void Stops::stopsCSVOrder(const QVector<QString> csvHeader,
                          qint8 &stopIdPos,
                          qint8 &stopDescPos,
                          qint8 &stopNamePos,
                          qint8 &stopLatPos,
                          qint8 &stopLonPos,
                          qint8 &parentStationPos)
{
    stopIdPos = stopNamePos = stopDescPos = stopLatPos = stopLonPos = parentStationPos = 0;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "stop_id") {
            stopIdPos = position;
        } else if (item == "stop_name") {
            stopNamePos = position;
        } else if (item == "stop_desc") {
            stopDescPos = position;
        } else if (item == "stop_lat") {
            stopLatPos = position;
        } else if (item == "stop_lon") {
            stopLonPos = position;
        } else if (item == "parent_station") {
            parentStationPos = position;
        }

        ++position;
    }
}

} // Namespace GTFS
