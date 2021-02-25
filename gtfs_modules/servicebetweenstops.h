/*
 * GtfsProc_Server
 * Copyright (C) 2018-2021, Daniel Brook
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

    /* See GtfsProc_Documentation.html for JSON response format */
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
