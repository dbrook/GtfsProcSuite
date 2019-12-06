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

#ifndef STOPSWITHOUTTRIPS_H
#define STOPSWITHOUTTRIPS_H

#include "staticstatus.h"

#include "gtfsstops.h"

namespace GTFS {

/*
 * GTFS::StopsWithoutTrips
 * Retrieves a list of all stops which have no trips ever serving them in the static dataset
 * The following GTFS data are required to render this:
 *  - StopData and ParentStopData
 */
class StopsWithoutTrips : public StaticStatus
{
public:
    StopsWithoutTrips();

    /*
     * Fills a JSON response with details a stopID/station in the static feed
     *
     * {
     *   message_type     :string: Standard Content : "SNT" indicates this is a "Stops [with] No Trips" message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success (no reject possible as this is debugging)
     *
     *   stops            :ARRAY : collection of stop ID (except for parent stations!) which have no serving trip IDs
     *   [
     *     stop_id        :string: stop ID of the current stop
     *     stop_name      :string: name associated with the stop ID (from stops.txt)
     *     stop_desc      :string: description associated with the stop ID (from stops.txt)
     *     loc_lat        :double: latitude of the stop (decimal degrees)
     *     loc_lon        :double: longitude of the stop (decimal degrees)
     *     parent_sta     :string: name of the parent station (to look up and see what other stops may be associated)
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    const GTFS::ParentStopData *_par;
    const GTFS::StopData       *_stops;
};

}  // Namespace GTFS

#endif // STOPSWITHOUTTRIPS_H
