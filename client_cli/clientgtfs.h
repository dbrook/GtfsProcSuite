#ifndef CLIENTGTFS_H
#define CLIENTGTFS_H

#include "structureddisplay.h"

#include <QObject>
#include <QTcpSocket>

class ClientGtfs : public QObject
{
    Q_OBJECT
public:
    // Initializes termio / clears screen / prints welcome
    explicit ClientGtfs(QObject *parent = nullptr);

    // Connects to the server at 'hostname' on port 'port'
    bool startConnection(QString hostname, int port, int userTimeout);

    // Awaits user input and sends each query to the server
    void repl();

    
signals:

public slots:

private:
    QString dropoffToChar(qint8 svcDropOff);
    QString pickupToChar(qint8 svcPickUp);

    Display disp;
    QTcpSocket commSocket;
};

#endif // CLIENTGTFS_H
