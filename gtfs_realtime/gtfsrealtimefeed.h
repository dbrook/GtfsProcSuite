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
#include <QMap>
#include <QPair>
#include <QString>
#include <QDate>
#include <QDateTime>

// And of course have a space for the protobuf
#include "gtfs-realtime.pb.h"

namespace GTFS {

typedef struct {
    qint64    stopSequence;
    QString   stopID;
    QDateTime depTime;
    QDateTime arrTime;
    bool      stopSkipped;
} rtStopTimeUpdate;

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
 */
class RealTimeTripUpdate : public QObject
{
    Q_OBJECT
public:
    explicit RealTimeTripUpdate(const QString &rtPath, QObject *parent = nullptr);
    explicit RealTimeTripUpdate(const QByteArray &gtfsRealTimeData, QObject *parent = nullptr);
    virtual ~RealTimeTripUpdate();

    /*
     * General status functions / diagnostics
     */
    // Time of feed (in seconds since UNIX epoch - UTC)
    QDateTime getFeedTime() const;

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
    bool tripIsCancelled(const QString &trip_id, const QDate &serviceDay) const;

    // List the trip_ids which serve a stop_id (populates 'addedTrips' with new-found trips) across all added stops
    // We map all relevant routes to a vector of trips that serve the requested stop_id
    void getAddedTripsServingStop(const QString &stop_id,
                                  QMap<QString, QVector<QPair<QString, quint32>>> &addedTrips) const;

    // Figure out where an added trip is headed since no way to lookup headsign for a trip not in the static feed
    const QString getFinalStopIdForAddedTrip(const QString &trip_id) const;

    // Is the trip (that came from the static feed) actually running?
    bool scheduledTripIsRunning(const QString &trip_id, const QDate &operDateDMY) const;

    // Was the trip-update to specifically skip the stop?
    bool tripSkipsStop(const QString &stop_id, const QString &trip_id, qint64 stopSeq, const QDate &serviceDay) const;

    // Has the trip already passed the stop? This is useful for indicating that a trip went by early (i.e. don't care
    // about showing things which have already departed the stop)
    // NOTE: This is not compatible with V1 feeds!
    bool scheduledTripAlreadyPassed(const QString &trip_id, qint64 stopSeq) const;

    // What is the actual time of arrival? (Returns realArr/DepTime as seconds-since-UNIX-epoch in UTC - 64-bit)
    // Returns a FALSE if the trip and stopseq combination was not found in the data
    // Unused values will come back as 0 ... probably
    //
    // VERSION - COMPATIBILITY WARNING! Version 2 of GTFS-Realtime natively uses seconds-since-UNIX-epoch / UTC 64-bit,
    // but Version 1 simply uses offsets, so this function does a version sanity check and then...:
    //   Version 2 - compares the trip_id and stopSeq to propagate realArrTimeUTC/realDepTimeUTC
    //   Version 1 - use trip_id, stop_id, schedArrTimeUTC, schedDepTimeUTC to compute realArrTimeUTC/realDepTimeUTC
    bool tripStopActualTime(const QString   &trip_id,
                            qint64           stopSeq,
                            const QString   &stop_id,
                            const QDateTime &schedArrTimeUTC,
                            const QDateTime &schedDepTimeUTC,
                            QDateTime       &realArrTimeUTC,
                            QDateTime       &realDepTimeUTC) const;

    // Fill an array of all the stop times for a requested real-time trip_id
    // Passes back route_id and stopTimes
    //
    // VERSION - COMPATIBILITY WARNING! See note from tripStopActualTime above, the same problem exists for listing
    // all the stop times of a tripID because with V1 data everything is done with offsets
    //
    // Use isRTVersion1 function to see if you need to send in an additional array with static feed
    void fillStopTimesForTrip(const QString             &tripID,
                              QString                   &route_id,
                              QVector<rtStopTimeUpdate> &schedStopTimes,
                              QVector<rtStopTimeUpdate> &stopTimes) const;

    // Returns true if the realtime data within is version 1
    bool isRTVersion1() const;

    // Retrieve operating vehicle information
    const QString getOperatingVehicle(const QString &tripID) const;

    // Get a map of routes with lists of trips with prediction information
    void getAllTripsWithPredictions(QMap<QString, QVector<QString>> &addedRouteTrips,
                                    QMap<QString, QVector<QString>> &activeRouteTrips,
                                    QMap<QString, QVector<QString>> &cancelledRouteTrips) const;

signals:

public slots:

private:
    /*
     * Helper functions
     */
    // Once data is ingested (either from a byte array from a URL or a local file's fstream), the process to ingest
    // trip updates is the same and encapsulated in this function to prevent previous code duplication
    void processUpdateDetails(const QDateTime &startProcTimeUTC);

    /*
     * Data Members
     */
    transit_realtime::FeedMessage   _tripUpdate;     // Hold the raw protobuf here, but it is not optimized for reading

    QMap<QString, qint32>           _cancelledTrips; // Track cancelled trips with associated entity idx
    QMap<QString, qint32>           _addedTrips;     // Trips running which do not correspond to the GTFS Static data
    QMap<QString, qint32>           _activeTrips;    // Trips running with real-time data (to replace schedule times)

    // Stop-IDs which have been skipped by any number of trips
    QMap<QString, QVector<QPair<QString, quint32>>> _skippedStops;

    qint64 _downloadTimeMSec;
    qint64 _integrationTimeMSec;
};

} // namespace GTFS

#endif // GTFSREALTIMEFEED_H
