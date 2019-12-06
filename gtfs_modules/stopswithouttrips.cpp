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

#include "stopswithouttrips.h"

#include "datagateway.h"

#include <QJsonArray>

namespace GTFS {

StopsWithoutTrips::StopsWithoutTrips()
{
    _par = GTFS::DataGateway::inst().getParentsDB();
    _stops = GTFS::DataGateway::inst().getStopsDB();
}

void StopsWithoutTrips::fillResponseData(QJsonObject &resp)
{
    // Run through the entire stops database and find any that have no trips associated with them
    QJsonArray stopArray;
    for (const QString &stopID : (*_stops).keys()) {
        if (_par->contains(stopID)) {
            // Don't bother listing parent stations, as their childless children will be printed anyway
            continue;
        }

        if ((*_stops)[stopID].stopTripsRoutes.size() == 0) {
            QJsonObject unassocStop;
            unassocStop["stop_id"]    = stopID;
            unassocStop["stop_name"]  = (*_stops)[stopID].stop_name;
            unassocStop["stop_desc"]  = (*_stops)[stopID].stop_desc;
            unassocStop["loc_lat"]    = (*_stops)[stopID].stop_lat;
            unassocStop["loc_lon"]    = (*_stops)[stopID].stop_lon;
            unassocStop["parent_sta"] = (*_stops)[stopID].parent_station;
            stopArray.push_back(unassocStop);
        }
    }

    resp["stops"] = stopArray;

    // Fill message protocol requirements
    fillProtocolFields("SNT", 0, resp);
}

}  // Namespace GTFS
