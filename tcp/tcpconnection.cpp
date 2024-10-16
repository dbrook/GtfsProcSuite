/*
 * This file comes from Bryan Cairns of VoidRealms. See http://www.voidrealms.com/
 */

#include "tcpconnection.h"

TcpConnection::TcpConnection(QObject *parent) : QObject(parent)
{
//    qDebug() << this << "Created";
}

TcpConnection::~TcpConnection()
{
//    qDebug() << this << "Destroyed";
}

void TcpConnection::setSocket(QTcpSocket *socket)
{
    m_socket = socket;
    connect(m_socket,&QTcpSocket::connected, this, &TcpConnection::connected);
    connect(m_socket,&QTcpSocket::disconnected, this, &TcpConnection::disconnected);
    connect(m_socket,&QTcpSocket::readyRead, this, &TcpConnection::readyRead);
    connect(m_socket,&QTcpSocket::bytesWritten, this, &TcpConnection::bytesWritten);
    connect(m_socket,&QTcpSocket::stateChanged, this, &TcpConnection::stateChanged);
    connect(m_socket,
            static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::errorOccurred),
            this, &TcpConnection::error);
}

QTcpSocket *TcpConnection::getSocket()
{
    if(!sender()) return nullptr;
    return static_cast<QTcpSocket*>(sender());
}

void TcpConnection::connected()
{
    if(!sender()) return;
//    qDebug() << this << " connected " << sender();
}

void TcpConnection::disconnected()
{
    if(!sender()) return;
//    qDebug() << this << " disconnected " << getSocket();
}

void TcpConnection::readyRead()
{
    if(!sender()) return;
//    qDebug() << this << " readyRead " << getSocket();
    QTcpSocket *socket = getSocket();
    if(!socket) return;
    socket->close();
}

void TcpConnection::bytesWritten(qint64 bytes)
{
    (void)bytes;

    if(!sender()) return;
//    qDebug() << this << " bytesWritten " << getSocket() << " number of bytes = " << bytes;
}

void TcpConnection::stateChanged(QAbstractSocket::SocketState socketState)
{
    (void)socketState;

    if(!sender()) return;
//    qDebug() << this << " stateChanged " << getSocket() << " state = " << socketState;
}

void TcpConnection::error(QAbstractSocket::SocketError socketError)
{
    (void)socketError;

    if(!sender()) return;
//    qDebug() << this << " error " << getSocket() << " error = " << socketError;
}

