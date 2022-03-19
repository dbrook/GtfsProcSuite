/*
 * GtfsProc_Server
 * Copyright (C) 2018-2022, Daniel Brook
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

#ifndef REALTIMEPRODUCTSTATUS_H
#define REALTIMEPRODUCTSTATUS_H

#include "staticstatus.h"

#include "gtfsrealtimegateway.h"

namespace GTFS {

/*
 *
 *
 */
class RealtimeProductStatus : public StaticStatus
{
public:
    RealtimeProductStatus();

    /* See GtfsProc_Documentation.html for JSON response format */
    void fillResponseData(QJsonObject &resp);

private:
    const GTFS::Status *_stat;
    GTFS::RealTimeGateway &_rg;
};

}  // Namespace GTFS

#endif // REALTIMEPRODUCTSTATUS_H
