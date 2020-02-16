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

#ifndef TRIPSSERVINGSTOP_H
#define TRIPSSERVINGSTOP_H

#include "staticstatus.h"

#include "gtfsstops.h"
#include "gtfstrip.h"
#include "gtfsstoptimes.h"
#include "gtfsroute.h"
#include "operatingday.h"

namespace GTFS {

/*
 * GTFS::TripsServingStop
 * Retrieves all the trips that serve a stop (or just a the trips for a given operating day*).
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
 *  - ParentStations
 *  - OperatingDay
 */
class TripsServingStop : public StaticStatus
{
public:
    explicit TripsServingStop(const QString &stopID, const QDate serviceDay);

    /*
     * Fills a JSON response with details about the stop and all trips associated with it (optionally: for single day)
     *
     * NOTE: Fields with (D) indicated mean they only appear when a specific date was requested
     * {
     *   message_type     :string: Standard Content : "TSS" indicates this is a Trip Stop Display message
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success
     *                                                301: stop ID does not exist in the static data set
     *
     *   stop_id          :string: stop ID requested (echoed back if the lookup was successful)
     *   stop_name        :string: name of the stop (from stops.txt)
     *   stop_desc        :string: description of the stop (from stops.txt)
     *   parent_sta       :string: parent station to which this stop belongs (optional) (from stops.txt)
     *   service_date     :string: service date (if requested by message) (from stops.txt)
     *
     *   routes           :ARRAY : one for each route ID that has AT LEAST ONE trip serving this stop ID
     *   [
     *     route_id         :string: route ID (from routes.txt) that the trips array contains trips for
     *     route_short_name :string: short name of the route (from routes.txt)
     *     route_long_name  :string: long name of the route (from routes.txt)
     *     route_color      :string: background color of route branding (from routes.txt)
     *     route_text_color :string: text/foreground color of route branding (from routes.txt)
     *     trips            :ARRAY : collection of all trips that serve this trip/route
     *     [
     *       trip_id                :string: trip ID that services the stop
     *       headsign               :string: headsign the vehicle will display (it can be specific to the stop or
     *                                         the same as the trip's overall headsign)
     *       short_name             :string: trip's short name
     *       drop_off_type          :int   : drop-off type (see GTFS specification for numerical values)
     *       pickup_type            :int   : pick-up type (see GTFS specification for numerical values)
     *       service_id             :string: service ID that the trip operates in conjuction with
     *       svc_start_date         :string: start date (inclusive) of the service ID
     *       svc_end_date           :string: end date (inclusive) of the service ID
     *       operate_days_condensed :string: trip's operating days (condensed into a single string like "MoTuWeThFrSa")
     *       supplements_other_days :bool  : true if the trip supplements specific days in addition to operating_days
     *       exceptions_present     :bool  : true if the trip does not operate on some of its operating_days
     *       op_mon                 :bool  : true if the trip operates on Mondays
     *       op_tue                 :bool  : true if the trip operates on Tuesdays
     *       op_wed                 :bool  : true if the trip operates on Wednesdays
     *       op_thu                 :bool  : true if the trip operates on Thursdays
     *       op_fri                 :bool  : true if the trip operates on Fridays
     *       op_sat                 :bool  : true if the trip operates on Saturdays
     *       op_sun                 :bool  : true if the trip operates on Sundays
     *       dep_time               :string: departure time at stop ((D) only corrected for DST when requesting a day)
     *       arr_time               :string: arrival time at stop ((D) only corrected for DST when requesting a day)
     *       dst_on                 :bool  : true if times are in DST ((D) only shown when requesting a specific day)
     *       trip_terminates        :bool  : true if the stop is at the last position of the trip
     *       trip_begins            :bool  : true if the stop is at the first position of the trip
     *     ]
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    // Helper function to append a valid trip belonging to a route to a structured TSS response
    void fillUnifiedTripDetailsForArray(const QString            &tripID,
                                        qint32                    stopTripIdx,
                                        const GTFS::OperatingDay *svc,
                                        const GTFS::StopTimeData *stopTimes,
                                        const GTFS::TripData     *tripDB,
                                        const QDate              &serviceDate,
                                        bool                      skipServiceDetail,
                                        bool                      addWaitTime,
                                        const QDateTime          &currAgency,
                                        qint64                    waitTimeSec,
                                        QJsonObject              &singleStopJSON);

    // Instance variables
    QString _stopID;
    QDate   _onlyDate;

    // GTFS datasets
    const GTFS::StopData       *_stops;
    const GTFS::TripData       *_tripDB;
    const GTFS::OperatingDay   *_svc;
    const GTFS::StopTimeData   *_stopTimes;
    const GTFS::RouteData      *_routes;
};

} //  Namespace GTFS

#endif // TRIPSSERVINGSTOP_H
