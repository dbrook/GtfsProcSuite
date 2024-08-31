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

    /* See GtfsProc_Documentation.html for JSON response format */
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
    GTFS::Status const *_status;
};

}  // Namespace GTFS

#endif // REALTIMETRIPINFORMATION_H
