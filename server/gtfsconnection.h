/*
 * This file is based on the HttpConnection class from the VoidRealms Qt Tutorials: http://www.voidrealms.com/
 */

#ifndef GTFSCONNECTION_H
#define GTFSCONNECTION_H

#include "tcpconnection.h"

// Access data
#include "gtfsstatus.h"

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
