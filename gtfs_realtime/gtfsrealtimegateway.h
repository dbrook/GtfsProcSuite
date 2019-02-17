#ifndef GTFSREALTIMEGATEWAY_H
#define GTFSREALTIMEGATEWAY_H

#include <QObject>
#include <QThread>
#include <QMutex>

#include "gtfsrealtimefeed.h"

namespace GTFS {

// Mechanism to avoid clashes when multithreading is processing new data
typedef enum {
    DISABLED,
    SIDE_A,
    SIDE_B
} RealTimeDataRepo;

class RealTimeGateway : public QObject
{
    Q_OBJECT
public:
    // Singleton Operations
    static RealTimeGateway &inst();

    // Store the path from which to grab new protobuf data
    void setRealTimeFeedPath(const QString &realTimeFeedPath);

    // Single Fetch-and-Stop -- for debugging with local files
    void refetchData();

    // Query the active feed
    RealTimeDataRepo activeBuffer();

    // Retrieve the active feed (returns NULL if we are in the DISABLED mode)
    RealTimeTripUpdate *getActiveFeed();

signals:

public slots:

private:
    // Singleton Pattern Requirements
    explicit RealTimeGateway(QObject *parent = nullptr);
    explicit RealTimeGateway(const RealTimeGateway &);
    RealTimeGateway &operator =(RealTimeGateway const &other);
    virtual ~RealTimeGateway();

    // Dataset Members
    QMutex              _lock_activeSide;    // Prevent messing up the active side 'pointer'
    RealTimeDataRepo    _activeSide;         // Atomic indicator to denote data which is both active and ready
    QThread             _fetcherProcessor;   // Data Fetching and Processing thread
    QString             _dataPath;
    RealTimeTripUpdate *_sideA;
    RealTimeTripUpdate *_sideB;
};

} // Namespace GTFS

#endif // GTFSREALTIMEGATEWAY_H
