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

#include "availableroutes.h"

#include "datagateway.h"

#include <QJsonArray>

GTFS::AvailableRoutes::AvailableRoutes() : StaticStatus()
{
    _routes = GTFS::DataGateway::inst().getRoutesDB();
}

void GTFS::AvailableRoutes::fillResponseData(QJsonObject &resp)
{
    QJsonArray routeArray;
    for (const QString &routeID : _routes->keys()) {
        QJsonObject singleRouteJSON;
        singleRouteJSON["id"]         = routeID;
        singleRouteJSON["agency_id"]  = (*_routes)[routeID].agency_id;
        singleRouteJSON["short_name"] = (*_routes)[routeID].route_short_name;
        singleRouteJSON["long_name"]  = (*_routes)[routeID].route_long_name;
        singleRouteJSON["desc"]       = (*_routes)[routeID].route_desc;
        singleRouteJSON["type"]       = (*_routes)[routeID].route_type;
        singleRouteJSON["url"]        = (*_routes)[routeID].route_url;
        singleRouteJSON["color"]      = (*_routes)[routeID].route_color;
        singleRouteJSON["text_color"] = (*_routes)[routeID].route_text_color;
        singleRouteJSON["nb_trips"]   = (*_routes)[routeID].trips.size();
        routeArray.push_back(singleRouteJSON);
    }
    resp["routes"] = routeArray;

    // Required GtfsProc protocol fields
    fillProtocolFields("RTE", 0, resp);
}