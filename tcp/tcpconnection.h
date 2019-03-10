/*
 * This file comes from Bryan Cairns of VoidRealms. See http://www.voidrealms.com/
 */

#ifndef TCPCONNECTON_H
#define TCPCONNECTON_H

#include <QObject>
#include <QDebug>
#include <QTcpSocket>

class TcpConnection : public QObject
{
    Q_OBJECT
public:
    explicit TcpConnection(QObject *parent = 0);
    ~TcpConnection();

    virtual void setSocket(QTcpSocket *socket);

protected:
    QTcpSocket *m_socket;
    QTcpSocket *getSocket();

signals:

public slots:
    virtual void connected();
    virtual void disconnected();
    virtual void readyRead();
    virtual void bytesWritten(qint64 bytes);
    virtual void stateChanged(QAbstractSocket::SocketState socketState);
    virtual void error(QAbstractSocket::SocketError socketError);

};

#endif // TCPCONNECTON_H
