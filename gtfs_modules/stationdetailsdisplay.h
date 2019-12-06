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

#ifndef STATIONDETAILSDISPLAY_H
#define STATIONDETAILSDISPLAY_H

#include "staticstatus.h"

#include "gtfsstops.h"
#include "gtfsroute.h"

namespace GTFS {

/*
 * GTFS::StationDetailsDisplay
 * Retrieves details about a requested stop/station
 *
 * Computing and displaying the relevant information requires the following datasets from GTFS:
 *  - StopData and ParentStopData
 *  - RouteData
 */
class StationDetailsDisplay : public StaticStatus
{
public:
    StationDetailsDisplay(const QString &stopID);

    /*
     * Fills a JSON response with details a stopID/station in the static feed
     *
     * {
     *   message_type     :string: Standard Content : "STA" indicates this is a Trip Stop Display message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success
     *                                                401: stop ID does not exist in the static data set
     *
     *   stop_id          :string: the stop ID that was requested
     *   stop_name        :string: name of the stop requested (from stops.txt)
     *   stop_desc        :string: description of the stop requested (from stops.txt)
     *   parent_sta       :string: parent station (optional, used to link related stop_ids)
     *   loc_lat          :double: latitude (decimal degrees) of stop
     *   loc_lon          :double: longitude (decimal degrees) of stop
     *
     *   routes           :ARRAY : collection of routes that serve the stop at least once per the static feed
     *   [
     *     route_id         :string: route ID of the route
     *     route_short_name :string: short name of the route (from routes.txt)
     *     route_long_name  :string: long name of the route (from routes.txt)
     *     route_color      :string: background color of the route branding (from routes.txt)
     *     route_text_color :string: text/foreground color of the route branding (from routes.txt)
     *   ]
     *
     *   stops_sharing_parent :ARRAY : collection of stop IDs which share the parent station of stop_id
     *   [
     *     stop_id        :string: stop ID of the stop that shares the parent
     *     stop_name      :string: stop's name (from stops.txt)
     *     stop_desc      :string: stop's description (from stops.txt)
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    // Instance variables
    QString _stopID;

    // GTFS datasets
    const GTFS::StopData       *_stops;
    const GTFS::ParentStopData *_parSta;
    const GTFS::RouteData      *_routes;
};

}  // Namespace GTFS

#endif // STATIONDETAILSDISPLAY_H
