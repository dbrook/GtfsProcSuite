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

#include "stationdetailsdisplay.h"

#include "datagateway.h"

#include <QSet>
#include <QJsonArray>

namespace GTFS {

StationDetailsDisplay::StationDetailsDisplay(const QString &stopID)
    : StaticStatus(), _stopID(stopID)
{
    _stops  = GTFS::DataGateway::inst().getStopsDB();
    _parSta = GTFS::DataGateway::inst().getParentsDB();
    _routes = GTFS::DataGateway::inst().getRoutesDB();
}

void StationDetailsDisplay::fillResponseData(QJsonObject &resp)
{
    QSet<QString> routesServed;
    bool stopIsParent;

    if (_parSta->contains(_stopID)) {
        // If we find a parent station, populate all the routes for all of the "sub-stations" as well
        for (const QString &subStop : (*_parSta)[_stopID]) {
            for (const QString &routeID : (*_stops)[subStop].stopTripsRoutes.keys()) {
                routesServed.insert(routeID);
            }
        }
        stopIsParent = true;
    } else if (_stops->contains(_stopID)) {
        // If it's a standalone station, populate the routes JUST for this station
        for (const QString &routeID : (*_stops)[_stopID].stopTripsRoutes.keys()) {
            routesServed.insert(routeID);
        }
        stopIsParent = false;
    } else {
        // StopID doesn't exist in either database, then an error must be raised
        fillProtocolFields("STA", 401, resp);
        return;
    }

    // Sort the routes by route_id
    QJsonArray stopRouteArray;
    QList<QString> listOfRoutes = routesServed.values();
    std::sort(listOfRoutes.begin(), listOfRoutes.end());

    resp["stop_id"]    = _stopID;
    resp["stop_name"]  = (*_stops)[_stopID].stop_name;
    resp["stop_desc"]  = (*_stops)[_stopID].stop_desc;
    resp["parent_sta"] = (*_stops)[_stopID].parent_station;
    resp["loc_lat"]    = (*_stops)[_stopID].stop_lat;
    resp["loc_lon"]    = (*_stops)[_stopID].stop_lon;

    // Find more details about all the routes served by the station
    for (const QString &routeID : qAsConst(listOfRoutes)) {
        QJsonObject singleRouteJSON;
        singleRouteJSON["route_id"]         = routeID;
        singleRouteJSON["route_short_name"] = (*_routes)[routeID].route_short_name;
        singleRouteJSON["route_long_name"]  = (*_routes)[routeID].route_long_name;
        singleRouteJSON["route_color"]      = (*_routes)[routeID].route_color;
        singleRouteJSON["route_text_color"] = (*_routes)[routeID].route_text_color;
        stopRouteArray.push_back(singleRouteJSON);
    }
    resp["routes"] = stopRouteArray;

    // If there is a valid parent, display the other associated stations
    QJsonArray stopsSharingParent;
    QString parentStation = (stopIsParent) ? _stopID : (*_stops)[_stopID].parent_station;
    if (parentStation != "") {
        for (const QString &subStopID : (*_parSta)[parentStation]) {
            QJsonObject singleSubStop;
            singleSubStop["stop_id"]   = subStopID;
            singleSubStop["stop_name"] = (*_stops)[subStopID].stop_name;
            singleSubStop["stop_desc"] = (*_stops)[subStopID].stop_desc;
            stopsSharingParent.push_back(singleSubStop);
        }
    }
    resp["stops_sharing_parent"] = stopsSharingParent;

    // Successfully filled all station information, now populate core protocol fields
    fillProtocolFields("STA", 0, resp);
}

}  // Namespace GTFS
