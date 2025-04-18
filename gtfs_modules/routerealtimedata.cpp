/*
 * GtfsProc_Server
 * Copyright (C) 2022-2025, Daniel Brook
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

#include "routerealtimedata.h"

#include "datagateway.h"

#include <QJsonArray>

namespace GTFS {

RouteRealtimeData::RouteRealtimeData(QList<QString> routeIDs)
    : StaticStatus(),
      _routeIDs(routeIDs),
      _rtData(false)
{
    // Realtime Data Determination
    RealTimeGateway::inst().realTimeTransactionHandled();
    _realTimeProc = GTFS::RealTimeGateway::inst().getActiveFeed();
    if (_realTimeProc != nullptr) {
        _rtData = true;
    }

    // GTFS Database Items
    _routes = GTFS::DataGateway::inst().getRoutesDB();
    _trips = GTFS::DataGateway::inst().getTripsDB();
    _stops = GTFS::DataGateway::inst().getStopsDB();
    _stopTimes = GTFS::DataGateway::inst().getStopTimesDB();

    // Notify real-time processor that real-time data is (still) being used
    RealTimeGateway::inst().realTimeTransactionHandled();
}

void RouteRealtimeData::fillResponseData(QJsonObject &resp)
{
    if (!_rtData) {
        // No realtime data loaded? This transaction is pointless
        fillProtocolFields("TRR", 801, resp);
        return;
    } else if (!allRoutesExistInFeed()) {
        // Is the requested route ID even available?
        fillProtocolFields("TRR", 802, resp);
        return;
    }

    QDateTime activeFeedTime = _realTimeProc->getFeedTime();
    if (activeFeedTime.isNull()) {
        resp["realtime_age_sec"] = "-";
        fillProtocolFields("TRR", 803, resp);
        return;
    } else {
        resp["realtime_age_sec"] = activeFeedTime.secsTo(getAgencyTime());
    }

    bool loosenStopSequence = _realTimeProc->getLoosenStopSeqEnf();

    // Loop over all requested routes and find the real time trip information
    QJsonArray realtimeRoutes;
    for (const QString &routeID : _routeIDs) {
        QJsonObject routeEntry;
        routeEntry["route_id"] = routeID;
        routeEntry["route_short_name"] = (*_routes)[routeID].route_short_name;
        routeEntry["route_long_name"] = (*_routes)[routeID].route_long_name;
        routeEntry["color"] = (*_routes)[routeID].route_color;
        routeEntry["text_color"] = (*_routes)[routeID].route_text_color;

        // Sort by trip ID (lexicographically) to make output more consistent between each update/call
        QVector<QString> tripIDsForRouteID;
        _realTimeProc->getActiveTripsForRouteID(routeID, tripIDsForRouteID);
        std::sort(tripIDsForRouteID.begin(), tripIDsForRouteID.end(), [] (auto &a, auto &b) { return a < b; });

        QJsonArray tripsForRoute;
        for (const QString &tripID : tripIDsForRouteID) {
            // There's no guarantee the real-time feed has the date filled at all. If this field is empty for the
            // real-time trip update, then 1) assume the current actual day, 2) do not print the day in arrive/depart.
            const QString rtTripStartDate = _realTimeProc->getTripStartDate(tripID);
            bool startDateMissing = false;
            QDate qRtTripStartDate;
            if (rtTripStartDate.isEmpty()) {
                qRtTripStartDate = getAgencyTime().date();
                startDateMissing = true;
            } else {
                qRtTripStartDate = QDate::fromString(rtTripStartDate, "yyyyMMdd");
            }
            QDateTime localNoon = QDateTime(qRtTripStartDate, QTime(12, 0, 0), getAgencyTime().timeZone());
            QJsonObject tripInfo;
            tripInfo["trip_id"] = tripID;
            tripInfo["rt_start_date"] = startDateMissing ? "-" : qRtTripStartDate.toString("dd-MMM-yyyy");
            tripInfo["vehicle"] = _realTimeProc->getOperatingVehicle(tripID);
            tripInfo["direction_id"] = QJsonValue::fromVariant(_realTimeProc->getDirectionId(tripID));

            // Static trip feed details (if possible)
            bool isSupplementalTrip = false;
            if (_trips->contains(tripID)) {
                tripInfo["headsign"] = (*_trips)[tripID].trip_headsign;
                tripInfo["short_name"] = (*_trips)[tripID].trip_short_name;
            } else {
                const QString finalStopIdSoFar = _realTimeProc->getFinalStopIdForAddedTrip(tripID);
                tripInfo["headsign"] = _stops->contains(finalStopIdSoFar) ? (*_stops)[finalStopIdSoFar].stop_name : "-";
                tripInfo["short_name"] = "*SPLM*";
                isSupplementalTrip = true;
            }

            // Load up / reconcile the static and real time data of the trip so we can get times
            QVector<rtStopTimeUpdate> stopTimesUTC;
            _realTimeProc->fillStopTimesForTrip(GTFS::TRIPID_RECONCILE,
                                                -1,  // Not needed due to use of TRIPID_RECONCILE
                                                tripID,
                                                getAgencyTime().timeZone(),
                                                qRtTripStartDate,
                                                (*_stopTimes)[tripID],
                                                stopTimesUTC);

            // Get the computed/actual stop time of the stopID for display
            tripInfo["skipped"] = false;
            tripInfo["arrive"] = "-";
            tripInfo["depart"] = "-";

            tripInfo["next_stop_id"] = "-";
            tripInfo["next_stop_name"] = "-";
            tripInfo["next_stop_parent"] = "-";

            for (const rtStopTimeUpdate &rtstu : stopTimesUTC) {
                // Move onto next stop time if it's already past the trip departure time at the matched stop
                if (!rtstu.depTime.isNull() && getAgencyTime() > rtstu.depTime) {
                    continue;
                } else if (rtstu.depTime.isNull() && !rtstu.arrTime.isNull() && getAgencyTime() > rtstu.arrTime.addSecs(30)) {
                    continue;
                } else if (rtstu.depTime.isNull() && rtstu.arrTime.isNull()) {
                    continue;
                }

                if (_stops->contains(rtstu.stopID)) {
                    tripInfo["next_stop_id"] = rtstu.stopID;
                    tripInfo["next_stop_name"] = (*_stops)[rtstu.stopID].stop_name;
                    tripInfo["next_stop_parent"] = (*_stops)[rtstu.stopID].parent_station;
                } else {
                    tripInfo["next_stop_id"] = rtstu.stopID;
                    tripInfo["next_stop_name"] = "StopID: " + rtstu.stopID;
                    tripInfo["next_stop_parent"] = "-";
                }

                // Trip/Stop Specifics - Pickup/DropOff Type - Optional (only for trips with a static schedule)
                quint64 staticFeedStopSequence = 0;
                qint16 dropOffType = -1;
                qint16 pickupType = -1;
                QString stopSpecificHeadsign;
                for (const StopTimeRec &stopTime : (*_stopTimes)[tripID]) {
                    if (loosenStopSequence
                        ? (stopTime.stop_id == rtstu.stopID)
                        : (stopTime.stop_sequence == rtstu.stopSequence)) {
                        dropOffType = stopTime.drop_off_type;
                        pickupType = stopTime.pickup_type;
                        staticFeedStopSequence = stopTime.stop_sequence;
                        if (!stopTime.stop_headsign.isEmpty()) {
                            // Stop-Specific Headsign
                            stopSpecificHeadsign = stopTime.stop_headsign;
                        }
                        break;
                    }
                }

                // If the stop sequence doesn't match and strict checking mode is on, this "match" fails.
                // Warning is expected per discussion in the GTFS::RealTimeTripUpate class.
                if (!loosenStopSequence && rtstu.stopSequence != staticFeedStopSequence && !isSupplementalTrip) {
                    continue;
                }

                // Only set the drop off, pickup, headsign if the matching algorithm doesn't skip this entry
                tripInfo["drop_off_type"] = dropOffType;
                tripInfo["pickup_type"] = pickupType;
                if (!stopSpecificHeadsign.isEmpty()) {
                    tripInfo["headsign"] = stopSpecificHeadsign;
                }
                // Type of Stop - Skipped, drop-off/pick-up type
                tripInfo["skipped"] = rtstu.stopSkipped;

                // Arrival/Departure Real-Time display
                QString timeFormat = (startDateMissing) ?
                                         ((getStatus()->format12h()) ? "h:mma" : "hh:mm") :
                                         ((getStatus()->format12h()) ? "ddd h:mma" : "ddd hh:mm");
                tripInfo["arrive"] = rtstu.arrTime.toTimeZone(getAgencyTime().timeZone()).toString(timeFormat);
                tripInfo["depart"] = rtstu.depTime.toTimeZone(getAgencyTime().timeZone()).toString(timeFormat);
                break;
            }
            tripsForRoute.push_back(tripInfo);
        }
        routeEntry["trips"] = tripsForRoute;
        // Add constructed item to the routes array
        realtimeRoutes.push_back(routeEntry);
    }

    resp["routes"] = realtimeRoutes;

    // Fill response time of whole transaction
    fillProtocolFields("TRR", 0, resp);
}

bool RouteRealtimeData::allRoutesExistInFeed()
{
    for (const QString &routeID : _routeIDs) {
        if (!_routes->contains(routeID)) {
            return false;
        }
    }
    return true;
}

}  // Namespace GTFS
