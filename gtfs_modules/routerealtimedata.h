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

#ifndef ROUTEREALTIMEDATA_H
#define ROUTEREALTIMEDATA_H

#include "staticstatus.h"
#include "operatingday.h"
#include "gtfsstops.h"
#include "gtfsroute.h"
#include "gtfsrealtimegateway.h"

namespace GTFS {

/*
 * GTFS::RouteRealtimeData
 * This module scrapes the real-time feed (if available) and lists all the added trips and scheduled trips with
 * real-time predictions available. One or more route IDs must be requested and the following information is given for
 * each added/real-time prediction trip pertaining to that route: trip ID, start date, vehicle, headsign, trip name,
 * the first stop ID in the real-time feed (indicating APPROXIMATELY the next stop, but it may have been passed
 * already), drop-off / pickup types, skip flag, and arrival/departure at said stop. Additionally, the route names and
 * colors are presented at route-level.
 *
 * Only trips from real-time feed are considered, but details from the trip and stop databases are merged if present.
 *
 * The following information is required:
 *  - Stops
 *  - Routes
 *  - Trips
 *  - Stop Times
 *  - Realtime Data
 */
class RouteRealtimeData : public StaticStatus
{
public:
    /*
     * Constructor requires the following details
     *
     * routeIDs        - list of route IDs or which to fetch real-time trips (added and scheduled-with-predictions)
     */
    RouteRealtimeData(QList<QString> routeIDs);

    /* See GtfsProc_Documentation.html for JSON response format */
    void fillResponseData(QJsonObject &resp);

private:
    // Utility function to determine that all the routes sent in the constructor actually exist in the feed
    bool allRoutesExistInFeed();

    QList<QString> _routeIDs;

    // GTFS Datasets
    const StopData       *_stops;
    const RouteData      *_routes;
    const TripData       *_trips;
    const StopTimeData   *_stopTimes;

    // Real-Time Data Feed Access
    bool _rtData;
    const RealTimeTripUpdate *_realTimeProc;
};

}  // Namespace GTFS

#endif // ROUTEREALTIMEDATA_H
