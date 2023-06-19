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

#ifndef TRIPSCHEDULEDISPLAY_H
#define TRIPSCHEDULEDISPLAY_H

#include "staticstatus.h"

#include "gtfstrip.h"
#include "operatingday.h"
#include "gtfsroute.h"
#include "gtfsstops.h"
#include "gtfsstoptimes.h"
#include "gtfsrealtimegateway.h"

#include <QJsonObject>

namespace GTFS {

/*
 * GTFS::TripScheduleDisplay
 * Retrieves the route, stops, and schedule/predicted time for each stop along a requested tripID.
 *
 * This class can be used to query static dataset information or real-time "predicted" stop times for a tripID.
 * Due to the practical differences in static and real-time trips, pay close attention to the JSON response details
 * in the comment for GTFS::TripScheduleDisplay::fillResponseData.
 *
 * Computing and displaying the relevant information requires the following datasets from GTFS:
 *  - TripData
 *  - OperatingDay
 *  - RouteData
 *  - StopData
 *  - StopTimeData
 *  - RealTimeTripUpdate
 */
class TripScheduleDisplay : public StaticStatus
{
public:
    /* Connects to a GTFS::DataGateway, Trips, Stops to prepare rendering of a specific Trip ID */
    explicit TripScheduleDisplay(const QString &tripID,
                                 bool           useRealTimeData,
                                 const QDate   &realTimeDate,
                                 rtUpdateMatch  realTimeTripStyle);

    /* See GtfsProc_Documentation.html for JSON response format */
    void fillResponseData(QJsonObject &resp);

private:
    // Instance Variables
    QString _tripID;
    bool    _realTimeDataRequested;
    bool    _realTimeDataAvailable;
    QDate   _realTimeDate;

    // Query Mode
    rtUpdateMatch _realTimeTripStyle;
    quint64       _rttuIdx;

    // Datasets
    const GTFS::TripData           *_tripDB;
    const GTFS::OperatingDay       *_svc;
    const GTFS::RouteData          *_routes;
    const GTFS::StopData           *_stops;
    const GTFS::StopTimeData       *_stopTimes;
    const GTFS::RealTimeTripUpdate *_realTimeProc;
};

}  // Namespace GTFS

#endif // TRIPSCHEDULEDISPLAY_H
