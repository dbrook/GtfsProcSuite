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

#ifndef GTFSREALTIMEGATEWAY_H
#define GTFSREALTIMEGATEWAY_H

#include <QObject>
#include <QDateTime>
#include <QUrl>
#include <QMutex>

#include "gtfsrealtimefeed.h"

namespace GTFS {

// Mechanism to avoid clashes when multithreading is processing new data
typedef enum {
    DISABLED,
    SIDE_A,
    SIDE_B,
    IDLED
} RealTimeDataRepo;

class RealTimeGateway : public QObject
{
    Q_OBJECT
public:
    // Singleton Operations
    static RealTimeGateway &inst();

    // Store the path from which to grab new protobuf data
    void setRealTimeFeedPath(const QString      &realTimeFeedPath,
                             qint32              refreshIntervalSec,
                             bool                showProtobuf,
                             rtDateLevel         rtDateMatchLevel,
                             bool                loosenStopSeqEnf,
                             bool                showDebugTrace,
                             const TripData     *tripsDB,
                             const StopTimeData *stopTimeDB);

    // How long until the next fetch?
    qint64 secondsToFetch() const;

    // Query the active feed
    RealTimeDataRepo activeBuffer();

    // Switch over the active feed side
    void setActiveFeed(RealTimeDataRepo nextSide);

    // Retrieve the active feed (returns NULL if we are in the DISABLED mode)
    RealTimeTripUpdate *getActiveFeed();

    // Get the date and time of the most recent transaction which used realtime information
    QDateTime mostRecentTransaction();

signals:

public slots:
    // Single Fetch-and-Stop -- This should probably be made private!
    void refetchData();

    // Infinite loop to fetch new data, checks every 10 seconds the number of seconds which have elapsed since a refresh
    void dataRetrievalLoop();

    // Indicate that a transaction with real-time data was just requested so we don't idle the fetching process
    void realTimeTransactionHandled();

private:
    // Singleton Pattern Requirements
    explicit RealTimeGateway(QObject *parent = nullptr);
    explicit RealTimeGateway(const RealTimeGateway &);
    RealTimeGateway &operator =(RealTimeGateway const &other);
    virtual ~RealTimeGateway();

    // Dataset Members
    qint32              _refreshIntervalSec; // Time between each data refresh attempt (in seconds)
    QMutex              _lock_activeSide;    // Prevent messing up the active side 'pointer'
    QMutex              _lock_lastRTTxn;     // Prevent messing up the last realtime transaction time
    RealTimeDataRepo    _activeSide;         // Atomic indicator to denote data which is both active and ready
    QString             _dataPathLocal;      // Data Fetch Path (local file)
    QUrl                _dataPathRemote;     // Data Fetch Path (remote URL)
    QDateTime           _nextFetchTimeUTC;   // Time at which new data real-time data should be fetched
    QDateTime           _latestRealTimeTxn;  // Stores the date of the most recent transaction requesting realtime data
    bool                _debugProtobuf;      // true if a protobuf should be serialized each time it is received
    rtDateLevel         _skipDateMatching;   // true to skip all date matching from schedule trips and realtime feed
    bool                _loosenStopSeqEnf;   // true to skip stop seq checks from stop ids from static vs. realtime feed
    bool                _trace;              // true if the periodic real-time trip update refresh traces should show
    RealTimeTripUpdate *_sideA;
    RealTimeTripUpdate *_sideB;

    const TripData     *_staticFeedTripDB;   // Pointer to the static trip database (for route ID matching)
    const StopTimeData *_staticStopTimeDB;   // Pointer to the static stop time database (for trip sanity-checks)
};

} // Namespace GTFS

#endif // GTFSREALTIMEGATEWAY_H
