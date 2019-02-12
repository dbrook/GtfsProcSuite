#ifndef GTFSREALTIMEFEED_H
#define GTFSREALTIMEFEED_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QString>

// And of course have a space for the protobuf
#include "gtfs-realtime.pb.h"

namespace GTFS {

class RealTimeTripUpdate : public QObject
{
    Q_OBJECT
public:
    explicit RealTimeTripUpdate(const QString &rtPath, QObject *parent = nullptr);
    virtual ~RealTimeTripUpdate();

    /*
     * General status functions / diagnostics
     */
    // Time of feed (in seconds since UNIX epoch - UTC)
    quint64 getFeedTime() const;

    /*
     * Stop-Route-Trip-Time calculation functions
     */
    // Is the trip cancelled based on the real-time data?
    bool tripIsCancelled(const QString &trip_id) const;

    // List the trip_ids which are added for the route (populates 'addedTrips' with new-found trips)
    void getAddedTrips(const QString &route_id, QVector<QString> &addedTrips) const;

    // Is the trip (that came from the static feed) actually running?
    bool scheduledTripIsRunning(const QString &trip_id, const QString &operDateDMY) const;

    // Will the trip actually serve the stop?
    bool tripServesStop(const QString &stop_id, const QString &trip_id, qint64 stopSeq) const;

    // Was the trip-update to specifically skip the stop?
    bool tripSkipsStop(const QString &stop_id, const QString &trip_id, qint64 stopSeq) const;

    // What is the actual time of arrival? (Returns realArr/DepTime as seconds-since-UNIX-epoch in UTC - 64-bit)
    // Returns a FALSE if the trip and stopseq combination was not found in the data
    bool tripStopActualTime(const QString &trip_id, qint64 stopSeq, qint64 &realArrTime, qint64 &realDepTime) const;

signals:

public slots:

private:
    /*
     * Private Functions
     */
    // Determine if a stop-id is in the specified trip_id
    bool stopValidForTrip(qint32 tripUpdateEntity, const QString &stop_id, qint64 stopSequence) const;

    /*
     * Data Members
     */
    transit_realtime::FeedMessage   _tripUpdate;     // Hold the raw protobuf here, but it is not optimized for reading

    QMap<QString, qint32>           _cancelledTrips; // Track cancelled trips with associated entity idx
    QMap<QString, qint32>           _addedTrips;     // Trips running which do not correspond to the GTFS Static data
    QMap<QString, qint32>           _activeTrips;    // Trips running with real-time data (to replace schedule times)

    // Stop-IDs which have been skipped by any number of trips
    QMap<QString, QVector<QPair<QString, quint32>>> _skippedStops;
};

} // namespace GTFS

#endif // GTFSREALTIMEFEED_H
