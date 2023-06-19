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

#ifndef UPCOMINGSTOPSERVICE_H
#define UPCOMINGSTOPSERVICE_H

#include "staticstatus.h"
#include "gtfstrip.h"
#include "gtfsstatus.h"
#include "gtfsstops.h"
#include "gtfsstoptimes.h"
#include "gtfsroute.h"
#include "operatingday.h"
#include "tripstopreconciler.h"

#include <QList>

namespace GTFS {

/*
 * GTFS::UpcomingStopService
 * This was really the initial intent of the GtfsProc - a way to avoid the API call limitations that transit agencies
 * enforce on their web services to get stop trip service wait times. Early forms of real-time data provided by the
 * Massachusetts Bay Transportation Authority (MBTA) in the Commonwealth of Massachusetts, U.S.A., but it was specific
 * to the agency and later decommissioned. A fully-standards-based replacement was envisioned, using the open General
 * Transit Feed Specification (GTFS).
 *
 * This module loads trips that service a single stop ID, several stop IDs, or parent-stations as created in the
 * GTFS Static Dataset files. It uses the scheduled (and real-time, if present) trip data and determines the wait time
 * for each trip within a specified time window, or number of future trips. Along with this information, if real-time
 * data is present, the schedule offset and arrival/departure/boarding statuses are determined.
 *
 * There are several other points to know about transit feeds (services after midnight, in particular), so it is also
 * wise to read the comments in the member functions of this class as well.
 *
 * The following information is required:
 *  - Stops and Parent Stations
 *  - Routes
 *  - Trips
 *  - Stop Times
 */
class UpcomingStopService : public StaticStatus
{
public:
    /*
     * Constructor requires the following details:
     *
     * stopID           - the stop ID for which upcoming service shall be determined
     *
     * futureMinutes    - number of minutes into the future that should be scanned for stop trip service (max of 4320)
     *
     * nexCombFormat    - Send in true if the process should combine all upcoming service and sort by arrival time
     *                    (preferred) or if not available, then the departure time. Send in false if the upcoming
     *                    service should be sorted by time but grouped by each route
     *
     * realtimeOnly     - only show service times for trips that appear in the realtime feed
     */
    UpcomingStopService(QList<QString> stopIDs,
                        qint32         futureMinutes,
                        bool           nexCombFormat,
                        bool           realtimeOnly);

    /* See GtfsProc_Documentation.html for JSON response format */
    void fillResponseData(QJsonObject &resp);

private:
    QList<QString> _stopIDs;
    QDate          _serviceDate;
    qint32         _futureMinutes;
    bool           _combinedFormat;
    bool           _realtimeOnly;

    const Status         *_status;
    const OperatingDay   *_service;
    const StopData       *_stops;
    const ParentStopData *_parentSta;
    const RouteData      *_routes;
    const TripData       *_tripDB;
    const StopTimeData   *_stopTimes;

    bool _rtData;
    const GTFS::RealTimeTripUpdate *_realTimeProc;

    // Utility Functions
    void fillTripData(const GTFS::StopRecoTripRec &rts, QJsonObject &stopTripItem);
};

}  // Namespace GTFS

#endif // UPCOMINGSTOPSERVICE_H
