#ifndef TRIPSTOPRECONCILER_H
#define TRIPSTOPRECONCILER_H

#include "datagateway.h"
#include "gtfsrealtimegateway.h"

#include <QObject>
//#include <QMap>
//#include <QVector>
//#include <QDateTime>
//#include <QDate>

namespace GTFS {

/*
 * Trip - Stop Descriptors (populated based on schedule information
 */
typedef enum {
    SCHEDULE,   // Trip appears in the static schedule / has no real-time data associated
    NOSCHEDULE, // Trip appears in the static schedule, no real-time data associated, but also has no schedule time
    IRRELEVANT, // Trip information is too old (no longer running, for ex.) or beyond cutoff: should not appear
    SUPPLEMENT, // Trip supplements the static schedule / no basis for schedule comparison
    ON_TIME,    // Trip is running - compare expected times to determine lateness (offset from schedule)
    LATE,       // Trip is running - behind schedule
    EARLY,      // Trip is running - ahead of schedule
    DEPART,     // Trip has departed the stop_id (but it still appears in the real-time feed)
    BOARD,      // Trip is at the stop (current time is between arrival and departure time)
    ARRIVE,     // Trip is arriving at the stop_id (< 30 seconds)
    SKIP,       // Trip is skipping the stop_id
    CANCEL      // Trip is cancelled and will not serve any stop (especially this one)
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
    QDateTime    realTimeActual;      // Arrival Time (if possible, else it is the Departure) from the real-time data
    QDateTime    schDepTime;          // Departure time from the static data
    QDateTime    schArrTime;          // Arrival time from the static data
    QDateTime    schSortTime;         // In the absence of the dep/arr times in the database, we use the 'sort time'
    qint64       waitTimeSec;         // Wait time (in seconds) until the stop_id is served
    QString      headsign;            // Text that is displayed on the front of the vehicle serving the stop
    qint8        pickupType;          // Pickup Type straight from the GTFS static feed
    qint8        dropoffType;         // Drop Off Type straight from the GTFS static feed
    qint32       stopSequenceNum;     // Stop Sequence within the trip (some trips can serve a stop_id multiple times!)
    qint32       stopTimesIndex;      // Offset index of stop into the stop_times table of the stop within the trip
    bool         beginningOfTrip;     // TRUE if the stop_id is at the beginning of the trip
    bool         endOfTrip;           // TRUE if the stop_id is at the end of the trip
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
    explicit TripStopReconciler(const QString &stop_id,
                                bool           realTimeProcess,
                                QDate          serviceDate,
                                QDateTime     &currAgencyTime,
                                qint32         futureMinutes,
                                qint32         maxTripsForRoute,
                                QObject       *parent = nullptr);

    /*
     * Core stop_id information retrieval
     */
    bool stopIdExists() const;
    QString getStopName() const;
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
    void getTripsByRoute(QMap<QString, StopRecoRouteRec> &routeTrips);

signals:

public slots:

private:
    /*
     * Local Functions for helping retrieve data from the gateway
     */
    // Fill in the StopRecoTripRec for a particular service day and route
    void addTripRecordsForServiceDay(const QString &routeID,
                                     const QDate &serviceDay,
                                     StopRecoRouteRec &routeRecord) const;

    // Invalidate trips that fall outside the requested thresholds
    void invalidateTrips(const QString &routeID, QMap<QString, StopRecoRouteRec> &routeTrips);

    // Sorting based on predicted arrival times
    static bool arrivalThenDepartureSortRealTime(StopRecoTripRec &left, StopRecoTripRec &right);

    /*
     * Data Members
     */
    bool      _realTimeMode;
    QDate     _svcDate;
    QString   _stopID;
    qint32    _lookaheadMins;      // If set to 0, it is ignored
    qint32    _maxTripsPerRoute;
    bool      _realTimeOnly;

    QDate     _svcYesterday;
    QDate     _svcToday;
    QDate     _svcTomorrow;
    QDateTime _currentUTC;
    QDateTime _agencyTime;
    QDateTime _lookaheadTime;

    /*
     * GTFS Static Database Handles
     */
    const Status                              *sStatus;
    const OperatingDay                        *sService;
    const QMap<QString, StopRec>              *sStops;
    const QMap<QString, RouteRec>             *sRoutes;
    const QMap<QString, TripRec>              *sTripDB;
    const QMap<QString, QVector<StopTimeRec>> *sStopTimes;

    /*
     * GTFS Realtime Feed Handle
     */
    RealTimeTripUpdate *rActiveFeed;
};

} // Namespace GTFS

#endif // TRIPSTOPRECONCILER_H
