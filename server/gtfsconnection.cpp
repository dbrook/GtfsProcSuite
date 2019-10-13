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

#include "gtfsconnection.h"
#include "gtfsrequestprocessor.h"

#include <QDebug>
#include <QThreadPool>

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
    qDebug() << "User Query: " << qSocketInput;

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
//    qDebug() << "GtfsConnection " << this << " stateChanged " << socketState;
}

void GtfsConnection::error(QAbstractSocket::SocketError socketError)
{
//    qDebug() << "GtfsConnection " << this << " error " << m_socket << socketError;
}
