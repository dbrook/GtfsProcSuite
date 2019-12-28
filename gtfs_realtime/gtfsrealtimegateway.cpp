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

#include "gtfsrealtimegateway.h"

#include <QDateTime>
#include <QTimer>
#include <QEventLoop>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QDebug>

namespace GTFS {

RealTimeGateway &RealTimeGateway::inst()
{
    static RealTimeGateway *_instance = nullptr;
    if (_instance == nullptr) {
        _instance = new RealTimeGateway();
        _instance->_activeSide = DISABLED;
        _instance->_sideA      = nullptr;
        _instance->_sideB      = nullptr;
    }
    return *_instance;
}

void RealTimeGateway::setRealTimeFeedPath(const QString &realTimeFeedPath, qint32 refreshIntervalSec)
{
    // TODO : Make this more robust!
    if (realTimeFeedPath.startsWith("http://") || realTimeFeedPath.startsWith("https://")) {
        _dataPathRemote = QUrl(realTimeFeedPath);
    } else {
        _dataPathLocal = realTimeFeedPath;
    }

    _refreshIntervalSec = refreshIntervalSec;
}

qint64 RealTimeGateway::secondsToFetch() const
{
    return QDateTime::currentDateTimeUtc().secsTo(_nextFetchTimeUTC);
}

void RealTimeGateway::dataRetrievalLoop()
{
    // Setup the refetching
    QTimer *dataTimer = new QTimer();
    connect(dataTimer, SIGNAL(timeout()), SLOT(refetchData()));

    // Download interval setting
    dataTimer->start(_refreshIntervalSec * 1000);
}

void RealTimeGateway::refetchData()
{
    qDebug() << "  (RTTU) Refetching realtime data...";

    // We want to time how long the download
    qint64 start = QDateTime::currentMSecsSinceEpoch();

    // Do the download into memory if required, otherwise we are in local file mode, so skip the download
    QByteArray GtfsRealTimePB;
    if (!_dataPathRemote.isLocalFile()) {
        QNetworkAccessManager  manager;
        QNetworkReply         *response = manager.get(QNetworkRequest(_dataPathRemote));
        QEventLoop             event;
        connect(response, SIGNAL(finished()), &event, SLOT(quit()));
        event.exec();
        GtfsRealTimePB = response->readAll();
        response->deleteLater();               // Need to do this or else we leak memory from the QNetworkReply pointer
    }

    qint64 end   = QDateTime::currentMSecsSinceEpoch();

    // An empty GtfsRealTimePB is either because of an error or no data available. Either way, it's an error.
    // Simply force the active side to disabled but leave the rest alone. This should help the processor to not seek
    // any realtime information, but will also prevent existing transactions not seg-fault. When good data is found,
    // the processor will start populating SIDE_A per the logic below for non-empty data.
    // ... unless we have a local file, that is!
    if (GtfsRealTimePB.isEmpty() && _dataPathLocal.isEmpty()) {
        qDebug() << "  (RTTU) ERROR : Data feed was empty, setting active feed to DISABLED";
        setActiveFeed(DISABLED);
        return;
    }

    // Non-empty data is considered valid: place the data in the correct buffer, then activate it when ready
    try {
        RealTimeDataRepo currentSide = activeBuffer();
        RealTimeDataRepo nextSide    = DISABLED;
        if (currentSide == DISABLED || currentSide == SIDE_B) {
            // This **should** be safe enough. Most transactions should take < 250 ms, so nothing should be using the
            // currently-inactive side by the time the next fetch occurs. Therefore only deleting the inactive side
            // when it comes time to actually allocate into it again.
            nextSide = SIDE_A;

            if (_sideA != nullptr)
                delete _sideA;

            if (!_dataPathLocal.isNull()) {
                _sideA = new RealTimeTripUpdate(_dataPathLocal);
            } else {
                _sideA = new RealTimeTripUpdate(GtfsRealTimePB);
            }
        } else if (currentSide == SIDE_A) {
            nextSide = SIDE_B;

            if (_sideB != nullptr)
                delete _sideB;

            if (!_dataPathLocal.isNull()) {
                _sideB = new RealTimeTripUpdate(_dataPathLocal);
            } else {
                _sideB = new RealTimeTripUpdate(GtfsRealTimePB);
            }
        }

        // Make the switch to the next side after the data is ingested
        if (nextSide == SIDE_A) {
            _sideA->setDownloadTimeMSec(end - start);
            setActiveFeed(SIDE_A);
        } else if (nextSide == SIDE_B) {
            _sideB->setDownloadTimeMSec(end - start);
            setActiveFeed(SIDE_B);
        }
    } catch (...) {
        // If an exception is raised at any point, it should be considered the same as an empty dataset error
        qDebug() << "  (RTTU) ERROR : Exception raised while ingesting realtime data, setting active feed to DISABLED";
        setActiveFeed(DISABLED);
    }

    _nextFetchTimeUTC = QDateTime::currentDateTimeUtc().addMSecs(_refreshIntervalSec * 1000);
}

RealTimeDataRepo RealTimeGateway::activeBuffer()
{
    _lock_activeSide.lock();
    RealTimeDataRepo currentSide = _activeSide;
    _lock_activeSide.unlock();
    return currentSide;
}

void RealTimeGateway::setActiveFeed(RealTimeDataRepo nextSide)
{
    _lock_activeSide.lock();
    _activeSide = nextSide;
    _lock_activeSide.unlock();
}

RealTimeTripUpdate *RealTimeGateway::getActiveFeed()
{
    RealTimeDataRepo activeSide = activeBuffer();
    if (activeSide == SIDE_A) {
        return _sideA;
    } else if (activeSide == SIDE_B) {
        return _sideB;
    } else {
        return nullptr;
    }
}

/*
 * Singleton Pattern Requirements
 */
RealTimeGateway::RealTimeGateway(QObject *parent) :
    QObject(parent)
{
}

RealTimeGateway::~RealTimeGateway() {

}

} // namespace GTFS
