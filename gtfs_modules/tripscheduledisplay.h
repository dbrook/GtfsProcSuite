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
    explicit TripScheduleDisplay(const QString &tripID, bool useRealTimeData, const QDate &realTimeDate);

    /*
     * Fills a JSON (resp) with server status information, formatted in the schema below
     * NOTE: Fields with (SR) indicate they appear both when real-time and static data is requested, if (S ): only for
     *       static dataset requests, if ( R): only for real-time requests. This is due to limitations present in the
     *       real-time data (like when a trip is "added" and has no associated static information like headsign, etc.)
     * {
     *   message_type     :string: Standard Content : "TRI" indicates this is a Trip Stop Display message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success
     *                                                101: trip ID does not exist in the static data set
     *                                                102: trip ID does not exist in the real-time feed
     *                                                103: real time data not available, but was requested anyway
     *
     *   real_time           :bool  : true if realtime prediction data is in the response, false if static
     *   real_time_data_time :string: ( R) time the real-time data set was valid
     *
     *   route_id         :string: (SR) Route ID to which the Trip ID belongs
     *   trip_id          :string: (SR) the Trip ID requested by the transaction
     *   headsign         :string: (S ) Trip's Headsign (note that headsigns can differ at individual stoptimes!)
     *   short_name       :string: (SR) short name of the trip (from trips.txt)
     *   route_short_name :string: (SR) short name of the route (from routes.txt)
     *   route_long_name  :string: (SR) long name of the route (from routes.txt)
     *   service_id       :string: (S ) service ID that the trip is assigned to
     *   operate_days     :string: (S ) days of the week that a trip operates on
     *   exception_dates  :string: (S ) individual date exceptions to the days of the weeks indicated
     *   added_dates      :string: (S ) individual dates in addition to the days of the week indicated
     *   svc_start_date   :string: (S ) start of the range of days the trip operates (inclusive)
     *   svc_end_date     :string: (S ) end of the range of days the trip operates (inclusive)
     *   vehicle          :string: ( R) the vehicle operating on the trip
     *   start_time       :string: ( R) the start time of the trip as it was given from the realtime feed
     *   start_date       :string: ( R) the start date of the trip as it was given from the realtime feed.
     *                                      ^^^^^^^^^^ is it the service date or the ACTUAL local-time-based date?
     *
     *   stops            :ARRAY : (SR) one for each stop along the trip requested
     *   [
     *     arr_time       :string: (S ) time of arrival at stop (from feed, wrapped at 24 hours, not date specific)
     *                             ( R) the time is derived from a UNIX timestamp
     *     dep_time       :string: (S ) time of departure from stop (from feed, wrapped at 24 hours, not date specific)
     *                             ( R) the time is derived from a UNIX timestamp
     *     sequence       :int   : (SR) sort of the position of the stop in the overall trip (from stoptimes.txt)
     *     stop_id        :string: (SR) stop ID of this stop
     *     stop_name      :string: (SR) name of this stop
     *     drop_off_type  :int   : (S )see the GTFS specification: the type of a drop-off (1, 2, 3, 4) of this stop
     *     pickup_type    :int   : (S )see the GTFS specificaiton: the type of a pick-up (1, 2, 3, 4) of this stop
     *     skipped        :bool  : ( R) the trip skips this otherwise-scheduled stop
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    // Instance Variables
    QString _tripID;
    bool    _realTimeDataRequested;
    bool    _realTimeDataAvailable;
    QDate   _realTimeDate;

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
