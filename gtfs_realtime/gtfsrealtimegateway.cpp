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
    static RealTimeGateway *_instance = 0;
    if (_instance == 0) {
        _instance = new RealTimeGateway();
        _instance->_activeSide = DISABLED;
        _instance->_sideA      = NULL;
        _instance->_sideB      = NULL;
    }
    return *_instance;
}

void RealTimeGateway::setRealTimeFeedPath(const QString &realTimeFeedPath)
{
    // TODO : Make this more robust!
    if (realTimeFeedPath.startsWith("http://") || realTimeFeedPath.startsWith("https://")) {
        _dataPathRemote = QUrl(realTimeFeedPath);
    } else {
        _dataPathLocal = realTimeFeedPath;
    }
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

    // Download data every 2 minutes (120000 ms)
    dataTimer->start(120000);
}

void RealTimeGateway::refetchData()
{
    qDebug() << "Refetching realtime data...";

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

    // Place the data in the correct buffer
    RealTimeDataRepo currentSide = activeBuffer();
    RealTimeDataRepo nextSide    = DISABLED;
    if (currentSide == DISABLED || currentSide == SIDE_B) {
        // This **should** be safe enough. Most transactions should take < 100 ms, so nothing should be using the
        // currently-inactive side by the time the next feth occurs. Therefore we will only delete the inactive side
        // when it comes time to actually allocate into it again.
        nextSide = SIDE_A;

        if (_sideA != NULL)
            delete _sideA;

        if (!_dataPathLocal.isNull()) {
            _sideA = new RealTimeTripUpdate(_dataPathLocal);
        } else {
            _sideA = new RealTimeTripUpdate(GtfsRealTimePB);
        }
    } else if (currentSide == SIDE_A) {
        nextSide = SIDE_B;

        if (_sideB != NULL)
            delete _sideB;

        if (!_dataPathLocal.isNull()) {
            _sideB = new RealTimeTripUpdate(_dataPathLocal);
        } else {
            _sideB = new RealTimeTripUpdate(GtfsRealTimePB);
        }
    }

    if (nextSide == SIDE_A) {
        _sideA->setDownloadTimeMSec(end - start);
        setActiveFeed(SIDE_A);
    } else if (nextSide == SIDE_B) {
        _sideB->setDownloadTimeMSec(end - start);
        setActiveFeed(SIDE_B);
    }

    _nextFetchTimeUTC = QDateTime::currentDateTimeUtc().addMSecs(120000);
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
        return NULL;
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
