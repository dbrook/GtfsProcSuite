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

#ifndef REALTIMESTATUS_H
#define REALTIMESTATUS_H

#include "staticstatus.h"

#include "gtfsrealtimegateway.h"

namespace GTFS {

/*
 * GTFS::RealtimeStatus
 * Retrieves status from whichever realtime data buffer is currently active. This is mostly for debugging purposes
 * to see if bad information was retrieved (or nothing at all, possibly!)
 */
class RealtimeStatus : public StaticStatus
{
public:
    RealtimeStatus();

    /*
     * Fills a JSON response with details a stopID/station in the static feed
     *
     * {
     *   message_type     :string: Standard Content : "RDS" indicates this is a realtime data information message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success (no reject possible as this is debugging)
     *
     *   seconds_to_next_fetch :int   : number of seconds remaining until realtime data feed will be refetched
     *   active_side           :string: "N/A", "A", or "B", indicating which buffer is active for realtime operations
     *                                        A|B: all information herein refers to the statistics of the "active side")
     *                                        N/A: realtime information is not active
     *                                        IDLE: realtime information would be available, but no queries were made
     *                                              which would require it for the last 3 minutes. Data will be fetched
     *                                              within 10 seconds after a new NEX, NCF, RTR, RTI transaction
     *   last_realtime_query   :string: the time (in acency timezone) of the last query requiring realtime information
     *                                  [format: (dd-MMM-yyyy hh:mm:ss t)]
     *   active_rt_version     :string: version string from the protocol buffer (GTFS Realtime Trip Update version)
     *   active_age_sec        :int   : number of seconds since the active feed was integrated
     *   active_feed_time      :string: time denoting when the string was generated at provider (dd-MMM-yyyy hh:mm:ss t)
     *   active_download_ms    :int   : time it took to download the currently active feed (milliseconds)
     *   active_integration_ms :int   : time it took to integrate the data on the GtfsProc backend
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    GTFS::RealTimeGateway &_rg;
};

}  // Namespace GTFS

#endif // REALTIMESTATUS_H
