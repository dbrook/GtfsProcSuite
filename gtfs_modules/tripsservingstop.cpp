/*
 * GtfsProc_Server
 * Copyright (C) 2018-2023, Daniel Brook
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

#include "tripsservingstop.h"

#include "datagateway.h"

#include <QJsonArray>

namespace GTFS {

TripsServingStop::TripsServingStop(const QString &stopID, const QDate serviceDay)
    : StaticStatus(), _stopID(stopID), _onlyDate(serviceDay)
{
    _stops     = GTFS::DataGateway::inst().getStopsDB();
    _tripDB    = GTFS::DataGateway::inst().getTripsDB();
    _svc       = GTFS::DataGateway::inst().getServiceDB();
    _stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
    _routes    = GTFS::DataGateway::inst().getRoutesDB();
}

void TripsServingStop::fillResponseData(QJsonObject &resp)
{
    if (!_stops->contains(_stopID)) {
        fillProtocolFields("TSS", 301, resp);
        return;
    }

    resp["stop_id"]      = _stopID;
    resp["stop_name"]    = (*_stops)[_stopID].stop_name;
    resp["stop_desc"]    = (*_stops)[_stopID].stop_desc;
    resp["parent_sta"]   = (*_stops)[_stopID].parent_station;
    resp["service_date"] = _onlyDate.toString("ddd dd-MMM-yyyy");

    QJsonArray stopRouteArray;
    for (const QString &routeID : (*_stops)[_stopID].stopTripsRoutes.keys()) {
        QJsonObject singleRouteJSON;
        QJsonArray routeTripArray;

        singleRouteJSON["route_id"]         = routeID;
        singleRouteJSON["route_short_name"] = (*_routes)[routeID].route_short_name;
        singleRouteJSON["route_long_name"]  = (*_routes)[routeID].route_long_name;
        singleRouteJSON["route_color"]      = (*_routes)[routeID].route_color;
        singleRouteJSON["route_text_color"] = (*_routes)[routeID].route_text_color;

        for (qint32 tripLoopIdx = 0;
             tripLoopIdx < (*_stops)[_stopID].stopTripsRoutes[routeID].length();
             ++tripLoopIdx) {
            GTFS::tripStopSeqInfo tssi = (*_stops)[_stopID].stopTripsRoutes[routeID].at(tripLoopIdx);
            QString tripID      = tssi.tripID;
            qint32  stopTripIdx = tssi.tripStopIndex;

            // If only a certain day is requested, check if the service is actually running before appending a trip
            if (!_onlyDate.isNull() && !_svc->serviceRunning(_onlyDate, (*_tripDB)[tripID].service_id)) {
                continue;
            }

            QJsonObject tripDetails;
            fillUnifiedTripDetailsForArray(tripID, stopTripIdx, _svc, _stopTimes, _tripDB, _onlyDate, false,
                                           getAgencyTime(), tripDetails);

            routeTripArray.push_back(tripDetails);
        }

        // Add the individual route's worth of trips to the main array
        singleRouteJSON["trips"] = routeTripArray;

        // Add the whole route into the main return message
        stopRouteArray.push_back(singleRouteJSON);
    }

    resp["routes"] = stopRouteArray;

    // Successfully filled all schedule information for stop! Populate core protocol fields
    fillProtocolFields("TSS", 0, resp);
}

void TripsServingStop::fillUnifiedTripDetailsForArray(const QString            &tripID,
                                                      qint32                    stopTripIdx,
                                                      const GTFS::OperatingDay *svc,
                                                      const GTFS::StopTimeData *stopTimes,
                                                      const GTFS::TripData     *tripDB,
                                                      const QDate              &serviceDate,
                                                      bool                      skipServiceDetail,
                                                      const QDateTime          &currAgency,
                                                      QJsonObject              &singleStopJSON)
{
    QString serviceID = (*tripDB)[tripID].service_id;
    singleStopJSON["trip_id"]       = tripID;
    singleStopJSON["headsign"]      = ((*stopTimes)[tripID].at(stopTripIdx).stop_headsign != "")
                                         ? (*stopTimes)[tripID].at(stopTripIdx).stop_headsign
                                         : (*tripDB)[tripID].trip_headsign;
    singleStopJSON["short_name"]    = (*tripDB)[tripID].trip_short_name;
    singleStopJSON["drop_off_type"] = (*stopTimes)[tripID].at(stopTripIdx).drop_off_type;
    singleStopJSON["pickup_type"]   = (*stopTimes)[tripID].at(stopTripIdx).pickup_type;
    singleStopJSON["interp"]        = (*stopTimes)[tripID].at(stopTripIdx).interpolated;

    // The service (applicable days, dates, exceptions, additions) details of each trip are not very interesting
    // on transactions where a specific time is requested, so we provide the option to NOT bother sending it in the
    // interest of saving data transport needs
    if (!skipServiceDetail) {
        singleStopJSON["service_id"]             = serviceID;
        singleStopJSON["svc_start_date"]         = svc->getServiceStartDate(serviceID).toString("ddMMMyyyy");
        singleStopJSON["svc_end_date"]           = svc->getServiceEndDate(serviceID).toString("ddMMMyyyy");
        singleStopJSON["operate_days_condensed"] = svc->shortSerializeOpDays(serviceID);
        singleStopJSON["supplements_other_days"] = svc->serviceAddedOnOtherDates(serviceID);
        singleStopJSON["exceptions_present"]     = svc->serviceRemovedOnDates(serviceID);

        // More flexible operating day information
        bool mon, tue, wed, thu, fri, sat, sun = false;
        svc->booleanOpDays(serviceID, mon, tue, wed, thu, fri, sat, sun);
        singleStopJSON["op_mon"] = mon;
        singleStopJSON["op_tue"] = tue;
        singleStopJSON["op_wed"] = wed;
        singleStopJSON["op_thu"] = thu;
        singleStopJSON["op_fri"] = fri;
        singleStopJSON["op_sat"] = sat;
        singleStopJSON["op_sun"] = sun;
    }

    // Fill the time of service. Note that it could be distinct from the countdown when we have no scheduled time(s)!
    if (serviceDate.isNull()) {
        // No particular day requested, we will just show the "offset" times
        QTime localNoon(12, 0, 0);
        if ((*stopTimes)[tripID].at(stopTripIdx).departure_time != StopTimes::kNoTime) {
            QTime stopDep = localNoon.addSecs((*stopTimes)[tripID].at(stopTripIdx).departure_time);
            if (getStatus()->format12h()) {
                singleStopJSON["dep_time"] = stopDep.toString("h:mma");
            } else {
                singleStopJSON["dep_time"] = stopDep.toString("hh:mm");
            }
            singleStopJSON["dep_next_day"] = OperatingDay::isNextActualDay((*stopTimes)[tripID].at(stopTripIdx).departure_time);
        } else {
            singleStopJSON["dep_time"] = "-";
            singleStopJSON["dep_next_day"] = false;
        }
        if ((*stopTimes)[tripID].at(stopTripIdx).arrival_time != StopTimes::kNoTime) {
            QTime stopArr = localNoon.addSecs((*stopTimes)[tripID].at(stopTripIdx).arrival_time);
            if (getStatus()->format12h()) {
                singleStopJSON["arr_time"] = stopArr.toString("h:mma");
            } else {
                singleStopJSON["arr_time"] = stopArr.toString("hh:mm");
            }
            singleStopJSON["arr_next_day"] = OperatingDay::isNextActualDay((*stopTimes)[tripID].at(stopTripIdx).arrival_time);
        } else {
            singleStopJSON["arr_time"] = "-";
            singleStopJSON["arr_next_day"] = false;
        }
    } else {
        // A service day was specified so we should respect the actual DateTime (in case of daylight saving, etc.)
        // Given that we are using QDateTime creation from a QDate and a QTime, we SHOULD be immune from time-zone
        // and daylight-saving bugs
        QTime     localNoon(12, 0, 0);
        QDateTime localNoonDT(serviceDate, localNoon, currAgency.timeZone());

        if ((*stopTimes)[tripID].at(stopTripIdx).departure_time != StopTimes::kNoTime) {
            QDateTime stopDep = localNoonDT.addSecs((*stopTimes)[tripID].at(stopTripIdx).departure_time);
            if (getStatus()->format12h()) {
                singleStopJSON["dep_time"] = stopDep.toString("h:mma");
            } else {
                singleStopJSON["dep_time"] = stopDep.toString("hh:mm");
            }
            singleStopJSON["dst_on"]   = stopDep.isDaylightTime();
            singleStopJSON["dep_next_day"] = OperatingDay::isNextActualDay((*stopTimes)[tripID].at(stopTripIdx).departure_time);
        } else {
            singleStopJSON["dep_time"] = "-";
            singleStopJSON["dep_next_day"] = false;
        }
        if ((*stopTimes)[tripID].at(stopTripIdx).arrival_time != StopTimes::kNoTime) {
            QDateTime stopArr = localNoonDT.addSecs((*stopTimes)[tripID].at(stopTripIdx).arrival_time);
            if (getStatus()->format12h()) {
                singleStopJSON["arr_time"] = stopArr.toString("h:mma");
            } else {
                singleStopJSON["arr_time"] = stopArr.toString("hh:mm");
            }
            singleStopJSON["dst_on"]   = stopArr.isDaylightTime();
            singleStopJSON["arr_next_day"] = OperatingDay::isNextActualDay((*stopTimes)[tripID].at(stopTripIdx).arrival_time);
        } else {
            singleStopJSON["arr_time"] = "-";
            singleStopJSON["arr_next_day"] = false;
        }
    }

    // Indicate that the trip actually terminates at the requested stop (i.e. a client may wish to omit
    // service that stops running at the current station because nobody could board that trip.
    // DO NOTE HOWEVER: Some trips (depending on the agency, i.e. SEPTA Regional Rail) terminate in
    // Center City, but they actually continue on under a different route and trip ID. Some agencies account
    // for this using the drop_off_type and pickup_type.
    if (stopTripIdx == (*stopTimes)[tripID].length() - 1) {
        singleStopJSON["trip_terminates"] = true;
    } else {
        singleStopJSON["trip_terminates"] = false;
    }

    // ... and just for sanity, it is also helpful to know if the stop is at the beginning of the trip in question
    // since some agencies put a flag that the start of a trip will not drop off any passengers, but the "flag" to
    // indicate that might otherwise standout despite how normal it is to do. This hopes to provide a rationale.
    if (stopTripIdx == 0) {
        singleStopJSON["trip_begins"] = true;
    } else {
        singleStopJSON["trip_begins"] = false;
    }
}

}  // Namespace GTFS
