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

#ifndef TRIPSSERVINGROUTE_H
#define TRIPSSERVINGROUTE_H

#include "staticstatus.h"

#include "gtfsroute.h"
#include "gtfstrip.h"
#include "gtfsstoptimes.h"
#include "gtfsstops.h"
#include "operatingday.h"

namespace GTFS {

/*
 * GTFS::TripsServingRoute
 * Retrieves all the trips that serve a route (or just a the trips for a given operating day*).
 *
 * *= Note: "operating day" is not the same thing as an actual day. The best example is late night service that belongs
 *          to the day before (i.e. a "Friday Night 1am train" which technically is running on Saturday will appear
 *          on the Friday schedule. Requesting the list of service on a Saturday would NOT include this trip.
 *
 * This class does not operate on real-time information, only trips appearing in the static dataset are found.
 *
 * Computing and displaying the relevant information requires the following datasets from GTFS:
 *  - RouteData
 *  - TripData
 *  - StopTimeData
 *  - StopData
 *  - OperatingDay
 */
class TripsServingRoute : public StaticStatus
{
public:
    explicit TripsServingRoute(const QString &routeID, const QDate &onlyDate);

    /*
     * Fills a JSON response with details about the route and all trips associated with it (optionally: for single day)
     *
     * NOTE: Fields with (D) indicated mean they only appear when a specific date was requested
     * {
     *   message_type     :string: Standard Content : "TRI" indicates this is a Trip Stop Display message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success
     *                                                201: route ID does not exist in the static data set
     *
     *   route_id         :string: Route ID that was requested
     *   route_short_name :string: short name given to the route (from routes.txt) - generally preferred to long name
     *   route_long_name  :string: long name given to the route (from routes.txt) - used if no short name is given
     *   service_date     :string: (D) date for which service was requested
     *   route_color      :string: background color of the route's "brand" (from routes.txt)
     *   route_text_color :string: text/foreground color of the route's "brand" (from routes.txt)
     *
     *   trips            :ARRAY : one for each trip ID associated to the route (output also depends on date request!)
     *   [
     *     trip_id                :string: trip ID of the trip at this position of the array
     *     headsign               :string: trip's headsign
     *     service_id             :string: service ID to which this trip belongs
     *     svc_start_date         :string: start day (inclusive) of the trip's service
     *     svc_end_date           :string: end day (inclusive) of the trip's service
     *     operate_days_condensed :string: operating days of the week in a single string (like "MoTuWeThFrSaSu")
     *     supplements_other_days :bool  : true if the trip supplements other days in addition to those it operates
     *     exceptions_present     :bool  : true if there are exceptions to the days of the week the trip operates
     *     op_mon                 :bool  : true if trip operates on mondays (but there may be exceptions)
     *     op_tue                 :bool  : true if trip operates on tuesdays (but there may be exceptions)
     *     op_wed                 :bool  : true if trip operates on wednesdays (but there may be exceptions)
     *     op_thu                 :bool  : true if trip operates on thursdays (but there may be exceptions)
     *     op_fri                 :bool  : true if trip operates on fridays (but there may be exceptions)
     *     op_sat                 :bool  : true if trip operates on saturdays (but there may be exceptions)
     *     op_sun                 :bool  : true if trip operates on sundays (but there may be exceptions)
     *     first_stop_departure   :string: time that the trip leaves the first stop
     *     first_stop_dst_on      :bool  : (D) if the first stop departure time is in daylight saving time
     *     first_stop_id          :string: stop ID of the first stop of the trip
     *     first_stop_name        :string: stop name of the first stop of the trip
     *     last_stop_arrival      :string: time that the trip arrives at the last stop
     *     last_stop_dst_on       :bool  : (D) if the last stop arrival time is in daylight saving time
     *     last_stop_id           :string: stop ID of the last stop of the trip
     *     last_stop_name         :string: stop name of the last stop of the trip
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    // Instance variables
    QString _routeID;
    QDate   _onlyDate;

    // GTFS datasets
    const GTFS::RouteData    *_routes;
    const GTFS::TripData     *_tripDB;
    const GTFS::StopTimeData *_stopTimesDB;
    const GTFS::StopData     *_stopDB;
    const GTFS::OperatingDay *_svc;
};

}  // Namespace GTFS

#endif // TRIPSSERVINGROUTE_H
