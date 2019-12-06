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

#ifndef AVAILABLEROUTES_H
#define AVAILABLEROUTES_H

#include "staticstatus.h"
#include "gtfsroute.h"

#include <QJsonObject>

namespace GTFS {

/*
 * GTFS::AvailableRoutes
 * Lists all the routes available in the static dataset.
 */
class AvailableRoutes : public StaticStatus
{
public:
    explicit AvailableRoutes();

    /*
     * Fills a JSON (resp) with all of the routes appearing in the dataset, formatted in the schema below
     * {
     *   message_type     :string: Standard Content : "RTE" indicates this is a status message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content : 0 indicates success (no other value possible)
     *
     *   routes           :ARRAY : one for each route appearing in the feed (see contents)
     *   [
     *     id             :string: route ID (from routes.txt)
     *     agency_id      :int   : agency ID which operate the route ID (from routes.txt)
     *     short_name     :string: short name (from routes.txt)
     *     long_name      :string: long name (from routes.txt)
     *     desc           :string: description (from routes.txt)
     *     type           :int   : type of route (numerical, unreliable for mode identification) (from routes.txt)
     *     url            :string: route-specific URL (from routes.txt)
     *     color          :string: background color of the route (from routes.txt)
     *     text_color     :string: foreground/text color of the route (from routes.txt)
     *     nb_trips       :int   : number of trip IDs which operate on the route (computed)
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    const GTFS::RouteData *_routes;
};

}  // Namespace GTFS

#endif // AVAILABLEROUTES_H
