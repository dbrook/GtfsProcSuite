#include "gtfsrealtimegateway.h"

#include <QDateTime>
#include <QDebug>

namespace GTFS {

RealTimeGateway &RealTimeGateway::inst()
{
    static RealTimeGateway *_instance = 0;
    if (_instance == 0) {
        _instance = new RealTimeGateway();
        _instance->_activeSide = DISABLED;
    }
    return *_instance;
}

void RealTimeGateway::setRealTimeFeedPath(const QString &realTimeFeedPath) {_dataPath = realTimeFeedPath;}

void RealTimeGateway::refetchData()
{
    // We want to time how long the fetch and decode took
    qint64 start = QDateTime::currentMSecsSinceEpoch();

    // Place the data in the correct buffer
    RealTimeDataRepo currentSide = activeBuffer();
    if (currentSide == DISABLED) {
        // Insert into the "A" side the first time
        _sideA = new RealTimeTripUpdate(_dataPath);
        _activeSide = SIDE_A;
    } else if (currentSide == SIDE_A) {
        // "A" side is active, so we should only play in "B"
        //TODO
    } else {
        // "B" side is active, so we should only play in "A"
        //TODO
    }

    qint64 end = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "Data Ingest took" << end - start << " milliseconds";
}

RealTimeDataRepo RealTimeGateway::activeBuffer()
{
    _lock_activeSide.lock();
    RealTimeDataRepo currentSide = _activeSide;
    _lock_activeSide.unlock();
    return currentSide;
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

quint64 RealTimeGateway::activeFeedTime()
{
    RealTimeDataRepo currentSide = activeBuffer();
    if (currentSide == SIDE_A) {
        return _sideA->getFeedTime();
    } else if (currentSide == SIDE_B) {
        return _sideB->getFeedTime();
    }
    // Not running
    return 0;
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
