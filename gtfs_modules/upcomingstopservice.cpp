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

#include "tripstopreconciler.h"   // The ugly trip-stop reconciliation "helper" code      (TODO: see about reorganizing)

#include <QJsonArray>
#include <QDebug>

// Ridiculous sort function for the NCF module
static inline void swap(QJsonValueRef left, QJsonValueRef right)
{
    // This seems horrible!
    QJsonValue temp(left);
    left  = QJsonValue(right);
    right = temp;
}


namespace GTFS {

UpcomingStopService::UpcomingStopService(QString stopID,
                                         qint32  futureMinutes,
                                         qint32  maxTripsPerRoute,
                                         bool    nexCombFormat,
                                         bool    realtimeOnly)
    : StaticStatus      (),
      _stopID           (stopID),
      _serviceDate      (QDate::currentDate()),
      _futureMinutes    (futureMinutes),
      _maxTripsPerRoute (maxTripsPerRoute),
      _combinedFormat   (nexCombFormat),
      _realtimeOnly     (realtimeOnly),
      _rtData           (false)
{
    // Realtime Data Determination
    _realTimeProc = GTFS::RealTimeGateway::inst().getActiveFeed();
    if (_realTimeProc != nullptr) {
        _rtData = true;
    }

    // Static Datasets for the TripStopReconciler
    _status    = GTFS::DataGateway::inst().getStatus();
    _service   = GTFS::DataGateway::inst().getServiceDB();
    _stops     = GTFS::DataGateway::inst().getStopsDB();
    _routes    = GTFS::DataGateway::inst().getRoutesDB();
    _stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
    _tripDB    = GTFS::DataGateway::inst().getTripsDB();

}

void UpcomingStopService::fillResponseData(QJsonObject &resp)
{
    // Array to populate based on the response of the tripStopLoader
    GTFS::TripStopReconciler tripStopLoader(_stopID,
                                            _rtData,
                                            _serviceDate,
                                            getAgencyTime(),
                                            _futureMinutes,
                                            _maxTripsPerRoute,
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

    // Fill stop information in response
    resp["stop_id"]   = _stopID;
    resp["stop_name"] = tripStopLoader.getStopName();
    resp["stop_desc"] = tripStopLoader.getStopDesciption();

    // Populate the valid upcoming routes with trips for the stop_id requested
    QJsonArray  stopRouteArray;
    QMap<QString, GTFS::StopRecoRouteRec> tripsForStopByRouteID;
    tripStopLoader.getTripsByRoute(tripsForStopByRouteID);

    // DEFAULT MODE: "NEX" IS THE UPCOMING ARRIVALS GROUPED BY THE TRIP'S RESPECTIVE ROUTE ID
    if (!_combinedFormat) {
        for (const QString &routeID : tripsForStopByRouteID.keys()) {
            QJsonObject singleRouteJSON;
            singleRouteJSON["route_id"]         = routeID;
            singleRouteJSON["route_short_name"] = tripsForStopByRouteID[routeID].shortRouteName;
            singleRouteJSON["route_long_name"]  = tripsForStopByRouteID[routeID].longRouteName;
            singleRouteJSON["route_color"]      = tripsForStopByRouteID[routeID].routeColor;
            singleRouteJSON["route_text_color"] = tripsForStopByRouteID[routeID].routeTextColor;

            // Fetch the trips
            QJsonArray stopTrips;
            for (const GTFS::StopRecoTripRec &rts : tripsForStopByRouteID[routeID].tripRecos) {
                GTFS::TripRecStat tripStat = rts.tripStatus;

                if (tripStat == GTFS::IRRELEVANT)
                    continue;

                if (_realtimeOnly && (tripStat == GTFS::SCHEDULE || tripStat == GTFS::NOSCHEDULE))
                    continue;

                QJsonObject stopTripItem;
                stopTripItem["trip_id"]         = rts.tripID;
                stopTripItem["short_name"]      = (*_tripDB)[rts.tripID].trip_short_name;
                stopTripItem["dep_time"]        = rts.schDepTime.isNull() ? "-" : rts.schDepTime.toString("ddd hh:mm");
                stopTripItem["arr_time"]        = rts.schArrTime.isNull() ? "-" : rts.schArrTime.toString("ddd hh:mm");
                stopTripItem["wait_time_sec"]   = rts.waitTimeSec;
                stopTripItem["headsign"]        = rts.headsign;
                stopTripItem["pickup_type"]     = rts.pickupType;
                stopTripItem["drop_off_type"]   = rts.dropoffType;
                stopTripItem["trip_begins"]     = rts.beginningOfTrip;
                stopTripItem["trip_terminates"] = rts.endOfTrip;

                if (rts.realTimeDataAvail) {
                    QJsonObject realTimeData;
                    QString statusString;
                    if (tripStat == GTFS::ARRIVE)
                        statusString = "ARRV";
                    else if (tripStat == GTFS::BOARD)
                        statusString = "BRDG";
                    else if (tripStat == GTFS::DEPART)
                        statusString = "DPRT";
                    else if (tripStat == GTFS::ON_TIME)
                        statusString = "ONTM";
                    else if (tripStat == GTFS::LATE)
                        statusString = "LATE";
                    else if (tripStat == GTFS::EARLY)
                        statusString = "ERLY";
                    else if (tripStat == GTFS::SUPPLEMENT)
                        statusString = "SPLM";
                    else if (tripStat == GTFS::SKIP)
                        statusString = "SKIP";
                    else if (tripStat == GTFS::CANCEL)
                        statusString = "CNCL";
                    else if (tripStat == GTFS::MISSING)
                        statusString = "MSNG";

                    realTimeData["status"]           = statusString;
                    realTimeData["actual_arrival"]   = rts.realTimeArrival.toString("ddd hh:mm");
                    realTimeData["actual_departure"] = rts.realTimeDeparture.toString("ddd hh:mm");
                    realTimeData["offset_seconds"]   = rts.realTimeOffsetSec;
                    realTimeData["vehicle"]          = rts.vehicleRealTime;
                    stopTripItem["realtime_data"]    = realTimeData;
                }

                stopTrips.push_back(stopTripItem);
            }

            // Sort trips by arrival time:
            std::sort(stopTrips.begin(), stopTrips.end(), [](const QJsonValue &v1, const QJsonValue &v2) {
                return v1["wait_time_sec"].toInt() < v2["wait_time_sec"].toInt();
            });

            singleRouteJSON["trips"] = stopTrips;
            stopRouteArray.push_back(singleRouteJSON);
        }
    }

    // COMBINED-FORMAT MODE: "NCF" IS THE UPCOMING ARRIVALS IN ONE LINEAR ARRAY, SORTED BY ARRIVAL/DEPARTURE TIME
    else {
        for (const QString &routeID : tripsForStopByRouteID.keys()) {
            QJsonArray stopTrips;
            for (const GTFS::StopRecoTripRec &rts : tripsForStopByRouteID[routeID].tripRecos) {
                GTFS::TripRecStat tripStat = rts.tripStatus;
                if ((tripStat == GTFS::IRRELEVANT) ||
                    (_realtimeOnly && (tripStat == GTFS::SCHEDULE || tripStat == GTFS::NOSCHEDULE)))
                    continue;

                QJsonObject stopTripJSON;
                stopTripJSON["route_id"]         = routeID;
                stopTripJSON["route_short_name"] = tripsForStopByRouteID[routeID].shortRouteName;
                stopTripJSON["route_long_name"]  = tripsForStopByRouteID[routeID].longRouteName;
                stopTripJSON["route_color"]      = tripsForStopByRouteID[routeID].routeColor;
                stopTripJSON["route_text_color"] = tripsForStopByRouteID[routeID].routeTextColor;
                stopTripJSON["trip_id"]         = rts.tripID;
                stopTripJSON["short_name"]      = (*_tripDB)[rts.tripID].trip_short_name;
                stopTripJSON["dep_time"]        = rts.schDepTime.isNull() ? "-" : rts.schDepTime.toString("ddd hh:mm");
                stopTripJSON["arr_time"]        = rts.schArrTime.isNull() ? "-" : rts.schArrTime.toString("ddd hh:mm");
                stopTripJSON["wait_time_sec"]   = rts.waitTimeSec;
                stopTripJSON["headsign"]        = rts.headsign;
                stopTripJSON["pickup_type"]     = rts.pickupType;
                stopTripJSON["drop_off_type"]   = rts.dropoffType;
                stopTripJSON["trip_begins"]     = rts.beginningOfTrip;
                stopTripJSON["trip_terminates"] = rts.endOfTrip;

                if (rts.realTimeDataAvail) {
                    QJsonObject realTimeData;
                    QString statusString;
                    if (tripStat == GTFS::ARRIVE)
                        statusString = "ARRV";
                    else if (tripStat == GTFS::BOARD)
                        statusString = "BRDG";
                    else if (tripStat == GTFS::DEPART)
                        statusString = "DPRT";
                    else if (tripStat == GTFS::ON_TIME)
                        statusString = "ONTM";
                    else if (tripStat == GTFS::LATE)
                        statusString = "LATE";
                    else if (tripStat == GTFS::EARLY)
                        statusString = "ERLY";
                    else if (tripStat == GTFS::SUPPLEMENT)
                        statusString = "SPLM";
                    else if (tripStat == GTFS::SKIP)
                        statusString = "SKIP";
                    else if (tripStat == GTFS::CANCEL)
                        statusString = "CNCL";
                    else if (tripStat == GTFS::MISSING)
                        statusString = "MSNG";

                    realTimeData["status"]           = statusString;
                    realTimeData["actual_arrival"]   = rts.realTimeArrival.toString("ddd hh:mm");
                    realTimeData["actual_departure"] = rts.realTimeDeparture.toString("ddd hh:mm");
                    realTimeData["offset_seconds"]   = rts.realTimeOffsetSec;
                    realTimeData["vehicle"]          = rts.vehicleRealTime;
                    stopTripJSON["realtime_data"]    = realTimeData;
                }

                stopRouteArray.push_back(stopTripJSON);
            }
        }

        // Display in order of the wait time
        std::sort(stopRouteArray.begin(), stopRouteArray.end(), [](const QJsonValue &v1, const QJsonValue &v2) {
            return v1["wait_time_sec"].toInt() < v2["wait_time_sec"].toInt();
        });
    }

    // Finally, attach the route list!
    resp["routes"] = stopRouteArray;

    // Fill standard protocol-required information ... the grammars are different if using combined format vs. standard!
    if (_combinedFormat) {
        fillProtocolFields("NCF", 0, resp);
    } else {
        fillProtocolFields("NEX", 0, resp);
    }
}

}  // Namespace GTFS
