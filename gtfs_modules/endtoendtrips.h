/*
 * GtfsProc_Server
 * Copyright (C) 2023, Daniel Brook
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

#ifndef ENDTOENDTRIPS_H
#define ENDTOENDTRIPS_H

#include "staticstatus.h"
#include "gtfstrip.h"
#include "gtfsstatus.h"
#include "gtfsstops.h"
#include "gtfsstoptimes.h"
#include "gtfsroute.h"
#include "operatingday.h"
#include "tripstopreconciler.h"

namespace GTFS {

class EndToEndTrips : public StaticStatus
{
public:
    /*
     * Constructor requires the following arguments:
     *
     * futureMinutes    - number of minutes into the future to search for departures from the origin station
     * realtimeOnly     - only use trips with realtime data to make the journey recommendations
     * argList          - list of stop ids interspersed with connection times in minutes
     *
     * Example argument schema:
     * [0] "stopIdOrigin"
     * [1] "stopIdFirstConnection"
     * [2] "2"                      // Indicates 2 minute buffer between alighting at [1] and boarding at [3]
     * [3] "stopIdConnectionDep"
     * [4] "stopIdDestination"
     */
    EndToEndTrips(qint32 futureMinutes, bool realtimeOnly, QList<QString> argList);

    /* See GtfsProc_Documentation.html for JSON response format */
    void fillResponseData(QJsonObject &resp);

private:
    /*
     * Takes the upcoming departures from the oriStopId and compares the trip IDs to the arrivals at desStopId. As the
     * recommendations are built out, invalidated ones (where a connection conforming to the limitations set in the
     * constructor is/are not possible) are stored in deadRecos. The allRecos vector contains all the valid travel
     * proposals/recommendations.
     *
     * Arguments:
     * legNum           - 0-indexed leg number of the origin-destination requested
     * xferMin          - transfer time minutes between previous destination arrival and the current origin departure
     * oriStopId        - stop ID of the beginning of the leg to board
     * desStopId        - stop ID of the end of the leg to alight
     * deadRecos        - vector indexes of allRecos which are not possible to connect within 12 hours
     * allRecos         - internal data structure for each leg and the origin/destination within each leg
     */
    void fillRecoOD(quint8                             legNum,
                    quint32                            xferMin,
                    QString                            oriStopId,
                    QString                            desStopId,
                    QSet<qsizetype>                   &deadRecos,
                    QVector<QVector<StopRecoTripRec>> &allRecos);

    qint32         _futureMinutes;
    bool           _realtimeOnly;
    QList<QString> _tripCnx;

    bool                  _rtData;
    QDate                 _systemDate;
    const Status         *_status;
    const OperatingDay   *_service;
    const StopData       *_stops;
    const ParentStopData *_parentSta;
    const RouteData      *_routes;
    const TripData       *_tripDB;
    const StopTimeData   *_stopTimes;

    const GTFS::RealTimeTripUpdate *_realTimeProc;
};

}  // namespace GTFS

#endif // ENDTOENDTRIPS_H
