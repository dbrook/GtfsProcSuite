/*
 * GtfsProc_Server
 * Copyright (C) 2018-2019, Daniel Brook
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
    ServeGTFS(QString dbRootPath, QString realTimePath, QObject *parent = 0);
    virtual ~ServeGTFS();

    void displayDebugging() const;

protected:
    virtual void incomingConnection(qintptr descriptor); //qint64, qHandle, qintptr, uint

};

#endif // SERVEGTFS_H
