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

/*
 * This file is based on the HttpConnection class from the VoidRealms Qt Tutorials: http://www.voidrealms.com/
 */

#ifndef GTFSCONNECTION_H
#define GTFSCONNECTION_H

#include "tcpconnection.h"

// Access data
#include "gtfsstatus.h"

/*
 * GtfsConnection is a class which binds to a QThreadPool for the purposes of processing a client's request. Client
 * connections are handled with connected() and disconnected(), readyRead() is signaled when a request come from a
 * client (the handling of such a request is sent to requestApplication() which actually hands off the request to the
 * QThreadPool), and taskResult() is signaled from the thread processing a request so the data can be sent client-side.
 */
class GtfsConnection : public TcpConnection
{
    Q_OBJECT
public:
    GtfsConnection(QObject *parent = 0);
    virtual ~GtfsConnection();

protected:
    void requestApplication(QString applicationRequest);

signals:

public slots:
    virtual void connected();
    virtual void disconnected();

    // Accepts / hands off queries from the clients to application processing
    virtual void readyRead();

    // Responds to the clients with JSON data
    virtual void bytesWritten(qint64 bytes);

    // Accepts the asynchronous result of a task to respond to the client
    virtual void taskResult(QString userResponse);

    virtual void stateChanged(QAbstractSocket::SocketState socketState);
    virtual void error(QAbstractSocket::SocketError socketError);

private:
    // Data stores for application?
    GTFS::Status const *Status;
};

#endif // GTFSCONNECTION_H
