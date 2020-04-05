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

    /* See GTFSProc_Documentation.odt for JSON response format */
    void fillResponseData(QJsonObject &resp);

private:
    const GTFS::RouteData *_routes;
};

}  // Namespace GTFS

#endif // AVAILABLEROUTES_H
