#include "gtfsrealtimefeed.h"

#include <QDebug>

#include <QTimeZone>

#include <fstream>

namespace GTFS {

RealTimeTripUpdate::RealTimeTripUpdate(const QString &rtPath, QObject *parent) : QObject(parent)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    std::fstream pbfstream(rtPath.toUtf8(), std::ios::in | std::ios::binary);
    _tripUpdate.ParseFromIstream(&pbfstream);

    qDebug() << "Protobuf: " << _tripUpdate.ByteSize() << " bytes" << endl;

    // Will probabably want to make helper function for this, but for now let's just make it work!
    qDebug() << "Processing _tripUpdate.entity_size() = " << _tripUpdate.entity_size() << "real-time records.";

    for (qint32 recIdx = 0; recIdx < _tripUpdate.entity_size(); ++recIdx) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(recIdx);

        if (entity.trip_update().trip().schedule_relationship() ==
            transit_realtime::TripDescriptor_ScheduleRelationship_ADDED) {
            QString tripAdded = QString::fromStdString(entity.id());
            _addedTrips[tripAdded] = recIdx;
        } else if (entity.trip_update().trip().schedule_relationship() ==
            transit_realtime::TripDescriptor_ScheduleRelationship_CANCELED) {
            QString tripCancelled = QString::fromStdString(entity.id());
            _cancelledTrips[tripCancelled] = recIdx;
        } else {
            QString tripRealTime = QString::fromStdString(entity.id());
            _activeTrips[tripRealTime] = recIdx;

            // For active trips which are scheduled, there is a chance that individual stops have been removed from
            // the trip (running express / run-as-directed / etc.) so we should scan the stop_time_updates!
            for (qint32 stopTimeIdx = 0; stopTimeIdx < entity.trip_update().stop_time_update_size(); ++stopTimeIdx) {
                const transit_realtime::TripUpdate_StopTimeUpdate &stopTime = entity.trip_update().
                                                                              stop_time_update(stopTimeIdx);
                if (stopTime.schedule_relationship() ==
                    transit_realtime::TripUpdate_StopTimeUpdate_ScheduleRelationship_SKIPPED) {
                    QString qStopId = QString::fromStdString(stopTime.stop_id());
                    QPair<QString, quint32> qTripId(QString::fromStdString(entity.id()), stopTime.stop_sequence());

                    _skippedStops[qStopId].push_back(qTripId);
                }
            }
        }
    }

    qDebug() << "Added " << _addedTrips.size() << " bonus trips";
    qDebug() << "Added " << _cancelledTrips.size() << " removed trips";
    qDebug() << "Added " << _activeTrips.size() << " running trips";
    qDebug() << "Number of stop_id skipped at least once: " << _skippedStops.size();
}

RealTimeTripUpdate::~RealTimeTripUpdate()
{
    // The default constructor **should** be enough
}

QDateTime RealTimeTripUpdate::getFeedTime() const
{
    return QDateTime::fromSecsSinceEpoch(_tripUpdate.header().timestamp());
}

bool RealTimeTripUpdate::tripExists(const QString &trip_id) const
{
    if (_activeTrips.contains(trip_id)) {
        return true;
    }
    if (_addedTrips.contains(trip_id)) {
        return true;
    }
    return false;
}

bool RealTimeTripUpdate::tripIsCancelled(const QString &trip_id, const QDate &serviceDay) const
{
    if (_cancelledTrips.contains(trip_id)) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_cancelledTrips[trip_id]);
        const QString startDate = QString::fromStdString(entity.trip_update().trip().start_date());
        const QString reqServDay = serviceDay.toString("yyyyMMdd");
        if (startDate == reqServDay) {
            return true;
        }
    }
    return false;
}

void RealTimeTripUpdate::getAddedTripsServingStop(const QString &stop_id,
                                                  QMap<QString, QVector<QPair<QString, qint32>>> &addedTrips) const
{
    // TODO: This can be made more efficient by using the already-created vector of _addedTrips ???
    for (qint32 recIdx = 0; recIdx < _tripUpdate.entity_size(); ++recIdx) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(recIdx);
        QString qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        const transit_realtime::TripUpdate desc = entity.trip_update();
        transit_realtime::TripDescriptor_ScheduleRelationship tripType = desc.trip().schedule_relationship();

        // Loop on all stops if the trip is added, try to find the stop_id so we can add the route+trip to the vector
        if (tripType == transit_realtime::TripDescriptor_ScheduleRelationship_ADDED) {
            for (qint32 stopTimeIdx = 0; stopTimeIdx < entity.trip_update().stop_time_update_size(); ++stopTimeIdx) {
                const transit_realtime::TripUpdate_StopTimeUpdate &stu = desc.stop_time_update(stopTimeIdx);
                const QString rtStopId = QString::fromStdString(stu.stop_id());
                if (rtStopId == stop_id) {
                    QString trip_id = QString::fromStdString(desc.trip().trip_id());
                    QPair<QString, qint32> routeAddedTrip(trip_id, stu.stop_sequence());
                    addedTrips[qRouteId].push_back(routeAddedTrip);
                }
            }
        }
    }
}

const QString RealTimeTripUpdate::getFinalStopIdForAddedTrip(const QString &trip_id)
{
    qint64 recIdx = _addedTrips[trip_id];
    const transit_realtime::FeedEntity &entity = _tripUpdate.entity(recIdx);
    qint64 lastStopTimeIdx = entity.trip_update().stop_time_update_size() - 1;
    return QString::fromStdString(entity.trip_update().stop_time_update(lastStopTimeIdx).stop_id());
}

bool RealTimeTripUpdate::scheduledTripIsRunning(const QString &trip_id, const QDate &operDate) const
{
    if (_activeTrips.contains(trip_id)) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_activeTrips[trip_id]);
        const QString startDate = QString::fromStdString(entity.trip_update().trip().start_date());
        const QString serviceDate = operDate.toString("yyyyMMdd");
        if (startDate == serviceDate) {
            return true;
        }
    }
    return false;
}

bool RealTimeTripUpdate::tripServesStop(const QString &stop_id, const QString &trip_id, qint64 stopSeq) const
{
    /*
     * Added routes had nothing to begin with, so we cannot be sure stops normally served were specifically skipped,
     * therefore we will look through the added trips to see if we can find the stop + sequence within...
     */

    // First find the requested trip from the active trips
    if (_activeTrips.contains(trip_id)) {
        if (stopValidForTrip(_activeTrips[trip_id], stop_id, stopSeq)) {
            return true;
        }
    }

    // If we didn't find it in the active trips, maybe it was in the added trips
    if (_addedTrips.contains(trip_id)) {
        if (stopValidForTrip(_addedTrips[trip_id], stop_id, stopSeq)) {
            return true;
        }
    }
    return false;
}

bool RealTimeTripUpdate::tripSkipsStop(const QString &stop_id,
                                       const QString &trip_id,
                                       qint64         stopSeq,
                                       const QDate   &serviceDay) const
{
    /*
     * See if the stop is specifically skipped (based on a normally-schedule route)
     */
    if (_skippedStops.contains(stop_id)) {
        for (const QPair<QString, quint32> &stopList : _skippedStops[stop_id]) {
            const QString startDate =
                    QString::fromStdString(_tripUpdate.entity(_activeTrips[trip_id]).trip_update().trip().start_date());
            const QString svcDate   = serviceDay.toString("yyyyMMdd");
            if (stopList.first == trip_id && stopList.second == stopSeq && startDate == svcDate) {
                return true;
            }
        }
    }
    return false;
}

bool RealTimeTripUpdate::tripStopActualTime(const QString &trip_id, qint64 stopSeq,
                                            QDateTime &realArrTimeUTC, QDateTime &realDepTimeUTC) const
{
    // Assume that the trip id and stop-sequence are valid
    qint64 entityIdx;
    if (_activeTrips.contains(trip_id)) {
        entityIdx = _activeTrips[trip_id];
    } else if (_addedTrips.contains(trip_id)) {
        entityIdx = _addedTrips[trip_id];
    } else {
        return false;
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(entityIdx).trip_update();
    for (qint32 stopTimeIdx = 0; stopTimeIdx < tri.stop_time_update_size(); ++stopTimeIdx) {
        const transit_realtime::TripUpdate_StopTimeUpdate stu = tri.stop_time_update(stopTimeIdx);
        if (stu.stop_sequence() == stopSeq) {
            if (stu.arrival().time() != 0)
                realArrTimeUTC = QDateTime::fromSecsSinceEpoch(stu.arrival().time(), QTimeZone::utc());
            if (stu.departure().time() != 0)
                realDepTimeUTC = QDateTime::fromSecsSinceEpoch(stu.departure().time(), QTimeZone::utc());
            return true;
        }
    }
    return false;
}

void RealTimeTripUpdate::fillStopTimesForTrip(const QString             &tripID,
                                              QString                   &route_id,
                                              QVector<rtStopTimeUpdate> &stopTimes) const
{
    // We can determine which vector to loop over to fill route_id and stopTimes
    qint32 tripUpdateEntity;
    if (_addedTrips.contains(tripID))
        tripUpdateEntity = _addedTrips[tripID];
    else if (_activeTrips.contains(tripID))
        tripUpdateEntity = _activeTrips[tripID];
    else
        return;

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    // Route
    route_id = QString::fromStdString(tri.trip().route_id());

    // Stop Times
    for (qint32 stopTimeIdx = 0; stopTimeIdx < tri.stop_time_update_size(); ++stopTimeIdx) {
        rtStopTimeUpdate stu;
        stu.stopSequence = tri.stop_time_update(stopTimeIdx).stop_sequence();
        stu.stopID       = QString::fromStdString(tri.stop_time_update(stopTimeIdx).stop_id());
        if (tri.stop_time_update(stopTimeIdx).arrival().time() != 0)
            stu.arrTime = QDateTime::fromSecsSinceEpoch(tri.stop_time_update(stopTimeIdx).arrival().time(),
                                                        QTimeZone::utc());
        else
            stu.arrTime = QDateTime();
        if (tri.stop_time_update(stopTimeIdx).departure().time() != 0)
            stu.depTime = QDateTime::fromSecsSinceEpoch(tri.stop_time_update(stopTimeIdx).departure().time(),
                                                        QTimeZone::utc());
        else
            stu.depTime = QDateTime();

        stopTimes.push_back(stu);
    }
}

bool RealTimeTripUpdate::stopValidForTrip(qint32 tripUpdateEntity, const QString &stop_id, qint64 stopSequence) const
{
    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    // Loop through the stop-times of the discovered trip
    for (qint32 stopTimeIdx = 0; stopTimeIdx < tri.stop_time_update_size(); ++stopTimeIdx) {
        const transit_realtime::TripUpdate_StopTimeUpdate stu = tri.stop_time_update(stopTimeIdx);

        // TODO: This could be inefficient, we should probably make more higher-order realtime data with QString
        const QString qStopId = QString::fromStdString(stu.stop_id());

        if (stu.stop_sequence() == stopSequence && qStopId == stop_id) {
            return true;
        }
    }
    return false;
}

} // namespace GTFS
