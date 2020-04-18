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

#include "realtimetripinformation.h"

#include <QJsonArray>

namespace GTFS {

RealtimeTripInformation::RealtimeTripInformation()
    : StaticStatus()
{
    _rTrips = RealTimeGateway::inst().getActiveFeed();
    RealTimeGateway::inst().realTimeTransactionHandled();
}

void RealtimeTripInformation::fillResponseData(QJsonObject &resp)
{
    if (_rTrips == nullptr) {
        fillProtocolFields("RTI", 0, resp);
        return;
    }

    QMap<QString, QVector<QString> > addedRouteTrips, activeRouteTrips, cancelledRouteTrips;

    _rTrips->getAllTripsWithPredictions(addedRouteTrips, activeRouteTrips, cancelledRouteTrips);

    // Canceled Trips
    QJsonObject canceledCollection;
    for (const QString &routeID : cancelledRouteTrips.keys()) {
        QJsonArray canceledRoute;
        for (const QString &tripID : cancelledRouteTrips[routeID]) {
            canceledRoute.push_back(tripID);
        }
        canceledCollection[routeID] = canceledRoute;
    }
    resp["canceled_trips"] = canceledCollection;

    // Added Trips
    QJsonObject addedCollection;
    for (const QString &routeID : addedRouteTrips.keys()) {
        QJsonArray addedRoute;
        for (const QString &tripID : addedRouteTrips[routeID]) {
            addedRoute.push_back(tripID);
        }
        addedCollection[routeID] = addedRoute;
    }
    resp["added_trips"] = addedCollection;

    // Active-Scheduled Trips
    QJsonObject activeCollection;
    for (const QString &routeID : activeRouteTrips.keys()) {
        QJsonArray activeRoute;
        for (const QString &tripID : activeRouteTrips[routeID]) {
            activeRoute.push_back(tripID);
        }
        activeCollection[routeID] = activeRoute;
    }
    resp["active_trips"] = activeCollection;

    // Successful completion
    fillProtocolFields("RTI", 0, resp);
}

void RealtimeTripInformation::dumpRealTime(QString &decodedRealtime)
{
    if (_rTrips == nullptr) {
        return;
    }

    _rTrips->serializeTripUpdates(decodedRealtime);
}

}  // Namespace GTFS
