/*
 * GtfsProc_Server
 * Copyright (C) 2018-2021, Daniel Brook
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

#include "stopsservedbyroute.h"

#include "datagateway.h"

#include <QJsonArray>

static inline void swap(QJsonValueRef v1, QJsonValueRef v2)
{
    QJsonValue temp(v1);
    v1 = QJsonValue(v2);
    v2 = temp;
}

namespace GTFS {

StopsServedByRoute::StopsServedByRoute(const QString &routeID)
    : StaticStatus(), _routeID(routeID)
{
    _stops  = GTFS::DataGateway::inst().getStopsDB();
    _routes = GTFS::DataGateway::inst().getRoutesDB();
}

void StopsServedByRoute::fillResponseData(QJsonObject &resp)
{
    // The route ID does not exist in the database
    if (!_routes->contains(_routeID)) {
        fillProtocolFields("SSR", 501, resp);
        return;
    }

    resp["route_id"]         = _routeID;
    resp["route_short_name"] = (*_routes)[_routeID].route_short_name;
    resp["route_long_name"]  = (*_routes)[_routeID].route_long_name;
    resp["route_desc"]       = (*_routes)[_routeID].route_desc;
    resp["route_type"]       = (*_routes)[_routeID].route_type;
    resp["route_url"]        = (*_routes)[_routeID].route_url;
    resp["route_color"]      = (*_routes)[_routeID].route_color;
    resp["route_text_color"] = (*_routes)[_routeID].route_text_color;

    // Find more details about all the routes served by the station
    QJsonArray routeStopArray;
    for (const QString &stopID : (*_routes)[_routeID].stopService.keys()) {
        QJsonObject singleStopJSON;
        singleStopJSON["stop_id"]    = stopID;
        singleStopJSON["stop_name"]  = (*_stops)[stopID].stop_name;
        singleStopJSON["stop_desc"]  = (*_stops)[stopID].stop_desc;
        singleStopJSON["stop_lat"]   = (*_stops)[stopID].stop_lat;
        singleStopJSON["stop_lon"]   = (*_stops)[stopID].stop_lon;
        singleStopJSON["trip_count"] = (*_routes)[_routeID].stopService[stopID];
        routeStopArray.push_back(singleStopJSON);
    }

    // Sort the output by stop ID, otherwise the data coming from the hash will be in a seemingly random order
    std::sort(routeStopArray.begin(), routeStopArray.end(),
              [](const QJsonValue &routeA, const QJsonValue &routeB) {
        return routeA.toObject()["stop_id"].toString() < routeB.toObject()["stop_id"].toString();
    });

    resp["stops"] = routeStopArray;

    // Success, fill standard protocol information
    fillProtocolFields("SSR", 0, resp);
}

}  // Namespace GTFS
