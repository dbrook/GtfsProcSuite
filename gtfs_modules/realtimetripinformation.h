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

#ifndef REALTIMETRIPINFORMATION_H
#define REALTIMETRIPINFORMATION_H

#include "staticstatus.h"

#include "gtfsrealtimegateway.h"

namespace GTFS {

/*
 * GTFS::RealtimeTripInformation
 * Retrieves a list of all stops from the realtime feed which are:
 *  - Operating trips which appear in the static feed
 *  - Added trips which do not appear in the static feed
 *  - Cancelled trips from the static feed which will not operate
 *
 * This only requires the realtime gateway as the logic is very simple (only string based), clients should handle
 * any information linkage as this is really more for debugging and finding trip IDs to test other modules like NEX.
 */
class RealtimeTripInformation : public StaticStatus
{
public:
    RealtimeTripInformation();

    /*
     * Fills a JSON response with details a stopID/station in the static feed
     *
     * {
     *   message_type     :string: Standard Content : "RTI" indicates this is a realtime trip information message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success (only value possible, just a debug transacttion)
     *
     *   route_id         :string: echoed back the requested route ID
     *
     *   active_trips     :OBJECT: scheduled trips which have realtime prediuctions (sorted by the route ID)
     *   {
     *     routeID        :ARRAY : array of strings representing the trip IDs with realtime information for routeID
     *   }
     *   cancelled_trips  :OBJECT: static dataset trips that are cancelled (sorted by the route ID)
     *   {
     *     routeID        :ARRAY : array of strings representing the trip IDs from the static feed which are cancelled
     *   }
     *   added_trips      :OBJECT: added trips which do not appear in the static dataset but have realtime predictions
     *   {
     *     routeID        :ARRAY : array of strings representing the trip IDs which are added by the realtime feed
     *   }
     * }
     */
    void fillResponseData(QJsonObject &resp);

    /*
     * Return a pure-JSON version of the stored realtime trip updates.
     *
     * This is really more for debugging and not public consumption, it does not follow the standard output format
     * (message_type, message_time, proc_time_ms, error).
     */
    void dumpRealTime(QString &decodedRealtime);

private:
    // Realtime Gateway
    GTFS::RealTimeTripUpdate *_rTrips;
};

}  // Namespace GTFS

#endif // REALTIMETRIPINFORMATION_H
