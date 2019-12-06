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

#include "gtfsroute.h"
#include "csvprocessor.h"

#include <algorithm>

#include <QDebug>

namespace GTFS {

Routes::Routes(const QString dataRootPath, QObject *parent) : QObject(parent)
{
    QVector<QVector<QString>> dataStore;

    // Read in the feed information
    qDebug() << "Starting Route Process...";
    CsvProcess((dataRootPath + "/routes.txt").toUtf8(), &dataStore);
    qint8 idPos, agencyIdPos, shortNamePos, longNamePos, descPos, typePos, urlPos, colorPos, textColorPos;
    routesCSVOrder(dataStore.at(0),
                   idPos, agencyIdPos, shortNamePos, longNamePos, descPos, typePos, urlPos, colorPos, textColorPos);

    // Agencies have a really wide range of ways these data points could be filled, including not showing at all, so
    // to be safe we just check them all against the default/not-found value of -1
    for (int l = 1; l < dataStore.size(); ++l) {
        RouteRec route;
        route.agency_id        = (agencyIdPos  != -1) ? dataStore.at(l).at(agencyIdPos).toInt() : 0;  // 0-by-default?
        route.route_short_name = (shortNamePos != -1) ? dataStore.at(l).at(shortNamePos) : "";
        route.route_long_name  = (longNamePos  != -1) ? dataStore.at(l).at(longNamePos) : "";
        route.route_desc       = (descPos      != -1) ? dataStore.at(l).at(descPos) : "";
        route.route_type       = (typePos      != -1) ? dataStore.at(l).at(typePos) : "";
        route.route_url        = (urlPos       != -1) ? dataStore.at(l).at(urlPos) : "";
        route.route_color      = (colorPos     != -1) ? dataStore.at(l).at(colorPos) : "";
        route.route_text_color = (textColorPos != -1) ? dataStore.at(l).at(textColorPos) : "";

        this->routeDb[dataStore.at(l).at(idPos)] = route;
    }
}

qint64 Routes::getRoutesDBSize() const
{
    return this->routeDb.size();
}

const RouteData &Routes::getRoutesDB() const
{
    return this->routeDb;
}

void Routes::connectTrip(const QString &routeID, const QString &tripID,
                         const qint32 fstDepTime, const qint32 fstArrTime)
{
    QPair<QString, qint32> tripIDwithTime;

    // Warning: this assumes (and it should be fine) that the first stop on a trip is ALWAYS with a time. If we don't
    // see a departure time, then we will take the arrival time. If neither exist: oh well :-(
    if (fstDepTime != -1) {
        // Append first departure time
        tripIDwithTime = {tripID, fstDepTime};
    } else {
        // Append first arrival time if the first departure time isn't available.
        tripIDwithTime = {tripID, fstArrTime};
    }
    this->routeDb[routeID].trips.push_back(tripIDwithTime);
}

void Routes::connectStop(const QString &routeID, const QString &stopID)
{
    if (!this->routeDb[routeID].stopService.contains(stopID)) {
        this->routeDb[routeID].stopService[stopID] = 0;
    }
    ++this->routeDb[routeID].stopService[stopID];
}

void Routes::sortRouteTrips()
{
    // Scan through all the trips associated to this route (previously filled with connectTrip) so we can put them
    // in chronological order (based on the first departure time) with a helper sort function?
    for (const QString &routeID : this->routeDb.keys()) {
//        qDebug() << "Sorting Route: " << route.route_short_name << "::" << route.route_long_name;
        std::sort(this->routeDb[routeID].trips.begin(),
                  this->routeDb[routeID].trips.end(),
                  Routes::compareByTripStartTime);
    }
}

bool Routes::compareByTripStartTime(const QPair<QString, qint32> &tripA, const QPair<QString, qint32> &tripB)
{
    return tripA.second < tripB.second;
}

void Routes::routesCSVOrder(const QVector<QString> csvHeader,
                            qint8 &idPos,
                            qint8 &agencyIdPos,
                            qint8 &shortNamePos,
                            qint8 &longNamePos,
                            qint8 &descPos,
                            qint8 &typePos,
                            qint8 &urlPos,
                            qint8 &colorPos,
                            qint8 &textColorPos)
{
    // route_id,agency_id,route_short_name,route_long_name,route_desc,route_type,route_url,route_color,route_text_color
    idPos = agencyIdPos = shortNamePos = longNamePos = descPos = typePos = urlPos = colorPos = textColorPos = -1;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "route_id") {
            idPos = position;
        } else if (item == "agency_id") {
            agencyIdPos = position;
        } else if (item == "route_short_name") {
            shortNamePos = position;
        } else if (item == "route_long_name") {
            longNamePos = position;
        } else if (item == "route_desc") {
            descPos = position;
        } else if (item == "route_type") {
            typePos = position;
        } else if (item == "route_url") {
            urlPos = position;
        } else if (item == "route_color") {
            colorPos = position;
        } else if (item == "route_text_color") {
            textColorPos = position;
        }
        ++position;
    }
}
}  // Namespace GTFS
