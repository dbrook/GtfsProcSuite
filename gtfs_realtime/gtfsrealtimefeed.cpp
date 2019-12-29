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

#include "gtfsrealtimefeed.h"

#include <QTimeZone>
#include <QDebug>
#include <fstream>

#include <google/protobuf/text_format.h>

namespace GTFS {

RealTimeTripUpdate::RealTimeTripUpdate(const QString &rtPath, QObject *parent) : QObject(parent)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    QDateTime startUTC = QDateTime::currentDateTimeUtc();

    std::fstream pbfstream(rtPath.toUtf8(), std::ios::in | std::ios::binary);
    _tripUpdate.ParseFromIstream(&pbfstream);

    // Decode it and dump to output so it can be analyzed
    std::string data;
    google::protobuf::TextFormat::PrintToString(_tripUpdate, &data);
    qDebug() << "GTFS-Realtime : LOCAL DEBUGGING MODE";
    qDebug() << "-----[ CONTENTS ]------------------------------------------------------------------------------------";
    qDebug() << data.c_str() << endl
             << "-----------------------------------------------------------------------[ END PROTOBUF CONTENTS ]-----";
    qDebug() << "Protobuf: " << _tripUpdate.ByteSize() << " bytes" << endl;
    qDebug() << "Processing _tripUpdate.entity_size() = " << _tripUpdate.entity_size() << "real-time records.";

    processUpdateDetails(startUTC);
}

RealTimeTripUpdate::RealTimeTripUpdate(const QByteArray &gtfsRealTimeData, QObject *parent) : QObject(parent)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    QDateTime startUTC = QDateTime::currentDateTimeUtc();

    _tripUpdate.ParseFromArray(gtfsRealTimeData, gtfsRealTimeData.size());

    qDebug() << "  (RTTU) GTFS-Realtime : FETCH LIVE REAL DATA";
    qDebug() << "  (RTTU) LIVE Protobuf: " << _tripUpdate.ByteSize() << " bytes";
    qDebug() << "  (RTTU) Processing _tripUpdate.entity_size() = " << _tripUpdate.entity_size() << "real-time records."
             << endl;

    processUpdateDetails(startUTC);
}

RealTimeTripUpdate::~RealTimeTripUpdate()
{
    // The default destructor **should** be enough
}

QDateTime RealTimeTripUpdate::getFeedTime() const
{
    // ISSUE: Qt Framework expects an int64 but GTFS-Realtime Protobuf returns a uint64 for the timestamp
    return QDateTime::fromSecsSinceEpoch(_tripUpdate.header().timestamp());
}

const QString RealTimeTripUpdate::getFeedGTFSVersion() const
{
    return QString::fromStdString(_tripUpdate.header().gtfs_realtime_version());
}

void RealTimeTripUpdate::setDownloadTimeMSec(qint64 downloadTime)
{
    _downloadTimeMSec = downloadTime;
}

qint64 RealTimeTripUpdate::getDownloadTimeMSec() const
{
    return _downloadTimeMSec;
}

void RealTimeTripUpdate::setIntegrationTimeMSec(qint64 integrationTime)
{
    _integrationTimeMSec = integrationTime;
}

qint64 RealTimeTripUpdate::getIntegrationTimeMSec() const
{
    return _integrationTimeMSec;
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
                                                  QMap<QString, QVector<QPair<QString, quint32>>> &addedTrips) const
{
    for (const QString &tripID : _addedTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_addedTrips[tripID]);
        QString qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        const transit_realtime::TripUpdate desc = entity.trip_update();

        for (qint32 stopTimeIdx = 0; stopTimeIdx < entity.trip_update().stop_time_update_size(); ++stopTimeIdx) {
            const transit_realtime::TripUpdate_StopTimeUpdate &stu = desc.stop_time_update(stopTimeIdx);
            const QString rtStopId = QString::fromStdString(stu.stop_id());
            if (rtStopId == stop_id) {
                QString trip_id = QString::fromStdString(desc.trip().trip_id());
                QPair<QString, quint32> routeAddedTrip(trip_id, stu.stop_sequence());
                addedTrips[qRouteId].push_back(routeAddedTrip);
            }
        }
    }
}

const QString RealTimeTripUpdate::getFinalStopIdForAddedTrip(const QString &trip_id) const
{
    qint32 recIdx = _addedTrips[trip_id];
    const transit_realtime::FeedEntity &entity = _tripUpdate.entity(recIdx);
    qint32 lastStopTimeIdx = entity.trip_update().stop_time_update_size() - 1;
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

bool RealTimeTripUpdate::tripStopActualTime(const QString   &trip_id,
                                            qint64           stopSeq,
                                            const QString   &stop_id,
                                            const QDateTime &schedArrTimeUTC,
                                            const QDateTime &schedDepTimeUTC,
                                            QDateTime       &realArrTimeUTC,
                                            QDateTime       &realDepTimeUTC) const
{
    // Assume that the trip id and stop-sequence are valid
    qint32 entityIdx;
    if (_activeTrips.contains(trip_id)) {
        entityIdx = _activeTrips[trip_id];
    } else if (_addedTrips.contains(trip_id)) {
        entityIdx = _addedTrips[trip_id];
    } else {
        return false;
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(entityIdx).trip_update();

    /*
     * Version 2 of the GTFS-Realtime updates use actual POSIX seconds-since-epoch UTC timestamps for predictions, but
     * Version 1 uses only offsets from the schedule times nor do they have any notion of stop-sequence.
     */
    if (isRTVersion1()) {
        // Version-1-specific code: can only process lateness via offsets
        for (qint32 stopTimeIdx = 0; stopTimeIdx < tri.stop_time_update_size(); ++stopTimeIdx) {
            const transit_realtime::TripUpdate_StopTimeUpdate stu = tri.stop_time_update(stopTimeIdx);
            const QString rtStopID = QString::fromStdString(stu.stop_id());
            if (rtStopID == stop_id) {
                if (!schedArrTimeUTC.isNull() && stu.arrival().has_delay())
                    realArrTimeUTC = schedArrTimeUTC.addSecs(stu.arrival().delay());
                if (!schedDepTimeUTC.isNull() && stu.departure().has_delay())
                    realDepTimeUTC = schedDepTimeUTC.addSecs(stu.departure().delay());
                return true;
            }
        }
    } else {
        // Version-2-or-higher code; this will probably be an ongoing maintenance headache, enjoy subsequent versions!
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
    }

    return false;
}

void RealTimeTripUpdate::fillStopTimesForTrip(const QString             &tripID,
                                              QString                   &route_id,
                                              QVector<rtStopTimeUpdate> &schedStopTimes,
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

    if (isRTVersion1()) {
        // Stop Times - Version 1 / using offsets ("delay" field) to determine service times
        // Must be accomplished with a lovely O(n^2) algorithm since the static/realtime coherence cannot be trusted
        for (qint32 rtStopTimeIdx = 0; rtStopTimeIdx < tri.stop_time_update_size(); ++rtStopTimeIdx) {
            // for each real-time update ...
            for (const rtStopTimeUpdate &schedSTU : schedStopTimes) {
                // ... find it's corresponding static feed counterpart and process the offsets
                rtStopTimeUpdate rtSTU;
                QString rtStopID = QString::fromStdString(tri.stop_time_update(rtStopTimeIdx).stop_id());
                if (rtStopID == schedSTU.stopID) {
                    // Compute departure/arrival times
                    if (!schedSTU.arrTime.isNull() && tri.stop_time_update(rtStopTimeIdx).has_arrival()) {
                        rtSTU.arrTime = schedSTU.arrTime.addSecs(tri.stop_time_update(rtStopTimeIdx).arrival().delay());
                    } else {
                        rtSTU.arrTime = QDateTime();
                    }
                    if (!schedSTU.depTime.isNull() && tri.stop_time_update(rtStopTimeIdx).has_departure()) {
                        rtSTU.depTime = schedSTU.depTime.addSecs(tri.stop_time_update(rtStopTimeIdx).departure().delay());
                    } else {
                        rtSTU.depTime = QDateTime();
                    }

                    // Echo back the stop ID and sequence from the static feed
                    rtSTU.stopID = schedSTU.stopID;
                    rtSTU.stopSequence = schedSTU.stopSequence;

                    // Indicate that the stop is skipped -- no idea if the V1 feeds can support this?
                    if (tri.stop_time_update(rtStopTimeIdx).schedule_relationship() ==
                        transit_realtime::TripUpdate_StopTimeUpdate_ScheduleRelationship_SKIPPED) {
                        rtSTU.stopSkipped = true;
                    } else {
                        rtSTU.stopSkipped = false;
                    }

                    // Place the stop time into the output buffer
                    stopTimes.push_back(rtSTU);
                }
            }
        }
    } else {
        // Route (not provided in Version 1)
        route_id = QString::fromStdString(tri.trip().route_id());

        // Stop Times - Version 2 / using UNIX+UTC timestamps
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

            // Indicate that the stop is skipped
            if (tri.stop_time_update(stopTimeIdx).schedule_relationship() ==
                transit_realtime::TripUpdate_StopTimeUpdate_ScheduleRelationship_SKIPPED) {
                stu.stopSkipped = true;
            } else {
                stu.stopSkipped = false;
            }

            // Save the information for this trip
            stopTimes.push_back(stu);
        }
    }
}

bool RealTimeTripUpdate::isRTVersion1() const
{
    const QString rtVersion = QString::fromStdString(_tripUpdate.header().gtfs_realtime_version());
    return rtVersion.at(0) == '1';
}

const QString RealTimeTripUpdate::getOperatingVehicle(const QString &tripID) const
{
    // We can determine which vector to loop over to fill route_id and stopTimes
    qint32 tripUpdateEntity;
    if (_addedTrips.contains(tripID))
        tripUpdateEntity = _addedTrips[tripID];
    else if (_activeTrips.contains(tripID))
        tripUpdateEntity = _activeTrips[tripID];
    else
        return "";

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    return QString::fromStdString(tri.vehicle().label());
}

void RealTimeTripUpdate::getAllTripsWithPredictions(QMap<QString, QVector<QString> > &addedRouteTrips,
                                                    QMap<QString, QVector<QString> > &activeRouteTrips,
                                                    QMap<QString, QVector<QString> > &cancelledRouteTrips) const
{
    for (const QString &tripID : _addedTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_addedTrips[tripID]);
        QString qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        addedRouteTrips[qRouteId].push_back(tripID);
    }

    for (const QString &tripID : _activeTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_activeTrips[tripID]);
        QString qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        activeRouteTrips[qRouteId].push_back(tripID);
    }

    for (const QString &tripID : _cancelledTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_cancelledTrips[tripID]);
        QString qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        cancelledRouteTrips[qRouteId].push_back(tripID);
    }
}

void RealTimeTripUpdate::processUpdateDetails(const QDateTime &startProcTimeUTC)
{
    for (qint32 recIdx = 0; recIdx < _tripUpdate.entity_size(); ++recIdx) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(recIdx);

        if (entity.trip_update().trip().schedule_relationship() ==
            transit_realtime::TripDescriptor_ScheduleRelationship_ADDED) {
            QString tripAdded = QString::fromStdString(entity.trip_update().trip().trip_id());
            _addedTrips[tripAdded] = recIdx;
        } else if (entity.trip_update().trip().schedule_relationship() ==
            transit_realtime::TripDescriptor_ScheduleRelationship_CANCELED) {
            QString tripCancelled = QString::fromStdString(entity.trip_update().trip().trip_id());
            _cancelledTrips[tripCancelled] = recIdx;
        } else {
            QString tripRealTime = QString::fromStdString(entity.trip_update().trip().trip_id());
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

    setIntegrationTimeMSec(startProcTimeUTC.msecsTo(QDateTime::currentDateTimeUtc()));
}

} // namespace GTFS
