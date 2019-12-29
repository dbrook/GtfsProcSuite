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

#include "tripscheduledisplay.h"

#include "datagateway.h"

#include <QJsonArray>
#include <QDebug>

namespace GTFS {

TripScheduleDisplay::TripScheduleDisplay(const QString &tripID, bool useRealTimeData)
    : StaticStatus(), _tripID(tripID), _realTimeData(false)
{
    _tripDB    = GTFS::DataGateway::inst().getTripsDB();
    _svc       = GTFS::DataGateway::inst().getServiceDB();
    _routes    = GTFS::DataGateway::inst().getRoutesDB();
    _stops     = GTFS::DataGateway::inst().getStopsDB();
    _stopTimes = GTFS::DataGateway::inst().getStopTimesDB();

    // Real-time data gateway if it is requested and the data feed is alive
    if (useRealTimeData) {
        _realTimeProc = GTFS::RealTimeGateway::inst().getActiveFeed();
        if (_realTimeProc != nullptr) {
            _realTimeData = true;
        }
    }
}

void TripScheduleDisplay::fillResponseData(QJsonObject &resp)
{
    /************************************** Request for static trip ID schedule ***************************************/
    if (!_realTimeData) {
        // The trip ID doesn't exist in the static database -- ERROR CODE 101
        if (!_tripDB->contains(_tripID)) {
            fillProtocolFields("TRI", 101, resp);
            return;
        }

        resp["real_time"]        = false;

        // The Trip ID actually exists so fill the JSON
        resp["route_id"]         = (*_tripDB)[_tripID].route_id;
        resp["trip_id"]          = _tripID;
        resp["headsign"]         = (*_tripDB)[_tripID].trip_headsign;
        resp["short_name"]       = (*_tripDB)[_tripID].trip_short_name;
        resp["service_id"]       = (*_tripDB)[_tripID].service_id;
        resp["operate_days"]     = _svc->serializeOpDays((*_tripDB)[_tripID].service_id);

        // Provide the associated route name / details to the response
        resp["route_short_name"] = (*_routes)[(*_tripDB)[_tripID].route_id].route_short_name;
        resp["route_long_name"]  = (*_routes)[(*_tripDB)[_tripID].route_id].route_long_name;

        // String together all the dates in range for which the trip_id requested SPECIFICALLY does not operate
        resp["exception_dates"]  = _svc->serializeNoServiceDates((*_tripDB)[_tripID].service_id);

        // String together all the dates in range for which the trip_id requested operates additionally
        resp["added_dates"]      = _svc->serializeAddedServiceDates((*_tripDB)[_tripID].service_id);

        // Provide the validity range
        resp["svc_start_date"]   = _svc->getServiceStartDate((*_tripDB)[_tripID].service_id).toString("dd-MMM-yyyy");
        resp["svc_end_date"]     = _svc->getServiceEndDate((*_tripDB)[_tripID].service_id).toString("dd-MMM-yyyy");

        /*
         * Iterate through stops that the trip services and build an array for output
         * The times in the static feed are based off of local noon (this was accounted for at the backend load)
         */
        const QVector<GTFS::StopTimeRec> &tripStops = (*_stopTimes)[_tripID];
        QJsonArray tripStopArray;

        for (const GTFS::StopTimeRec &stop : tripStops) {
            QJsonObject singleStopJSON;
            QTime localNoon(12, 0, 0);

            if (stop.arrival_time != -1) {
                QTime arrivalTime = localNoon.addSecs(stop.arrival_time);
                singleStopJSON["arr_time"] = arrivalTime.toString("hh:mm");
            } else {
                singleStopJSON["arr_time"] = "-";
            }

            if (stop.departure_time != -1) {
                QTime departureTime = localNoon.addSecs(stop.departure_time);
                singleStopJSON["dep_time"]  = departureTime.toString("hh:mm");
            } else {
                singleStopJSON["dep_time"]  = "-";
            }

            singleStopJSON["sequence"]      = stop.stop_sequence;
            singleStopJSON["stop_id"]       = stop.stop_id;
            singleStopJSON["stop_name"]     = (*_stops)[stop.stop_id].stop_name;
            singleStopJSON["drop_off_type"] = stop.drop_off_type;
            singleStopJSON["pickup_type"]   = stop.pickup_type;
            tripStopArray.push_back(singleStopJSON);
        }
        resp["stops"] = tripStopArray;
    }

    /*********************************** Request for real-time trip ID predictions *************************************/
    else {
        // There is no real-time update for the trip ID requested -- ERROR CODE 102
        if (!_realTimeProc->tripExists(_tripID)) {
            fillProtocolFields("TRI", 102, resp);
            return;
        }

        // Prepare list of trip's stops and times for dealing with version 1 realtime feeds
        bool rtVersion1 = _realTimeProc->isRTVersion1();
        QVector<GTFS::rtStopTimeUpdate> staticStopTimes;
        if (rtVersion1 && _tripDB->contains(_tripID)) {
            const QVector<GTFS::StopTimeRec> &tripStops = (*_stopTimes)[_tripID];
            for (const GTFS::StopTimeRec &str : tripStops) {
                QTime localNoon(12, 0, 0);
                QDateTime todayNoon(QDate::currentDate(), localNoon);

                rtStopTimeUpdate staticStopUpdate;

                staticStopUpdate.stopID       = str.stop_id;
                staticStopUpdate.stopSequence = str.stop_sequence;

                if (str.arrival_time != -1) {
                    staticStopUpdate.arrTime = todayNoon.addSecs(str.arrival_time).toUTC();
                } else {
                    staticStopUpdate.arrTime = QDateTime();
                }
                if (str.departure_time != -1) {
                    staticStopUpdate.depTime = todayNoon.addSecs(str.departure_time).toUTC();
                } else {
                    staticStopUpdate.depTime = QDateTime();
                }

                staticStopTimes.push_back(staticStopUpdate);
            }
        }

        QJsonArray tripStopArray;

        // We will populate directly from the real-time feed so it will have somewhat fewer details than the static one
        // The other big difference from the static feed is we have to use date-times (since we get literal seconds
        // since the UNIX epoch) as the type from the GTFS-RealTime feed)
        QString route_id;
        QVector<GTFS::rtStopTimeUpdate> stopTimes;
        _realTimeProc->fillStopTimesForTrip(_tripID, route_id, staticStopTimes, stopTimes);

        // In Version 1 feeds, we have to look for the route ID from the static feed as well
        if (rtVersion1) {
            route_id = (*_tripDB)[_tripID].route_id;
        }

        resp["real_time"] = true;

        // Data information
        resp["real_time_data_time"] =
                 _realTimeProc->getFeedTime().toTimeZone(getAgencyTime().timeZone()).toString("dd-MMM-yyyy hh:mm:ss t");

        // Fill in some details (route specifically ... everything else should be added if I feel like it is useful)
        resp["route_id"]         = route_id;
        resp["trip_id"]          = _tripID;
        resp["short_name"]       = (*_tripDB)[_tripID].trip_short_name;
        resp["route_short_name"] = (*_routes)[route_id].route_short_name;
        resp["route_long_name"]  = (*_routes)[route_id].route_long_name;
        resp["vehicle"]          = _realTimeProc->getOperatingVehicle(_tripID);

        for (const GTFS::rtStopTimeUpdate &rtsu : stopTimes) {
            QJsonObject singleStopJSON;
            singleStopJSON["arr_time"]  = (rtsu.arrTime.isNull()) ?
                                            "-" : rtsu.arrTime.toTimeZone(getAgencyTime().timeZone()).toString("hh:mm");
            singleStopJSON["dep_time"]  = (rtsu.depTime.isNull()) ?
                                            "-" : rtsu.depTime.toTimeZone(getAgencyTime().timeZone()).toString("hh:mm");
            singleStopJSON["stop_id"]   = rtsu.stopID;
            singleStopJSON["stop_name"] = (*_stops)[rtsu.stopID].stop_name;
            singleStopJSON["sequence"]  = rtsu.stopSequence;
            singleStopJSON["skipped"]   = rtsu.stopSkipped;
            tripStopArray.push_back(singleStopJSON);
        }

        resp["stops"] = tripStopArray;
    }

    // Successfully filled all schedule information for trip! Populate core protocol fields
    fillProtocolFields("TRI", 0, resp);
}

}  // Namespace GTFS