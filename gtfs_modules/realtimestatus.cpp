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

#include "realtimestatus.h"

namespace GTFS {

RealtimeStatus::RealtimeStatus() : StaticStatus(), _rg(GTFS::RealTimeGateway::inst()) {}

void RealtimeStatus::fillResponseData(QJsonObject &resp)
{
    resp["seconds_to_next_fetch"] = _rg.secondsToFetch();

    QString activeSideStr, inactiveSideStr;
    GTFS::RealTimeDataRepo    active = _rg.activeBuffer();
    GTFS::RealTimeTripUpdate *rTrips = _rg.getActiveFeed();
    if (active == GTFS::SIDE_A) {
        activeSideStr   = "A";
        inactiveSideStr = "B";
    } else if (active == GTFS::SIDE_B) {
        activeSideStr   = "B";
        inactiveSideStr = "A";
    } else if (active == GTFS::IDLED) {
        activeSideStr   = "IDLE";
    } else {
        activeSideStr = inactiveSideStr = "N/A";
    }

    resp["last_realtime_query"] = _rg.mostRecentTransaction().toTimeZone(getAgencyTime().timeZone())
                                                             .toString("dd-MMM-yyyy hh:mm:ss t");

    if (rTrips == nullptr) {
        resp["active_side"] = activeSideStr;
    } else {
        // Active and inactive data information
        QDateTime activeFeedTime      = rTrips->getFeedTime();
        resp["active_rt_version"]     = rTrips->getFeedGTFSVersion();
        resp["active_side"]           = activeSideStr;
        resp["active_age_sec"]        = activeFeedTime.secsTo(getUTCTime());
        resp["active_feed_time"]      = activeFeedTime.toString("dd-MMM-yyyy hh:mm:ss t");
        resp["active_download_ms"]    = rTrips->getDownloadTimeMSec();
        resp["active_integration_ms"] = rTrips->getIntegrationTimeMSec();
    }

    // fill standard protocol information
    fillProtocolFields("RDS", 0, resp);
}

}  // Namespace GTFS
