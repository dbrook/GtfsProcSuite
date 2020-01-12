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

#ifndef UPCOMINGSTOPSERVICE_H
#define UPCOMINGSTOPSERVICE_H

#include "staticstatus.h"
#include "gtfsrealtimegateway.h"
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
 * ~~~~~~ TODO - finish a really detailed description of this module as it's the most complicated part of the backend!
 *
 * The following information is required:
 *  - ~~
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
     * maxTripsPerRoute - total number of trips that should be loaded per route serving the stop ID
     *
     * nexCombFormat    - Send in true if the process should combine all upcoming service and sort by arrival time
     *                    (preferred) or if not available, then the departure time. Send in false if the upcoming
     *                    service should be sorted by time but grouped by each route
     *
     * realtimeOnly     - only show service times for trips that appear in the realtime feed
     */
    UpcomingStopService(QList<QString> stopIDs,
                        qint32         futureMinutes,
                        qint32         maxTripsPerRoute,
                        bool           nexCombFormat,
                        bool           realtimeOnly);

    /*
     * Fills a JSON response with upcoming service information for a requested stop ID given the parameters requested
     *
     * NOTE: This process will look at AT MOST 3 operating days to get the full scope of the possible upcoming stops:
     *   - Yesterday -- specifically for any trips that began / still run after midnight that could show up, i.e. a
     *                  trip which began at 23:55 and runs until 25:10 (using time-since-midnight notation), but we ask
     *                  about a the next trips as of 00:35 where we are technically in the following day
     *   - Today     -- Obviously we want the trips that would pertain to this operating day, regardless of the time
     *   - Tomorrow  -- In case we ask for a future time-range which bleeds into the next operating day
     *
     * As a consequence to this, you will only ever realistically be able to ask for trips until the end of the next
     * operating day, thus the effective logical maximum future time is 1440 minutes or 24 hours, however it can be
     * longer depending on the time of day you ask for future trips and how 'far into' the current operating day it is.
     *
     * If it's preferred (and with stops covered by a myriad of confusing routes it might be), you can instead request
     * to show at most X future stops within the previous + current + next operating day using maxTripsPerRoute != 0.
     *
     * TODO: A route-filtering mechanism would be a good idea for this, since parent stations can be VERY active
     *
     * TODO: Providing a QList or something of the stop_ids should also be supported since many agencies don't file
     *       with parent_station, rendering several stop_ids which are basically the same physical stop. Basically it
     *       seems like depending on the agency size and technical expertise there is wildly different filing standards
     *
     * {
     *   message_type     :string: Standard Content : "NEX": upcoming service sorted by route ID (NEXt service)
     *                                                "NCF": upcoming service sorted by time (Next Combined Format)
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success
     *                                                601: the requested stop ID does not exist
     *
     *   stop_id          :string: stop ID for trips requested
     *   stop_name        :string: name of the stop ID (from stops.txt)
     *   stop_desc        :string: description of the stop ID (from stops.txt)
     *
     *   ---------- NEX Transactions -----------------------------------------------------------------------------------
     *   routes           :ARRAY :
     *   [
     *     route_id             :string: route ID of the trips serving the stop in the contained trips array
     *     route_short_name     :string: short name of the route
     *     route_long_name      :string: long name of the route
     *     route_color          :string: background color of the route brand
     *     route_text_color     :string: text/foreground color of the route brand
     *     trips                :ARRAY : collection of trips serving the stop
     *     [
     *       trip_id            :string: trip ID serving the stop
     *       short_name         :string: short name (from stops.txt) of the trip
     *       dep_time           :string: departure time from the stop (format "ddd hh:mm") from the static dataset
     *       arr_time           :string: arrival time at the stop (format "ddd hh:mm") from the static dataset
     *       wait_time_sec      :int   : number of seconds until the vehicle will arrive (or depart) the stop,
     *                                   adjusted for realtime information if present
     *       headsign           :string: headsign on the vehicle (can be from the overall trip, or specific to the stop)
     *       pickup_type        :int   : empty / 0 - Regularly scheduled pickup
     *                                           1 - No pickup available
     *                                           2 - Must phone agency to arrange pickup
     *                                           3 - Must coordinate with driver to arrange pickup
     *       drop_off_type      :int   : empty / 0 - Regularly scheduled drop off
     *                                           1 - No drop off available
     *                                           2 - Must phone agency to arrange drop off
     *                                           3 - Must coordinate with driver to arrange drop off
     *       trip_begins        :bool  : true if the trip begins at this stop, false otherwise
     *       trip_terminates    :bool  : true if the trip terminates at this stop, false otherwise
     *       realtime_data      :COLLEC: realtime prediction data (optional)
     *       {
     *         status           :string: ARRV = Vehicle is arriving at the stop
     *                                   BRDG = Vehicle is at the stop and boarding
     *                                   DPRT = Vehicle is departing the stop
     *                                   ONTM = Trip is operating on time vs. the schedule
     *                                   LATE = Trip is operating behind schedule (see offset_seconds)
     *                                   ERLY = Trip is operating ahead of schedule (see offset_seconds)
     *                                   SPLM = Supplemental trip (not on the static dataset)
     *                                   SKIP = Trip is skipping this stop which it would have normally stopped at
     *                                   CNCL = Trip is entirely cancelled
     *                                   MSNG = While realtime data exists, this particular stop doesn't appear in it
     *         actual_arrival   :string: time that the trip will actually arrive at the stop (format "ddd hh:mm")
     *         actual_departure :string: time that the trip will actually depart from the stop (format "ddd hh:mm")
     *         offset_seconds   :int   : number of seconds the trip is offset from the schedule: early < 0 < late)
     *         vehicle          :string: vehicle number operating on the trip
     *       }
     *     ]
     *   ]
     *
     *   ---------- NCF Transactions -----------------------------------------------------------------------------------
     *   routes {
     *     route_id           :COLLEC: {
     *       route_short_name :string: same as matching named fields in "NEX"
     *       route_long_name  :string: same as matching named fields in "NEX"
     *       route_color      :string: same as matching named fields in "NEX"
     *       route_text_color :string: same as matching named fields in "NEX"
     *     }
     *   }
     *   trips [
     *     route_id           :string: referrs to matching route_id under routes for common information
     *     trip_id            :string: same as matching named fields in "NEX"
     *     short_name         :string: same as matching named fields in "NEX"
     *     dep_time           :string: same as matching named fields in "NEX"
     *     arr_time           :string: same as matching named fields in "NEX"
     *     wait_time_sec      :int   : same as matching named fields in "NEX"
     *     headsign           :string: same as matching named fields in "NEX"
     *     pickup_type        :int   : same as matching named fields in "NEX"
     *     drop_off_type      :int   : same as matching named fields in "NEX"
     *     trip_begins        :bool  : same as matching named fields in "NEX"
     *     trip_terminates    :bool  : same as matching named fields in "NEX"
     *     realtime_data      :COLLEC:
     *     {
     *       see realtime_data documentation above (it's the same here)
     *     }
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    QList<QString> _stopIDs;
    QDate          _serviceDate;
    qint32         _futureMinutes;
    qint32         _maxTripsPerRoute;
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
    void fillRouteData(const QString &shortName,
                       const QString &longName,
                       const QString &color,
                       const QString &textColor,
                       QJsonObject   &routeDetails);

    void fillTripData(const GTFS::StopRecoTripRec &rts, QJsonObject &stopTripItem);
};

}  // Namespace GTFS

#endif // UPCOMINGSTOPSERVICE_H
