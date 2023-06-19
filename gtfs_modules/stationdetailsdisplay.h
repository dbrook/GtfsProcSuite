/*
 * GtfsProc_Server
 * Copyright (C) 2018-2023, Daniel Brook
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

    /* See GtfsProc_Documentation.html for JSON response format */
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
