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

#ifndef STOPSSERVEDBYROUTE_H
#define STOPSSERVEDBYROUTE_H

#include "staticstatus.h"

#include "gtfsstops.h"
#include "gtfsroute.h"

namespace GTFS {

/*
 * GTFS::StopsServedByRoute
 * Retrieves a list of all stops that a serves at least once on all of its static trips. This does not look for trips
 * from the realtime feeds.
 *
 * Computing and displaying the relevant information requires the following datasets from GTFS:
 *  - StopData
 *  - RouteData
 */
class StopsServedByRoute : public StaticStatus
{
public:
    StopsServedByRoute(const QString &routeID);

    /*
     * Fills a JSON response with details a stopID/station in the static feed
     *
     * {
     *   message_type     :string: Standard Content : "SSR" indicates this is a stops-served-by-route message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success
     *                                                501: route ID does not exist in the static dataset
     *
     *   route_id         :string: echoed back the requested route ID
     *   route_short_name :string: short name of the route (from routes.txt)
     *   route_long_name  :string: long name of the route (from routes.txt)
     *   route_desc       :string: description field of the route (from routes.txt)
     *   route_type       :string: type of route (see GTFS specification, it is unwise to rely on this field)
     *   route_url        :string: operator's dedicated URL for the route requested
     *   route_color      :string: background color of route's "brand"
     *   route_text_color :string: text/foreground color of route's "brand"
     *
     *   stops            :ARRAY : collection of stops that route's trips serve at least once per the static feed
     *   [
     *     stop_id        :string: stop ID served
     *     stop_name      :string: name of stop (from stops.txt)
     *     stop_desc      :string: description of the stop (from stops.txt)
     *     stop_lat       :double: latitude coordinates (decimal degrees) (from stops.txt)
     *     stop_lon       :double: longitude coordinates (decimal degrees) (from stops.txt)
     *     trip_count     :int   : count of the number of trip IDs which serve the stop for this route ID
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    // Instance variables
    QString _routeID;

    // GTFS datasets
    const GTFS::StopData       *_stops;
    const GTFS::RouteData      *_routes;
};

}  // Namespace GTFS

#endif // STOPSSERVEDBYROUTE_H
