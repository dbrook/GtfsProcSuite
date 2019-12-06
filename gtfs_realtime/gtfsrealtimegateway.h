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
    SIDE_B
} RealTimeDataRepo;

class RealTimeGateway : public QObject
{
    Q_OBJECT
public:
    // Singleton Operations
    static RealTimeGateway &inst();

    // Store the path from which to grab new protobuf data
    void setRealTimeFeedPath(const QString &realTimeFeedPath);

    // How long until the next fetch?
    qint64 secondsToFetch() const;

    // Query the active feed
    RealTimeDataRepo activeBuffer();

    // Switch over the active feed side
    void setActiveFeed(RealTimeDataRepo nextSide);

    // Retrieve the active feed (returns NULL if we are in the DISABLED mode)
    RealTimeTripUpdate *getActiveFeed();

signals:

public slots:
    // Single Fetch-and-Stop -- This should probably be made private!
    void refetchData();

    // Infinite loop to fetch new data
    void dataRetrievalLoop();

private:
    // Singleton Pattern Requirements
    explicit RealTimeGateway(QObject *parent = nullptr);
    explicit RealTimeGateway(const RealTimeGateway &);
    RealTimeGateway &operator =(RealTimeGateway const &other);
    virtual ~RealTimeGateway();

    // Dataset Members
    QMutex              _lock_activeSide;    // Prevent messing up the active side 'pointer'
    RealTimeDataRepo    _activeSide;         // Atomic indicator to denote data which is both active and ready
    QString             _dataPathLocal;
    QUrl                _dataPathRemote;
    QDateTime           _nextFetchTimeUTC;
    RealTimeTripUpdate *_sideA;
    RealTimeTripUpdate *_sideB;
};

} // Namespace GTFS

#endif // GTFSREALTIMEGATEWAY_H
