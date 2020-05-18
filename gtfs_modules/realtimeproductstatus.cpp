/*
 * GtfsProc_Server
 * Copyright (C) 2020, Daniel Brook
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

#include "realtimeproductstatus.h"

#include "datagateway.h"

namespace GTFS {

typedef struct {
    qint32 scheduled;
    qint32 added;
    qint32 canceled;
    qint32 duplicated;
    qint32 mismatched;
} routeRtStats;

RealtimeProductStatus::RealtimeProductStatus() : StaticStatus(), _rg(GTFS::RealTimeGateway::inst())
{
    _stat = GTFS::DataGateway::inst().getStatus();
}

void RealtimeProductStatus::fillResponseData(QJsonObject &resp)
{
    resp["uptm_ms"] = _stat->getServerStartTimeUTC().msecsTo(QDateTime::currentDateTimeUtc());
    resp["statdat"] = _stat->getStaticDatasetModifiedTime().toString("dd-MMM-yyyy hh:mm:ss t");
    resp["nb_reqs"] = GTFS::DataGateway::inst().getHandledRequests();

    if (_stat->format12h()) {
        resp["ltst_rt"] = _rg.mostRecentTransaction().toTimeZone(getAgencyTime().timeZone())
                                                     .toString("dd-MMM-yyyy h:mm:ss a t");
    } else {
        resp["ltst_rt"] = _rg.mostRecentTransaction().toTimeZone(getAgencyTime().timeZone())
                                                     .toString("dd-MMM-yyyy hh:mm:ss t");
    }

    QString activeSideStr;
    GTFS::RealTimeDataRepo    active = _rg.activeBuffer();
    if (active == GTFS::SIDE_A) {
        activeSideStr = "A";
    } else if (active == GTFS::SIDE_B) {
        activeSideStr = "B";
    } else if (active == GTFS::IDLED) {
        activeSideStr = "IDLE";
    } else {
        activeSideStr = "NONE";
    }
    resp["rt_buff"] = activeSideStr;

    GTFS::RealTimeTripUpdate *rTrips = _rg.getActiveFeed();
    if (rTrips) {
        QDateTime activeFeedTime = rTrips->getFeedTime();
        if (activeFeedTime.isNull()) {
            resp["datagen"] = "-";
            resp["age_sec"] = "-";
        } else if (getStatus()->format12h()) {
            resp["datagen"] = activeFeedTime.toTimeZone(getAgencyTime().timeZone())
                                            .toString("dd-MMM-yyyy h:mm:ss a t");
            resp["age_sec"] = activeFeedTime.secsTo(getAgencyTime());
        } else {
            resp["datagen"] = activeFeedTime.toTimeZone(getAgencyTime().timeZone())
                                            .toString("dd-MMM-yyyy hh:mm:ss t");
            resp["age_sec"] = activeFeedTime.secsTo(getAgencyTime());
        }

        resp["gtfsrtv"] = rTrips->getFeedGTFSVersion();
        resp["dwnldms"] = rTrips->getDownloadTimeMSec();
        resp["integms"] = rTrips->getIntegrationTimeMSec();

        QHash<QString, QVector<QString>> addedRouteTrips, activeRouteTrips, canceledRouteTrips, mismatchedTrips;
        QHash<QString, QHash<QString, QVector<qint32>>> duplicateTripIdx;
        QVector<QString> tripsWithoutRoute;
        rTrips->getAllTripsWithPredictions(addedRouteTrips,
                                           activeRouteTrips,
                                           canceledRouteTrips,
                                           mismatchedTrips,
                                           duplicateTripIdx,
                                           tripsWithoutRoute);

        qint32 activeTripCount = 0;
        for (const QString &routeID : activeRouteTrips.keys()) {
            activeTripCount += activeRouteTrips[routeID].size();
        }
        resp["sch"] = activeTripCount;

        qint32 addedTripCount = 0;
        for (const QString &routeID : addedRouteTrips.keys()) {
            addedTripCount += addedRouteTrips[routeID].size();
        }
        resp["add"] = addedTripCount;

        qint32 canceledTripCount = 0;
        for (const QString &routeID : canceledRouteTrips.keys()) {
            canceledTripCount += canceledRouteTrips[routeID].size();
        }
        resp["can"] = canceledTripCount;

        qint32 duplicateTripCount = 0;
        for (const QString &routeID : duplicateTripIdx.keys()) {
            for (const QString &tripID : duplicateTripIdx[routeID].keys()) {
                duplicateTripCount += duplicateTripIdx[routeID][tripID].size();
            }
        }
        resp["dup"] = duplicateTripCount;

        qint32 mismatchedTripCount = 0;
        for (const QString &routeID : mismatchedTrips.keys()) {
            mismatchedTripCount += mismatchedTrips[routeID].size();
        }
        resp["mis"] = mismatchedTripCount;

        resp["nrt"] = tripsWithoutRoute.size();

        // Build an array of route ID with the trip statistics for each
        QHash<QString, routeRtStats> routesRealTime;
        for (const QString &routeID : canceledRouteTrips.keys()) {
            routesRealTime[routeID].canceled = canceledRouteTrips[routeID].size();
        }
        for (const QString &routeID : addedRouteTrips.keys()) {
            routesRealTime[routeID].added = addedRouteTrips[routeID].size();
        }
        for (const QString &routeID : activeRouteTrips.keys()) {
            routesRealTime[routeID].scheduled = activeRouteTrips[routeID].size();
        }
        for (const QString &routeID : duplicateTripIdx.keys()) {
            routesRealTime[routeID].duplicated = 0;
            for (const QString &tripID : duplicateTripIdx[routeID].keys()) {
                routesRealTime[routeID].duplicated += duplicateTripIdx[routeID][tripID].size();
            }
        }
        for (const QString &routeID : mismatchedTrips.keys()) {
            routesRealTime[routeID].mismatched = mismatchedTrips[routeID].size();
        }

        QJsonObject realTimeRouteJson;
        for (const QString &routeID : routesRealTime.keys()) {
            QJsonObject realTimeSpecific;
            realTimeSpecific["add"] = routesRealTime[routeID].added;
            realTimeSpecific["sch"] = routesRealTime[routeID].scheduled;
            realTimeSpecific["can"] = routesRealTime[routeID].canceled;
            realTimeSpecific["dup"] = routesRealTime[routeID].duplicated;
            realTimeSpecific["mis"] = routesRealTime[routeID].mismatched;
            realTimeRouteJson[routeID] = realTimeSpecific;
        }
        resp["routes"] = realTimeRouteJson;
    }

    // fill standard protocol information
    fillProtocolFields("RPS", 0, resp);
}



} // Namespace GTFS
