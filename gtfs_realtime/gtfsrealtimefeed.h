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

#ifndef GTFSREALTIMEFEED_H
#define GTFSREALTIMEFEED_H

#include <QObject>
#include <QVector>
#include <QHash>
#include <QPair>
#include <QString>
#include <QDate>
#include <QDateTime>

// And of course have a space for the protobuf
#include "gtfs-realtime.pb.h"

// For feeds which do not use UNIX timestamps but instead just offsets, the static feed must be compared
#include "gtfsstoptimes.h"

// For real-time updates which do not include route information, provide the trips database which gives route ID
#include "gtfstrip.h"

namespace GTFS {

typedef struct {
    qint64    stopSequence;
    QString   stopID;
    QDateTime arrTime;
    QChar     arrBased;
    QDateTime depTime;
    QChar     depBased;
    bool      stopSkipped;
} rtStopTimeUpdate;

typedef enum {
    SERVICE_DATE = 0,
    ACTUAL_DATE  = 1,
    NO_MATCHING  = 2
} rtDateLevel;

/*
 * RealTimeTripUpdate is the main interface to the GTFS Realtime feed information. This class Qt-ifies the ProtoBuf
 * data so that it is quicker to search and provide useful real-time trip data to which the static feeds can be
 * compared.
 *
 * The client can either:
 *  1) Send actual bytes of the realTimeData using the response from the real-time data server, or
 *  2) with a fixed static file, just specify the local filesystem path (this is helpful for debugging)
 *
 * The current GTFS RealTime data supported is the Trip Updates, which include information on:
 *  - Actual stop arrival/departure times
 *  - Cancelled trips
 *  - Added trips (+all the stops they serve, since there would otherwise be no basis from which to compare)
 *  - Individual stop_ids which are skipped by running trips
 *
 * Agencies can use varying degrees of start date and start time (some use a start date of a realtime trip ID of the
 * actual service date + times able to go beyond 24:00:00, while others use the actual service date + clocks which go
 * to 23:59:59 and then 00:00:00. Use skipDateMatching == true to skip any enforcement of the start date when matching
 * trip IDs in the realtime data set.
 *
 * Agencies may also only provide a prediction for the next stop in a running trip. The departure offset can be
 * cascaded to all stops in the trip by setting propagateOffsetSec to true.
 */
class RealTimeTripUpdate : public QObject
{
    Q_OBJECT
public:
    explicit RealTimeTripUpdate(const QString      &rtPath,
                                bool                dumpProtobuf,
                                rtDateLevel         skipDateMatching,
                                const TripData     *tripsDB,
                                const StopTimeData *stopTimeDB,
                                QObject            *parent = nullptr);
    explicit RealTimeTripUpdate(const QByteArray   &gtfsRealTimeData,
                                bool                dumpProtobuf,
                                rtDateLevel         skipDateMatching,
                                bool                displayBufferInfo,
                                const TripData     *tripsDB,
                                const StopTimeData *stopTimeDB,
                                QObject            *parent = nullptr);
    virtual ~RealTimeTripUpdate();

    /*
     * General status functions / diagnostics
     */
    // Time of feed (in a QDateTime)
    QDateTime getFeedTime() const;

    // Time of feed (in seconds since UNIX epoch - UTC). If this returns 0, an empty buffer was downloaded
    quint64 getFeedTimePOSIX() const;

    // Version of the GTFS-Realtime message
    const QString getFeedGTFSVersion() const;

    // Time it took to download (milliseconds)
    void setDownloadTimeMSec(qint64 downloadTime);
    qint64 getDownloadTimeMSec() const;

    // Time it took to integrate the feed (milliseconds)
    void setIntegrationTimeMSec(qint64 integrationTime);
    qint64 getIntegrationTimeMSec() const;

    /*
     * Stop-Route-Trip-Time calculation functions
     */
    // Trip exists in either the added or running trip arrays
    bool tripExists(const QString &trip_id) const;

    // Is the trip cancelled based on the real-time data?
    bool tripIsCancelled(const QString &trip_id, const QDate &serviceDate, const QDate &actualDate) const;

    // List the trip_ids which serve a stop_id (populates 'addedTrips' with new-found trips) across all added stops
    // We map all relevant routes to a vector of trips that serve the requested stop_id
    void getAddedTripsServingStop(const QString &stop_id,
                                  QHash<QString, QVector<QPair<QString, quint32>>> &addedTrips) const;

    // Figure out where an added trip is headed since no way to lookup headsign for a trip not in the static feed
    const QString getFinalStopIdForAddedTrip(const QString &trip_id) const;

    // Retrieve the route ID that the trip relates to
    const QString getRouteID(const QString &trip_id) const;

    // Is the trip (that came from the static feed) actually running?
    bool scheduledTripIsRunning(const QString &trip_id, const QDate &serviceDate, const QDate &actualDate) const;

    // Was the trip-update to specifically skip the stop?
    bool tripSkipsStop(const QString &stop_id,
                       const QString &trip_id,
                       qint64         stopSeq,
                       const QDate   &serviceDate,
                       const QDate   &actualDate) const;

    // Has the trip already passed the stop? This is useful for indicating that a trip went by early (i.e. don't care
    // about showing things which have already departed the stop)
    bool scheduledTripAlreadyPassed(const QString              &trip_id,
                                    qint64                      stopSeq,
                                    const QVector<StopTimeRec> &tripTimes) const;

    // What is the actual time of arrival? (returns the QDateTime in UTC of the actual arrival/departure times)
    // If no match was found, realArrTimeUTC/realDepTimeUTC will be NULL QDateTimes
    void tripStopActualTime(const QString              &tripID,
                            qint64                      stopSeq,
                            const QString              &stop_id,
                            const QTimeZone            &agencyTZ,
                            const QVector<StopTimeRec> &tripTimes,
                            QDateTime                  &realArrTimeUTC,
                            QDateTime                  &realDepTimeUTC) const;

    // Fill in predicted times for a single stop (helper function to avoid repeated code
    void fillPredictedTime(const transit_realtime::TripUpdate_StopTimeUpdate &stu,
                           const QDateTime                                   &schedArrTimeUTC,
                           const QDateTime                                   &schedDepTimeUTC,
                           QDateTime                                         &realArrTimeUTC,
                           QDateTime                                         &realDepTimeUTC,
                           QChar                                             &realArrBased,
                           QChar                                             &realDepBased) const;

    // Fill an array of all the stop times for a requested real-time trip_id
    // Passes back route_id and stopTimes
    void fillStopTimesForTrip(const QString              &tripID,
                              const QTimeZone            &agencyTZ,
                              const QVector<StopTimeRec> &tripTimes,
                              QVector<rtStopTimeUpdate>  &rtStopTimes) const;

    // Retrieve operating vehicle information
    const QString getOperatingVehicle(const QString &tripID) const;

    // Retrieve the start_date and start_time fields of the trip_update from the realtime feed
    const QString getTripStartTime(const QString &tripID) const;
    const QString getTripStartDate(const QString &tripID) const;

    // Get a hash of routes with lists of trips with prediction information
    void getAllTripsWithPredictions(QHash<QString, QVector<QString>> &addedRouteTrips,
                                    QHash<QString, QVector<QString>> &activeRouteTrips,
                                    QHash<QString, QVector<QString>> &cancelledRouteTrips,
                                    QHash<QString, QVector<QString>> &mismatchRTTrips,
                                    QHash<QString, QHash<QString, QVector<qint32>>> &duplicateRTTrips,
                                    QVector<QString> &tripsWithoutRoutes) const;

    // Dump a string-representation of the protobuf trip updates
    void serializeTripUpdates(QString &output) const;

signals:

public slots:

private:
    /*
     * Helper functions
     */
    // Once data is ingested (either from a byte array from a URL or a local file's fstream), the process to ingest
    // trip updates is the same and encapsulated in this function to prevent previous code duplication
    void processUpdateDetails(const QDateTime &startProcTimeUTC);

    // Dump the protocol buffer real time update into QDebug
    void showProtobufData() const;

    // Checks the _addedTrips and _activeTrips fields and fills in the entityIdx with the position in the trip_update
    // FeedMessage. isSupplemental is filled with true if the trip came from the supplemental trips. A false is returned
    // in the event no entityIdx was found for the requested tripID.
    bool findEntityIndex(const QString &tripID, qint32 &entityIdx, bool &isSupplemental) const;

    /*
     * Data Members
     */
    const TripData                 *_tripDB;         // Trips Database for feeds which do not provide Route ID w/ Trips
    const StopTimeData             *_stopTimeDB;     // Stop-Times Database for sanity-checking real-time trip updates

    transit_realtime::FeedMessage   _tripUpdate;     // Hold the raw protobuf here, but it is not optimized for reading

    QHash<QString, qint32>          _cancelledTrips; // Track cancelled trips with associated entity idx
    QHash<QString, qint32>          _addedTrips;     // Trips running which do not correspond to the GTFS Static data
    QHash<QString, qint32>          _activeTrips;    // Trips running with real-time data (to replace schedule times)

    // Stop-IDs which have been skipped by any number of trips
    QHash<QString, QVector<QPair<QString, quint32>>> _skippedStops;

    /*
     * The early design decision to use route ID indexes means that anytime we have a duplicate trip ID (this could
     * theoretically be possible with a trip operating on contiguous days and there is real-time information for both)
     * ... it is unlikely, but with the creation of the RPS transaction, this condition is now easily monitor-able.
     */
    QHash<QString, QVector<qint32>> _duplicateTrips;

    /*
     * Trips may not have any route associated with them (also rare)
     * When processing real-time trip updates, a best effort is made to align the trip ID with a route (hence why the
     * trips database must be known to this class) in case the real-time trip updates do not contain the related route.
     */
    QVector<QString> _noRouteTrips;

    /*
     * Trips that are from the scheduled dataset might have additional stops encoded in the real-time feed. These stops
     * along these trips will NOT be considered by GtfsProc, so with the advent of the RPS transaction, this condition
     * may be monitored.
     */
    QHash<QString, QVector<QString>> _stopsMismatchTrips;

    qint64      _downloadTimeMSec;
    qint64      _integrationTimeMSec;

    rtDateLevel _dateEnforcement;   // Service / operating date enforcement level
};

} // namespace GTFS

#endif // GTFSREALTIMEFEED_H
