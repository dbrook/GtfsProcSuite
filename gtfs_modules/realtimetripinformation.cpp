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

    QHash<QString, QVector<QString>> addedRouteTrips, activeRouteTrips, cancelledRouteTrips, mismatchTripIdx;
    QHash<QString, QHash<QString, QVector<qint32>>> duplicateTripIdx;
    QVector<QString> tripsWithoutRoute;
    _rTrips->getAllTripsWithPredictions(addedRouteTrips,
                                        activeRouteTrips,
                                        cancelledRouteTrips,
                                        mismatchTripIdx,
                                        duplicateTripIdx,
                                        tripsWithoutRoute);

    // Canceled Trips
    QJsonObject canceledCollection;
    for (const QString &routeID : cancelledRouteTrips.keys()) {
        QJsonArray canceledRoute;
        for (const QString &tripID : qAsConst(cancelledRouteTrips[routeID])) {
            canceledRoute.push_back(tripID);
        }
        canceledCollection[routeID] = canceledRoute;
    }
    resp["canceled_trips"] = canceledCollection;

    // Added Trips
    QJsonObject addedCollection;
    for (const QString &routeID : addedRouteTrips.keys()) {
        QJsonArray addedRoute;
        for (const QString &tripID : qAsConst(addedRouteTrips[routeID])) {
            addedRoute.push_back(tripID);
        }
        addedCollection[routeID] = addedRoute;
    }
    resp["added_trips"] = addedCollection;

    // Active-Scheduled Trips
    QJsonObject activeCollection;
    for (const QString &routeID : activeRouteTrips.keys()) {
        QJsonArray activeRoute;
        for (const QString &tripID : qAsConst(activeRouteTrips[routeID])) {
            activeRoute.push_back(tripID);
        }
        activeCollection[routeID] = activeRoute;
    }
    resp["active_trips"] = activeCollection;

    // Routes without Trips / "Orphaned Trips"
    QJsonArray orphanedAll;
    for (const QString &tripID : qAsConst(tripsWithoutRoute)) {
        orphanedAll.push_back(tripID);
    }
    resp["orphaned_trips"] = orphanedAll;

    // Trips with Mismatching Stop Data vs. Schedule from the Static Feed
    QJsonObject mismatchCollection;
    for (const QString &routeID : mismatchTripIdx.keys()) {
        QJsonArray mismatchRoute;
        for (const QString &tripID : qAsConst(mismatchTripIdx[routeID])) {
            mismatchRoute.push_back(tripID);
        }
        mismatchCollection[routeID] = mismatchRoute;
    }
    resp["mismatch_trips"] = mismatchCollection;

    // Real-Time Trip Update indexes per route/trip that are duplicates
    QJsonObject duplicateCollection;
    for (const QString &routeID : duplicateTripIdx.keys()) {
        QJsonObject duplicateRoutes;
        for (const QString &tripID : duplicateTripIdx[routeID].keys()) {
            QJsonArray duplicateTripsRoute;
            for (qint32 rttuEntityIdx : qAsConst(duplicateTripIdx[routeID][tripID])) {
                duplicateTripsRoute.push_back(rttuEntityIdx);
            }
            duplicateRoutes[tripID] = duplicateTripsRoute;
        }
        duplicateCollection[routeID] = duplicateRoutes;
    }
    resp["duplicate_trips"] = duplicateCollection;

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
