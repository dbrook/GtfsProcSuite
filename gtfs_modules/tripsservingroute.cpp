/*
 * GtfsProc_Server
 * Copyright (C) 2018-2024, Daniel Brook
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

#include "tripsservingroute.h"

#include "datagateway.h"

#include <QJsonArray>

namespace GTFS {

TripsServingRoute::TripsServingRoute(const QString &routeID, const QDate &onlyDate)
    : StaticStatus(), _routeID(routeID), _onlyDate(onlyDate)
{
    _routes      = GTFS::DataGateway::inst().getRoutesDB();
    _tripDB      = GTFS::DataGateway::inst().getTripsDB();
    _stopTimesDB = GTFS::DataGateway::inst().getStopTimesDB();
    _stopDB      = GTFS::DataGateway::inst().getStopsDB();
    _svc         = GTFS::DataGateway::inst().getServiceDB();
}

void TripsServingRoute::fillResponseData(QJsonObject &resp)
{
    if (!_routes->contains(_routeID)) {
        fillProtocolFields("TSR", 201, resp);
        return;
    }

    resp["route_id"]         = _routeID;
    resp["route_short_name"] = (*_routes)[_routeID].route_short_name;
    resp["route_long_name"]  = (*_routes)[_routeID].route_long_name;
    resp["service_date"]     = _onlyDate.toString("ddd dd-MMM-yyyy");
    resp["route_color"]      = (*_routes)[_routeID].route_color;
    resp["route_text_color"] = (*_routes)[_routeID].route_text_color;

    QJsonArray routeTripArray;
    for (const QPair<QString, qint32> &tripIDwTime : (*_routes)[_routeID].trips) {
        // Loop on each route that serves the stop
        QString tripID    = tripIDwTime.first;
        QString serviceID = (*_tripDB)[tripID].service_id;

        // If only a certain day is requested, check if the service is actually running before appending a trip
        if (!_onlyDate.isNull() && !_svc->serviceRunning(_onlyDate, (*_tripDB)[tripID].service_id)) {
            continue;
        }

        QJsonObject singleStopJSON;
        singleStopJSON["trip_id"]                = tripID;
        singleStopJSON["headsign"]               = (*_tripDB)[tripID].trip_headsign;
        singleStopJSON["service_id"]             = serviceID;
        singleStopJSON["svc_start_date"]         = _svc->getServiceStartDate(serviceID).toString("ddMMMyyyy");
        singleStopJSON["svc_end_date"]           = _svc->getServiceEndDate(serviceID).toString("ddMMMyyyy");
        singleStopJSON["operate_days_condensed"] = _svc->shortSerializeOpDays(serviceID);
        singleStopJSON["supplements_other_days"] = _svc->serviceAddedOnOtherDates(serviceID);
        singleStopJSON["exceptions_present"]     = _svc->serviceRemovedOnDates(serviceID);

        // More flexible operating day information
        bool mon, tue, wed, thu, fri, sat, sun = false;
        _svc->booleanOpDays(serviceID, mon, tue, wed, thu, fri, sat, sun);
        singleStopJSON["op_mon"] = mon;
        singleStopJSON["op_tue"] = tue;
        singleStopJSON["op_wed"] = wed;
        singleStopJSON["op_thu"] = thu;
        singleStopJSON["op_fri"] = fri;
        singleStopJSON["op_sat"] = sat;
        singleStopJSON["op_sun"] = sun;

        // First Departure of Trip
        if (_onlyDate.isNull()) {
            QTime localNoon    = QTime(12, 0, 0);
            QTime firstStopDep = localNoon.addSecs(tripIDwTime.second);
            if (getStatus()->format12h()) {
                singleStopJSON["first_stop_departure"] = firstStopDep.toString("h:mma");
            } else {
                singleStopJSON["first_stop_departure"] = firstStopDep.toString("hh:mm");
            }
            singleStopJSON["first_stop_next_day"] = OperatingDay::isNextActualDay(tripIDwTime.second);
        } else {
            QDateTime localNoon(_onlyDate, QTime(12, 0, 0), getAgencyTime().timeZone());
            QDateTime firstStopDep = localNoon.addSecs(tripIDwTime.second);
            if (getStatus()->format12h()) {
                singleStopJSON["first_stop_departure"] = firstStopDep.toString("h:mma");
            } else {
                singleStopJSON["first_stop_departure"] = firstStopDep.toString("hh:mm");
            }
            singleStopJSON["first_stop_dst_on"]   = firstStopDep.isDaylightTime();
            singleStopJSON["first_stop_next_day"] = OperatingDay::isNextActualDay(tripIDwTime.second);
        }

        // First StopID of Trip (trips do not always begin at a terminal and just relying on headsign is thus evil!)
        const GTFS::StopTimeRec &firstStop = (*_stopTimesDB)[tripID].first();
        singleStopJSON["first_stop_id"]    = firstStop.stop_id;
        singleStopJSON["first_stop_name"]  = (*_stopDB)[firstStop.stop_id].stop_name;

        // Last Arrival of Trip
        const GTFS::StopTimeRec &lastStop = (*_stopTimesDB)[tripID].last();
        if (_onlyDate.isNull()) {
            QTime localNoon   = QTime(12, 0, 0);
            QTime lastStopArr = localNoon.addSecs(lastStop.arrival_time);
            if (getStatus()->format12h()) {
                singleStopJSON["last_stop_arrival"] = lastStopArr.toString("h:mma");
            } else {
                singleStopJSON["last_stop_arrival"] = lastStopArr.toString("hh:mm");
            }
            singleStopJSON["last_stop_next_day"] = OperatingDay::isNextActualDay(lastStop.arrival_time);
        } else {
            QDateTime localNoon(_onlyDate, QTime(12, 0, 0), getAgencyTime().timeZone());
            QDateTime lastStopArr = localNoon.addSecs(lastStop.arrival_time);
            if (getStatus()->format12h()) {
                singleStopJSON["last_stop_arrival"] = lastStopArr.toString("h:mma");
            } else {
                singleStopJSON["last_stop_arrival"] = lastStopArr.toString("hh:mm");
            }
            singleStopJSON["last_stop_dst_on"]   = lastStopArr.isDaylightTime();
            singleStopJSON["last_stop_next_day"] = OperatingDay::isNextActualDay(lastStop.arrival_time);
        }

        // Last StopID of Trip
        singleStopJSON["last_stop_id"]   = lastStop.stop_id;
        singleStopJSON["last_stop_name"] = (*_stopDB)[lastStop.stop_id].stop_name;

        routeTripArray.push_back(singleStopJSON);
    }

    resp["trips"] = routeTripArray;

    // Successfully filled all schedule information for trip! Populate core protocol fields
    fillProtocolFields("TSR", 0, resp);
}

}  // Namespace GTFS
