/*
 * GtfsProc_Server
 * Copyright (C) 2018-2020, Daniel Brook
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

#include "gtfstrip.h"
#include "csvprocessor.h"

#include <QDebug>

namespace GTFS {

Trips::Trips(const QString dataRootPath, QObject *parent) : QObject(parent)
{
    QVector<QVector<QString>> dataStore;

    // Read the feed information
    qDebug() << "Starting Trip Process...";
    CsvProcess((dataRootPath + "/trips.txt").toUtf8(), &dataStore);
    qint8 routeIdPos, tripIdPos, serviceIdPos, headsignPos, tripShortNamePos = -1;
    tripsCSVOrder(dataStore.at(0), routeIdPos, tripIdPos, serviceIdPos, headsignPos, tripShortNamePos);

    for (int l = 1; l < dataStore.size(); ++l) {
        TripRec trip;
        trip.route_id        = dataStore.at(l).at(routeIdPos);
        trip.service_id      = dataStore.at(l).at(serviceIdPos);
        trip.trip_headsign   = (headsignPos != -1) ? dataStore.at(l).at(headsignPos) : "";
        trip.trip_short_name = (tripShortNamePos != -1) ? dataStore.at(l).at(tripShortNamePos) : "";

        this->tripDb[dataStore.at(l).at(tripIdPos)] = trip;
    }
}

qint64 Trips::getTripsDBSize() const
{
    return this->tripDb.size();
}

const TripData &Trips::getTripsDB() const
{
    return this->tripDb;
}

void Trips::tripsCSVOrder(const QVector<QString> csvHeader,
                          qint8 &routeIdPos,
                          qint8 &tripIdPos,
                          qint8 &serviceIdPos,
                          qint8 &headsignPos,
                          qint8 &tripShortNamePos)
{
    routeIdPos = tripIdPos = serviceIdPos = headsignPos = -1;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "route_id") {
            routeIdPos = position;
        } else if (item == "trip_id") {
            tripIdPos = position;
        } else if (item == "service_id") {
            serviceIdPos = position;
        } else if (item == "trip_headsign") {
            headsignPos = position;
        } else if (item == "trip_short_name") {
            tripShortNamePos = position;
        }
        ++position;
    }
}

} // Namespace GTFS
