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

    /* See GTFSProc_Documentation.odt for JSON response format */
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
