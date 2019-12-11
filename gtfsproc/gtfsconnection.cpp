/*
 * This file is based on the HttpConnection class from the VoidRealms Qt Tutorials: http://www.voidrealms.com/
 */

#include "gtfsconnection.h"
#include "gtfsrequestprocessor.h"

#include <QDebug>
#include <QThreadPool>
#include <QDateTime>

GtfsConnection::GtfsConnection(QObject *parent) : TcpConnection(parent)
{
    // Start with only 2 threads so as not to blow things up
    QThreadPool::globalInstance()->setMaxThreadCount(2);
//    qDebug() << "GtfsConnection " << this << " created";
}

GtfsConnection::~GtfsConnection()
{
//    qDebug() << "GtfsConnection " << this << " destroyed";
}

void GtfsConnection::requestApplication(QString applicationRequest)
{
    // The request processor
    GtfsRequestProcessor *userRequest = new GtfsRequestProcessor(applicationRequest);
    userRequest->setAutoDelete(true);
    connect(userRequest, SIGNAL(Result(QString)), this, SLOT(taskResult(QString)), Qt::QueuedConnection);

    // Schedule for execution
    QThreadPool::globalInstance()->start(userRequest);
}

void GtfsConnection::connected()
{
//    qDebug() << "GtfsConnection " << this << " connected";
}

void GtfsConnection::disconnected()
{
//    qDebug() << "GtfsConnection " << this << " disconnected";
}

void GtfsConnection::readyRead()
{
    if (!m_socket) {
        return;
    }

//    qDebug() << "GtfsConnection " << this << " ReadyRead: " << m_socket;

    // Let's print the query sent in to the local debug
    QByteArray socketInput = m_socket->readAll();
    QString qSocketInput = socketInput;
    qDebug() << "[" << QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss t") << "] " << qSocketInput;

    this->requestApplication(qSocketInput);
}

void GtfsConnection::bytesWritten(qint64 bytes)
{
    (void)bytes;

    if (!m_socket) {
        return;
    }

//    qDebug() << "GtfsConnection " << this << " BytesWritten: " << m_socket;
}

void GtfsConnection::taskResult(QString userResponse)
{
    QByteArray dataOut = userResponse.toUtf8();
    m_socket->write(dataOut);
}

void GtfsConnection::stateChanged(QAbstractSocket::SocketState socketState)
{
    (void)socketState;
//    qDebug() << "GtfsConnection " << this << " stateChanged " << socketState;
}

void GtfsConnection::error(QAbstractSocket::SocketError socketError)
{
    qDebug() << "GtfsConnection " << this << " error " << m_socket << socketError;
}
