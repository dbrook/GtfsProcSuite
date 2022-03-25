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

#ifndef TRIPSTOPRECONCILER_H
#define TRIPSTOPRECONCILER_H

#include "datagateway.h"
#include "gtfsrealtimegateway.h"

#include <QObject>
#include <QList>

namespace GTFS {

/*
 * Trip - Stop Descriptors (populated based on schedule information
 */
typedef enum {
    SCHEDULE,   // Trip appears in the static schedule / has no real-time data associated
    NOSCHEDULE, // Trip appears in the static schedule, no real-time data associated, but also has no schedule time
    IRRELEVANT, // Trip information is too old (no longer running, for ex.) or beyond cutoff: should not appear
    DEPART,     // Trip has departed the stop_id (but it still appears in the real-time feed)
    BOARD,      // Trip is at the stop (current time is between arrival and departure time)
    ARRIVE,     // Trip is arriving at the stop_id (< 30 seconds)
    SKIP,       // Trip is skipping the stop_id
    CANCEL,     // Trip is cancelled and will not serve any stop (especially this one)
    RUNNING     // Trip is running, is not skipping the stop, nor is it canceled
} TripRecStat;

/*
 * Individual trip information for a single trip relative to a stop_id
 */
typedef struct {
    QString      tripID;              // Trip ID of the trip serving the stop
    bool         realTimeDataAvail;   // TRUE if there is real-time data available for this item
    qint64       realTimeOffsetSec;   // Schedule deviation (if a real-time recommendation with a static counterpart)
    TripRecStat  tripStatus;          // * See above *
    QDate        tripServiceDate;     // Date of service that the trip is from
    QDateTime    realTimeArrival;     // Arrival Time (if possible, else it is null) from the real-time data
    QDateTime    realTimeDeparture;   // Departure Time (if possible, else it is null) from the real-time data
    QDateTime    schDepTime;          // Departure time from the static data
    QDateTime    schArrTime;          // Arrival time from the static data
    QDateTime    schSortTime;         // In the absence of the dep/arr times in the database, we use the 'sort time'
    qint64       waitTimeSec;         // Wait time (in seconds) until the stop_id is served
    QString      headsign;            // Text that is displayed on the front of the vehicle serving the stop
    qint16       pickupType;          // Pickup Type straight from the GTFS static feed
    qint16       dropoffType;         // Drop Off Type straight from the GTFS static feed
    qint32       stopSequenceNum;     // Stop Sequence within the trip (some trips can serve a stop_id multiple times!)
    QString      stopID;              // Stop ID of the stop in question
    qint32       stopTimesIndex;      // Offset index of stop into the stop_times table of the stop within the trip
    bool         beginningOfTrip;     // TRUE if the stop_id is at the beginning of the trip
    bool         endOfTrip;           // TRUE if the stop_id is at the end of the trip
    QString      vehicleRealTime;     // Vehicle ID of an operating trip with real-time data
    QString      stopStatus;          // 4-letter code indicating validity of realTimeOffsetSec
    QDateTime    tripFirstDeparture;  // The first departure of the trip (it's actual date and time)
} StopRecoTripRec;

/*
 * For route-oriented requests, we provide a small structure to organize the data
 */
typedef struct {
    QString      shortRouteName;         // Short name of the route (from static data)
    QString      longRouteName;          // Long name of the route (from static data)
    QString      routeColor;             // Color code (background) of the route
    QString      routeTextColor;         // Color code (foreground) of the route
    QVector<StopRecoTripRec> tripRecos;  // Vector of trips serving the stop for this route
} StopRecoRouteRec;


/*
 * GTFS::TripStopReconciler is an abstraction layer to the GTFS::DataGateway to specifically process the upcoming
 * service a stop_id. Depending on the available data / style of request, it will return data regarding the upcoming
 * trips serving a stop from GTFS Realtime and/or GTFS Static feeds either based on a time range or purely # of trips.
 */
class TripStopReconciler : public QObject
{
    Q_OBJECT
public:
    /*
     * Loading a "day's worth" of trips actually requires loading 3 service days' worth:
     *
     *  - Yesterday (because trips can go past midnight and might still be valid)
     *  - Today (obvious...)
     *  - Tomorrow (in case the future minutes asked for exceeds the end of the day today
     */
    explicit TripStopReconciler(const QList<QString>     &stop_ids,
                                bool                      realTimeProcess,
                                QDate                     serviceDate,
                                const QDateTime          &currAgencyTime,
                                qint32                    futureMinutes,
                                const Status             *status,
                                const OperatingDay       *services,
                                const StopData           *stopDB,
                                const RouteData          *routeDB,
                                const TripData           *tripDB,
                                const StopTimeData       *stopTimeDB,
                                const RealTimeTripUpdate *activeFeed,
                                QObject                  *parent = nullptr);

    /*
     * Core stop_id information retrieval
     */
    // Checks that all stop IDs requested in the QList actually exist
    bool stopIdExists() const;

    // Returns the name of a single stop, or a comma-list of all stop names requested
    QString getStopName() const;

    // Returns the stop description if only 1 stop was requested
    QString getStopDesciption() const;

    /*
     * Populates routeTrips with the upcoming trips at the stop based on either:
     *  - Just static data when no realtime data is present
     *  - Reconciled static and realtime data when it is available
     *
     * This handles all the time logic and looking at the correct service days, as well as (for realtime enabled data)
     * the supplemented/delayed/early/cancelled stops with appropriate marking (TripRecStat). If TripRecStat is
     * SCHEDULE for a trip, then it is not based on any realtime data and should thus be understood as "hypothetical",
     * an INVALID trip-stop is one which no longer applies because it occurred in the past or is ahead of the lookahead
     */
    void getTripsByRoute(QHash<QString, StopRecoRouteRec> &routeTrips);

signals:

public slots:

private:
    /*
     * Local Functions for helping retrieve data from the gateway
     */
    // Fill in the StopRecoTripRec for a particular service day and route
    void addTripRecordsForServiceDay(const QString    &routeID,
                                     const QDate      &serviceDay,
                                     const QString    &stopID,
                                     StopRecoRouteRec &routeRecord) const;

    // Invalidate trips that fall outside the requested thresholds
    // The input is the fullTrips argument, the output (containing ONLY the trips which should be displayed per the
    // criteria set in the construction of this object) is found in relevantRouteTrips.
    void invalidateTrips(const QString                    &routeID,
                         QHash<QString, StopRecoRouteRec> &fullTrips,
                         QHash<QString, StopRecoRouteRec> &relevantRouteTrips);

    // Determines the running type of a scheduled trip with real-time information at a particular stop.
    // (In particular: RUN_FULLPRD vs. RUN_PR_ONLY vs. RUN_SC_ONLY)
    // Depending on the schedule vs. real-time information present, the wait time is also filled.
    //
    // Fill schArrUTC, schDepUTC, preArrUTC, preDepUTC from the results of calling tripStopActualTime()
    // The output parameters are the tripStopStatus, waitTimeSeconds, realTimeArrLT, realTimeDepLT (of StopRecoTripRec)
    void fillStopStatWaitTimeOffset(const QDateTime &schArrUTC,
                                    const QDateTime &schDepUTC,
                                    const QDateTime &preArrUTC,
                                    const QDateTime &preDepUTC,
                                    QString         &tripStopStatus,
                                    qint64          &waitTimeSeconds,
                                    qint64          &realTimeOffsetSec,
                                    QDateTime       &realTimeArrLT,
                                    QDateTime       &realTimeDepLT) const;

    /*
     * Data Members
     */
    bool           _realTimeMode;
    QDate          _svcDate;
    QList<QString> _stopIDs;            // List of Stop IDs to compute all at once
    qint32         _lookaheadMins;      // If set to 0, it is ignored
    bool           _realTimeOnly;

    QDate          _svcYesterday;
    QDate          _svcToday;
    QDate          _svcTomorrow;
    QDateTime      _currentUTC;
    QDateTime      _agencyTime;
    QDateTime      _lookaheadTime;

    /*
     * GTFS Static Database Handles
     */
    const Status       *sStatus;
    const OperatingDay *sService;
    const StopData     *sStops;
    const RouteData    *sRoutes;
    const TripData     *sTripDB;
    const StopTimeData *sStopTimes;

    /*
     * GTFS Realtime Feed Handle
     */
    const RealTimeTripUpdate *rActiveFeed;
};

} // Namespace GTFS

#endif // TRIPSTOPRECONCILER_H
