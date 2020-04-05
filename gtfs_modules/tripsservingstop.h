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

    /* See GTFSProc_Documentation.odt for JSON response format */
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
