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

#ifndef SERVEGTFS_H
#define SERVEGTFS_H

#include "tcpserver.h"

#include <QString>

/*
 * ServeGTFS is used to initialize the data from the requested data path(s), it also spawns the GTFS Realtime data
 * retriever's timed thread so new data is periodically retrieved. Lastly, incomingConnection is fired each time a
 * client open a socket to talk to this server.
 *
 * The transaction processing is actually handled by a QThreadPool and not with the main application thread. Therefore,
 * incoming clients are instantly connected, but their individual requests are queued.
 */
class ServeGTFS : public TcpServer
{
    Q_OBJECT
public:
    /*
     * GTFS / TCP Server object, this persists through the entire process
     *
     * dbRootPath:     path to the folder containing all GTFS *.txt files as the static dataset
     * realTimePath:   path (local or URI) to the GTFS real-time data to supplement the processor
     * rtInterval:     number of seconds to wait between each refresh of the real-time data feed
     * frozenTime:     yyyy,mm,dd,hh,mm,ss to force the transactions to always process as if it is the date specified
     *                     NOTE: this is in the timezone of the GTFS agency.txt file time
     * showProtobuf:   set to true to print realtime updates to a string on QDebug each time an update is received
     * use12h:         all date-times should render with AM/PM indicator using a 12-hour clock instead of default 24-h
     * rtDateMatchLev: real-time trip update date matching enforcement level
     * showTraces:     set to true if all transactions and real-time update operations should be logged to the terminal
     * nbTripsRtNEX:   number of trips per route that should be serialized in NEX responses
     * hideTermTrips:  set to true if trips terminating at the requested stop should be hidden (NEX/NCF only)
     * looseRTStopSeq: do not enforce strict stop sequence / stop id checks when sequences are avail. in realtime feed
     */
    ServeGTFS(QString  dbRootPath,
              QString  realTimePath,
              qint32   rtInterval,
              QString  frozenTime,
              bool     showProtobuf,
              bool     use12h,
              quint32  rtDateMatchLev,
              bool     showTraces,
              quint32  nbTripsRtNEX,
              bool     hideTermTrips,
              bool     looseRTStopSeq,
              QObject *parent        = nullptr);
    virtual ~ServeGTFS();

    void displayDebugging() const;

protected:
    virtual void incomingConnection(qintptr descriptor); //qint64, qHandle, qintptr, uint

private:
    bool _showTraces;
};

#endif // SERVEGTFS_H
