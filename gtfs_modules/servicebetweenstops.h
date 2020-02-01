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

#ifndef SERVICEBETWEENSTOPS_H
#define SERVICEBETWEENSTOPS_H

#include "staticstatus.h"

#include "operatingday.h"
#include "gtfsstops.h"
#include "gtfsroute.h"
#include "gtfstrip.h"
#include "gtfsstoptimes.h"

#include <QMap>

namespace GTFS {

typedef struct {
    QString   tripID;
    QString   routeID;
    QString   headsign;
    QDateTime oriArrival;
    QDateTime oriDeparture;
    qint32    oriStopSeq;
    qint16    ori_pickup_type;
    qint16    des_drop_off_type;
    QDateTime desArrival;
    QDateTime desDeparture;
    qint32    desStopSeq;
} tripOnDSchedule;

/*
 * Determines the services (trips, routes) that provide ***direct*** service between two requested Stop IDs so that
 * a list of times between them may be rendered. It does not provide any mechanism for transferring between services
 * as routing and other "advanced" algorithms are well outside the scope of this simple~ish data exploration platform.
 *
 * Only trips from the static dataset are considered.
 *
 * Only trips which pickup from the origin stop (all pickup types other than 1) and drop off at the destination stop
 * (all drop off types other than 1) are shown. Trips which require flagging/signaling an operator or calling an agency
 * will be denoted as such alongside the schedule times.
 */
class ServiceBetweenStops : public StaticStatus
{
public:
    /*
     * Constructor requires the following details
     *
     * originStop      - stop ID of the stop to show service from
     *
     * destinationStop - stop ID of the stop to show service to
     *
     * serviceDate     - the date for which the times should be rendered
     */
    explicit ServiceBetweenStops(const QString &originStop, const QString &destinationStop, const QDate &serviceDate);

    /*
     * Fills a JSON response with the schedule information serving the two stop IDs requested in the constructor.
     *
     * {
     *   message_type     :string: Standard Content : "SBS": service between two stop IDs
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *   error            :int   : Standard Content :   0: success
     *                                                701: the origin stop ID does not exist
     *                                                702: the destination stop ID does not exist
     *                                                703: there was no date provided or it was null
     *
     *   ori_stop_id        :string: stop ID of the origin stop (from stops.txt)
     *   ori_stop_name      :string: stop name of the origin stop (from stops.txt)
     *   ori_stop_desc      :string: stop description of the origin stop (from stops.txt)
     *   des_stop_id        :string: stop ID of the destination stop (from stops.txt)
     *   des_stop_name      :string: stop name of the destination stop (from stops.txt)
     *   des_stop_desc      :string: stop description of the destination stop (from stops.txt)
     *   service_date       :string: the service date for which trips were compared (format dd-MMM-yyyy)
     *
     *   trips              :ARRAY :
     *   [
     *     trip_id          :string: trip ID of the trip for which times are rendered (from trips.txt dataset)
     *     trip_short_name  :string: the short name of the trip ID (this is not always filled) (from trips.txt)
     *     route_id         :string: route ID that the trip belongs to (from routes.txt dataset)
     *     route_short_name :string: short name of the route ID
     *     route_long_name  :string: long name of the route ID
     *     headsign         :string: headsign on vehicle (it may be unique to the ori_stop_id or common to the trip_id)
     *     ori_arrival      :string: scheduled arrival time of the vehicle at the ori_stop_id
     *     ori_depature     :string: scheduled departure time of the vehicle at the ori_stop_id
     *     ori_pick_up      :int   : pickup type at the origin stop (0, 2, or 3, see GTFS documentation)
     *     des_arrival      :string: scheduled arrival time of the vehicle at the des_stop_id
     *     des_departure    :string: scheduled departure time of the vehicle at the des_stop_id
     *     des_drop_off     :int   : drop off type at the destination stop (0, 2, or 3, see GTFS documentation)
     *     duration         :string: duration of the trip (hh:mm format)
     *   ]
     * }
     */
    void fillResponseData(QJsonObject &resp);

private:
    // Utility function to determine which trips serve a stop for the service day
    void tripsForServiceDay(const QString                  &stopID,
                            QSet<QString>                  &tripSet,
                            bool                            isOrigin,
                            QMap<QString, tripOnDSchedule> &tods);

    // Origin stop ID and destination stop ID to find direct service between
    const QString _oriStopID;
    const QString _desStopID;

    // Service Date to render service
    const QDate   _serviceDate;

    // GTFS Datasets
    const OperatingDay   *_service;
    const StopData       *_stops;
    const ParentStopData *_parents;
    const RouteData      *_routes;
    const TripData       *_tripDB;
    const StopTimeData   *_stopTimes;
};

}  // Namespace GTFS

#endif // SERVICEBETWEENSTOPS_H
