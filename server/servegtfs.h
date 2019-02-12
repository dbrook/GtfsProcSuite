#ifndef SERVEGTFS_H
#define SERVEGTFS_H

#include "tcpserver.h"

// GTFS "Database Records" from CSV Files
#include "gtfsstatus.h"

#include <QString>

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
