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

#include "upcomingstopservice.h"

#include <QJsonArray>
#include <QDebug>

static inline void swap(QJsonValueRef v1, QJsonValueRef v2)
{
    QJsonValue temp(v1);
    v1 = QJsonValue(v2);
    v2 = temp;
}

namespace GTFS {

UpcomingStopService::UpcomingStopService(QList<QString> stopIDs,
                                         qint32         futureMinutes,
                                         bool           nexCombFormat,
                                         bool           realtimeOnly)
    : StaticStatus   (),
      _stopIDs       (stopIDs),
      _serviceDate   (QDate::currentDate()),
      _futureMinutes (futureMinutes),
      _combinedFormat(nexCombFormat),
      _realtimeOnly  (realtimeOnly),
      _rtData        (false)
{
    // Realtime Data Determination
    RealTimeGateway::inst().realTimeTransactionHandled();
    _realTimeProc = GTFS::RealTimeGateway::inst().getActiveFeed();
    if (_realTimeProc != nullptr) {
        _rtData = true;
    }

    // Static Datasets for the TripStopReconciler
    _status    = GTFS::DataGateway::inst().getStatus();
    _service   = GTFS::DataGateway::inst().getServiceDB();
    _stops     = GTFS::DataGateway::inst().getStopsDB();
    _parentSta = GTFS::DataGateway::inst().getParentsDB();
    _routes    = GTFS::DataGateway::inst().getRoutesDB();
    _stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
    _tripDB    = GTFS::DataGateway::inst().getTripsDB();

    // Override the service date if in the fixed debugging date mode
    if (!_status->getOverrideDateTime().isNull()) {
        _serviceDate = _status->getOverrideDateTime().date();
    }
}

void UpcomingStopService::fillResponseData(QJsonObject &resp)
{
    // If a parent station was requested, fetch all the relevant trips to all its child stops
    bool parentStationMode = false;
    QString parentStation;
    if (_stopIDs.size() == 1 && _parentSta->contains(_stopIDs.at(0))) {
        parentStation = _stopIDs.at(0);
        _stopIDs = (*_parentSta)[parentStation].toList();
        parentStationMode = true;
    }

    // Array to populate based on the response of the tripStopLoader
    GTFS::TripStopReconciler tripStopLoader(_stopIDs,
                                            _rtData,
                                            _serviceDate,
                                            getAgencyTime(),
                                            _futureMinutes,
                                            _status,
                                            _service,
                                            _stops,
                                            _routes,
                                            _tripDB,
                                            _stopTimes,
                                            _realTimeProc);

    // The stop ID requested does not exist
    if (!tripStopLoader.stopIdExists()) {
        if (_combinedFormat) {
            fillProtocolFields("NCF", 601, resp);
        } else {
            fillProtocolFields("NEX", 601, resp);
        }
        return;
    }

    // Store dataset modification time
    resp["static_data_modif"] = _status->getStaticDatasetModifiedTime().toString("dd-MMM-yyyy hh:mm:ss t");

    // If real-time data is available (regardless of if it's relevant for this request or not), store the age of the
    // data in the active buffer used to produce real-time predictions in this transaction.
    if (_rtData) {
        QDateTime activeFeedTime = _realTimeProc->getFeedTime();
        if (activeFeedTime.isNull()) {
            resp["realtime_age_sec"] = "-";
        } else {
            resp["realtime_age_sec"] = activeFeedTime.secsTo(getAgencyTime());
        }
    }

    // Fill stop information in response
    if (parentStationMode) {
        resp["stop_id"] = parentStation;
        resp["stop_name"] = (*_stops)[parentStation].stop_name;
        resp["stop_desc"] = "Parent Station";
    } else {
        if (_stopIDs.size() == 1) {
            resp["stop_id"] = _stopIDs.at(0);
        } else {
            QString stopIDconcat;
            for (const QString &stopID : _stopIDs) {
                stopIDconcat += stopID + " | ";
            }
            resp["stop_id"] = stopIDconcat;
        }
        resp["stop_name"] = tripStopLoader.getStopName();
        resp["stop_desc"] = tripStopLoader.getStopDesciption();
    }

    // Populate the valid upcoming routes with trips for the stop_id requested
    QJsonArray  stopRouteArray;
    QHash<QString, GTFS::StopRecoRouteRec> tripsForStopByRouteID;
    tripStopLoader.getTripsByRoute(tripsForStopByRouteID);

    // DEFAULT MODE: "NEX" IS THE UPCOMING ARRIVALS GROUPED BY THE TRIP'S RESPECTIVE ROUTE ID
    if (!_combinedFormat) {
        for (const QString &routeID : tripsForStopByRouteID.keys()) {
            QJsonObject routeItem;
            routeItem["route_id"] = routeID;

            // Fetch the trips
            QJsonArray stopTrips;
            quint32    tripsFoundForRoute = 0;
            for (const GTFS::StopRecoTripRec &rts : tripsForStopByRouteID[routeID].tripRecos) {
                GTFS::TripRecStat tripStat = rts.tripStatus;
                if ((tripStat == GTFS::IRRELEVANT) || (_status->hideTerminatingTripsForNEXNCF() && rts.endOfTrip) ||
                    (_realtimeOnly && (tripStat == GTFS::SCHEDULE || tripStat == GTFS::NOSCHEDULE))) {
                    continue;
                }

                QJsonObject stopTripItem;
                fillTripData(rts, stopTripItem);
                stopTrips.push_back(stopTripItem);

                ++tripsFoundForRoute;
                if (tripsFoundForRoute == _status->getNbTripsPerRoute()) {
                    break;
                }
            }

            routeItem["trips"] = stopTrips;
            stopRouteArray.push_back(routeItem);
        }

        // Sort routes lexicographically by route ID ...
        // (agencies are wildly inconsistent about how route IDs match short/long names, so there's no ideal/normal way)
        std::sort(stopRouteArray.begin(), stopRouteArray.end(),
                  [](const QJsonValue &routeA, const QJsonValue &routeB) {
            return routeA.toObject()["route_id"].toString() < routeB.toObject()["route_id"].toString();
        });

        // Finally, attach the route list!
        resp["routes"] = stopRouteArray;
    }

    // COMBINED-FORMAT MODE: "NCF" IS THE UPCOMING ARRIVALS IN ONE LINEAR ARRAY, SORTED BY ARRIVAL/DEPARTURE TIME
    else {
        QVector<QPair<GTFS::StopRecoTripRec, QString>> unifiedTrips;  // A trip and its associated routeID (QString)

        for (const QString &routeID : tripsForStopByRouteID.keys()) {
            // Flatten trips into a single array (as opposed to the nesting by route present) to sorted times together
            for (const GTFS::StopRecoTripRec &rts : tripsForStopByRouteID[routeID].tripRecos) {
                GTFS::TripRecStat tripStat = rts.tripStatus;
                if ((tripStat == GTFS::IRRELEVANT) || (_status->hideTerminatingTripsForNEXNCF() && rts.endOfTrip) ||
                    (_realtimeOnly && (tripStat == GTFS::SCHEDULE || tripStat == GTFS::NOSCHEDULE))) {
                    continue;
                }
                unifiedTrips.push_back(qMakePair(rts, routeID));
            }
        }

        // Sort the unified trips array in order of wait time
        std::sort(unifiedTrips.begin(), unifiedTrips.end(),
                  [](const QPair<GTFS::StopRecoTripRec, QString> &v1, const QPair<GTFS::StopRecoTripRec, QString> &v2) {
            return v1.first.waitTimeSec < v2.first.waitTimeSec;
        });

        // Place into JSON style output now that it is sorted together by wait time
        for (const QPair<GTFS::StopRecoTripRec, QString> &rts : unifiedTrips) {
            QJsonObject stopTripItem;
            stopTripItem["route_id"] = rts.second;    // Link to the route information
            fillTripData(rts.first, stopTripItem);
            stopRouteArray.push_back(stopTripItem);
        }

        // Attach the routes and trips collections
        resp["trips"]  = stopRouteArray;
    }

    // Fill standard protocol-required information ... the grammars are different if using combined format vs. standard!
    if (_combinedFormat) {
        fillProtocolFields("NCF", 0, resp);
    } else {
        fillProtocolFields("NEX", 0, resp);
    }
}

void UpcomingStopService::fillTripData(const StopRecoTripRec &rts, QJsonObject &stopTripItem)
{
    GTFS::TripRecStat tripStat = rts.tripStatus;

    stopTripItem["trip_id"]         = rts.tripID;
    stopTripItem["short_name"]      = (*_tripDB)[rts.tripID].trip_short_name;
    stopTripItem["wait_time_sec"]   = rts.waitTimeSec;
    stopTripItem["headsign"]        = rts.headsign;
    stopTripItem["pickup_type"]     = rts.pickupType;
    stopTripItem["drop_off_type"]   = rts.dropoffType;
    stopTripItem["trip_begins"]     = rts.beginningOfTrip;
    stopTripItem["trip_terminates"] = rts.endOfTrip;

    if (getStatus()->format12h()) {
        stopTripItem["dep_time"]        = rts.schDepTime.isNull() ? "-" : rts.schDepTime.toString("ddd h:mma");
        stopTripItem["arr_time"]        = rts.schArrTime.isNull() ? "-" : rts.schArrTime.toString("ddd h:mma");
    } else {
        stopTripItem["dep_time"]        = rts.schDepTime.isNull() ? "-" : rts.schDepTime.toString("ddd hh:mm");
        stopTripItem["arr_time"]        = rts.schArrTime.isNull() ? "-" : rts.schArrTime.toString("ddd hh:mm");
    }

    if (rts.realTimeDataAvail) {
        QJsonObject realTimeData;
        QString statusString;
        if (tripStat == GTFS::ARRIVE)
            statusString = "ARRV";
        else if (tripStat == GTFS::BOARD)
            statusString = "BRDG";
        else if (tripStat == GTFS::DEPART)
            statusString = "DPRT";
        else if (tripStat == GTFS::RUNNING)
            statusString = "RNNG";
        else if (tripStat == GTFS::SKIP)
            statusString = "SKIP";
        else if (tripStat == GTFS::CANCEL)
            statusString = "CNCL";

        realTimeData["status"]         = statusString;
        realTimeData["stop_status"]    = rts.stopStatus;
        realTimeData["offset_seconds"] = rts.realTimeOffsetSec;
        realTimeData["vehicle"]        = rts.vehicleRealTime;

        if (getStatus()->format12h()) {
            realTimeData["actual_arrival"]   = rts.realTimeArrival.toString("ddd h:mma");
            realTimeData["actual_departure"] = rts.realTimeDeparture.toString("ddd h:mma");
        } else {
            realTimeData["actual_arrival"]   = rts.realTimeArrival.toString("ddd hh:mm");
            realTimeData["actual_departure"] = rts.realTimeDeparture.toString("ddd hh:mm");
        }

        stopTripItem["realtime_data"] = realTimeData;
    }
}

}  // Namespace GTFS
