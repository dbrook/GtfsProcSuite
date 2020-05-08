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
#include <google/protobuf/util/json_util.h>

namespace GTFS {

RealTimeTripUpdate::RealTimeTripUpdate(const QString &rtPath,
                                       bool           dumpProtobuf,
                                       rtDateLevel    skipDateMatching,
                                       bool           propagateOffsetSec,
                                       QObject        *parent)
    : QObject(parent), _dateEnforcement(skipDateMatching), _extrapolateOffset(propagateOffsetSec)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    QDateTime startUTC = QDateTime::currentDateTimeUtc();

    std::fstream pbfstream(rtPath.toUtf8(), std::ios::in | std::ios::binary);
    _tripUpdate.ParseFromIstream(&pbfstream);

    if (dumpProtobuf) {
        showProtobufData();
    }

    processUpdateDetails(startUTC);
}

RealTimeTripUpdate::RealTimeTripUpdate(const QByteArray &gtfsRealTimeData,
                                       bool              dumpProtobuf,
                                       rtDateLevel skipDateMatching,
                                       bool              propagateOffsetSec,
                                       QObject          *parent)
    : QObject(parent), _dateEnforcement(skipDateMatching), _extrapolateOffset(propagateOffsetSec)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    QDateTime startUTC = QDateTime::currentDateTimeUtc();

    _tripUpdate.ParseFromArray(gtfsRealTimeData, gtfsRealTimeData.size());

    qDebug() << "  (RTTU) GTFS-Realtime : LIVE Protobuf: " << _tripUpdate.ByteSize()
             << "bytes consisting of" << _tripUpdate.entity_size() << "real-time records.";

    if (dumpProtobuf) {
        showProtobufData();
    }

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

quint64 RealTimeTripUpdate::getFeedTimePOSIX() const
{
    return _tripUpdate.header().timestamp();
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

bool RealTimeTripUpdate::tripIsCancelled(const QString &trip_id,
                                         const QDate   &serviceDate,
                                         const QDate   &actualDate) const
{
    if (_cancelledTrips.contains(trip_id)) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_cancelledTrips[trip_id]);
        const QString startDate = QString::fromStdString(entity.trip_update().trip().start_date());
        const QString serviceDateStr = serviceDate.toString("yyyyMMdd");
        const QString actualDateStr  = actualDate.toString("yyyyMMdd");
        if ((_dateEnforcement == NO_MATCHING) ||
            (_dateEnforcement == SERVICE_DATE && startDate == serviceDateStr) ||
            (_dateEnforcement == ACTUAL_DATE  && startDate == actualDateStr)) {
            return true;
        }
    }
    return false;
}

void RealTimeTripUpdate::getAddedTripsServingStop(const QString &stop_id,
                                                  QHash<QString, QVector<QPair<QString, quint32>>> &addedTrips) const
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

const QString RealTimeTripUpdate::getRouteID(const QString &trip_id, const QDate &serviceDay) const
{
    QString routeIDrt;
    qint32  tripUpdateEntity;
    bool    isSupplementalTrip;
    if (!findEntityIndex(trip_id, tripUpdateEntity, isSupplementalTrip)) {
        return routeIDrt;
    }

    // TODO: THE DATE NEEDS TO BE HANDLED?
    //       sit on this longer ... given the SEPTA and RTC Southern Nevada agencies using no start date/time in their
    //       real time trip IDs, this is more complicated than I thought

    routeIDrt = QString::fromStdString(_tripUpdate.entity(tripUpdateEntity).trip_update().trip().route_id());
    return routeIDrt;
}

bool RealTimeTripUpdate::scheduledTripIsRunning(const QString &trip_id,
                                                const QDate   &serviceDate,
                                                const QDate   &actualDate) const
{
    if (_activeTrips.contains(trip_id)) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_activeTrips[trip_id]);

        const QString rtStartDateStr = QString::fromStdString(entity.trip_update().trip().start_date());
        const QString serviceDateStr = serviceDate.toString("yyyyMMdd");
        const QString actualDateStr  = actualDate.toString("yyyyMMdd");

        /*
         * If no date matching is requested: sure, this trip counts as running, no further check required
         *
         * When matching the actual date, trips are considered based on the actual local time they started (the
         * agency opted to not use the greater-than-24-hour clock for after-midnight real-time trip updates)
         *
         * Strictest date matching (the default) means the real-time start date is that of the service date (which,
         * for after-midnight trips (times >= 24:00:00) is technically the day before)
         */
        if ((_dateEnforcement == NO_MATCHING) ||
            (_dateEnforcement == SERVICE_DATE && rtStartDateStr == serviceDateStr) ||
            (_dateEnforcement == ACTUAL_DATE  && rtStartDateStr == actualDateStr)) {
            return true;
        }
    }
    return false;
}

bool RealTimeTripUpdate::tripSkipsStop(const QString &stop_id,
                                       const QString &trip_id,
                                       qint64         stopSeq,
                                       const QDate   &serviceDate,
                                       const QDate   &actualDate) const
{
    /*
     * See if the stop is specifically skipped (based on a normally-schedule route)
     */
    if (_skippedStops.contains(stop_id)) {
        const QString serviceDateStr = serviceDate.toString("yyyyMMdd");
        const QString actualDateStr  = actualDate.toString("yyyyMMdd");
        for (const QPair<QString, quint32> &stopList : _skippedStops[stop_id]) {
            const QString rtStartDateStr =
                    QString::fromStdString(_tripUpdate.entity(_activeTrips[trip_id]).trip_update().trip().start_date());
            if (stopList.first  == trip_id &&
                stopList.second == stopSeq &&
                ((_dateEnforcement == NO_MATCHING) ||
                 (_dateEnforcement == SERVICE_DATE && rtStartDateStr == serviceDateStr) ||
                 (_dateEnforcement == ACTUAL_DATE  && rtStartDateStr == actualDateStr))) {
                return true;
            }
        }
    }
    return false;
}

bool RealTimeTripUpdate::scheduledTripAlreadyPassed(const QString              &trip_id,
                                                    qint64                      stopSeq,
                                                    const QVector<StopTimeRec> &tripTimes) const
{
    /*
     * We are assured from the GTFS specification that stop sequence numbers will ALWAYS increase, so if the first
     * stop sequence we find in the realtime feed is higher than that of the one requested by stopSeq, we should
     * note that the trip has already passed through the stop and therefore not display it
     */
    qint32 entityIdx;

    if (_activeTrips.contains(trip_id)) {
        entityIdx = _activeTrips[trip_id];
    } else {
        return false;
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(entityIdx).trip_update();

    if (tri.stop_time_update_size() > 0) {
        if (tri.stop_time_update(0).has_stop_sequence()) {
            /*
             * If there is a stop sequence present then if the stop being investigated has a lower stop sequence than
             * the first sequence in the trip update then we assume it has already passed the stop.
             */
            if (tri.stop_time_update(0).stop_sequence() > stopSeq) {
                return true;
            }
        } else {
            /*
             * Otherwise see if the first stop ID in the realtime trip update is after the stop ID requested, if it is,
             * then the trip has already passed the stop ID requested
             */
            for (const StopTimeRec &stopTimeInfo : tripTimes) {
                if (QString::fromStdString(tri.stop_time_update(0).stop_id()) == stopTimeInfo.stop_id) {
                    return false;
                }
            }
            return true;                // If the stop was never found in the active feed then it must have been passed
        }
    }

    // Empty trips either haven't been filled in yet or have terminated; it is not immediately obvious so return false
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
    // Determine that the trip id and stop-sequence are valid
    qint32 tripUpdateEntity;
    bool   isSupplementalTrip;
    if (!findEntityIndex(trip_id, tripUpdateEntity, isSupplementalTrip)) {
        return false;
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    // Conventional flow (when trip update has multiple stops encoded)
    // Either the realtime update is in POSIX-style seconds-since-UNIX-epoch UTC timestamps OR just offset-seconds
    for (qint32 stopTimeIdx = 0; stopTimeIdx < tri.stop_time_update_size(); ++stopTimeIdx) {
        const transit_realtime::TripUpdate_StopTimeUpdate &stu = tri.stop_time_update(stopTimeIdx);

        /*
         * If stop sequence is available, it MUST be followed (this handles the case where a trip ID can be visited
         * more than once in a trip), a stop_id-only style match can only be done if no stop sequence is present
         */
        if ((stu.has_stop_sequence() && stu.stop_sequence() == stopSeq) ||
            (!stu.has_stop_sequence() && stu.has_stop_id() && QString::fromStdString(stu.stop_id()) == stop_id)) {
            fillPredictedTime(stu, schedArrTimeUTC, schedDepTimeUTC, realArrTimeUTC, realDepTimeUTC);
            return true;
        }
    }

    /*
     * If no exact match for the stop id or sequence was found, the server might have been configured to extrapolate
     * whatever the last-known offset is present into the rest of the trip ID
     */
    if (_extrapolateOffset) {
        // Apply the last-known offsets to the scheduled time if possible
        const transit_realtime::TripUpdate_StopTimeUpdate &stu = tri.stop_time_update(tri.stop_time_update_size() - 1);
        fillPredictedTime(stu, schedArrTimeUTC, schedDepTimeUTC, realArrTimeUTC, realDepTimeUTC);
    }

    // Exhausted the dataset with no success, indicate nothing could be filled
    return false;
}

void RealTimeTripUpdate::fillPredictedTime(const transit_realtime::TripUpdate_StopTimeUpdate &stu,
                                           const QDateTime                                   &schedArrTimeUTC,
                                           const QDateTime                                   &schedDepTimeUTC,
                                           QDateTime                                         &realArrTimeUTC,
                                           QDateTime                                         &realDepTimeUTC) const
{
    realArrTimeUTC = QDateTime();
    realDepTimeUTC = QDateTime();

    if (stu.has_arrival()) {
        if (stu.arrival().has_delay() && !schedArrTimeUTC.isNull()) {
            realArrTimeUTC = schedArrTimeUTC.addSecs(stu.arrival().delay());
        } else if (stu.arrival().has_time()) {
            realArrTimeUTC = QDateTime::fromSecsSinceEpoch(stu.arrival().time(), QTimeZone::utc());
        }
    }

    if (stu.has_departure()) {
        if (stu.departure().has_delay() && !schedDepTimeUTC.isNull()) {
            realDepTimeUTC = schedDepTimeUTC.addSecs(stu.departure().delay());
        } else if (stu.departure().has_time()) {
            realDepTimeUTC = QDateTime::fromSecsSinceEpoch(stu.departure().time(), QTimeZone::utc());
        }
    }
}

void RealTimeTripUpdate::fillStopTimesForTrip(const QString              &tripID,
                                              const QTimeZone            &agencyTZ,
                                              const QVector<StopTimeRec> &tripTimes,
                                              QVector<rtStopTimeUpdate>  &rtStopTimes) const
{
    // We can determine which vector to loop over to fill route_id and stopTimes
    qint32 tripUpdateEntity;
    bool   isSupplementalTrip;
    if (!findEntityIndex(tripID, tripUpdateEntity, isSupplementalTrip)) {
        return;
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    // Determine the service date of the trip based on the realtime start date information
    // This is needed for purely-offset-based trip updates since the offset must be computed to an actual DateTime
    QString decodedSvcDate = QString::fromStdString(tri.trip().start_date());
    QDate rtServiceDate(decodedSvcDate.mid(0, 4).toInt(),
                        decodedSvcDate.mid(4, 2).toInt(),
                        decodedSvcDate.mid(6, 2).toInt());
    QDate feedActualDate = QDateTime::fromSecsSinceEpoch(_tripUpdate.header().timestamp()).toUTC().date();
    QDate preferredDate  = rtServiceDate.isValid() ? rtServiceDate : feedActualDate;
    QDateTime localNoon  = QDateTime(preferredDate, QTime(12, 0, 0), agencyTZ);

    for (qint32 stopTimeIdx = 0; stopTimeIdx < tri.stop_time_update_size(); ++stopTimeIdx) {
        rtStopTimeUpdate stu;
        stu.stopID = QString::fromStdString(tri.stop_time_update(stopTimeIdx).stop_id());

        /*
         * In the event BOTH a stop sequence and stop id are present in the realtime feed, the stop sequence is
         * preferred over a lone stop_id (because some trips could visit the same stop_id several times).
         */
        if (tri.stop_time_update(stopTimeIdx).has_stop_sequence()) {
            QDateTime schArrTime;
            QDateTime schDepTime;

            if (!isSupplementalTrip) {
                // Given a stop sequence try and find the scheduled times from the static feed (supplemental trips N/A)
                for (const StopTimeRec &stopRec : tripTimes) {
                    if (stopRec.stop_sequence == tri.stop_time_update(stopTimeIdx).stop_sequence()) {
                        schArrTime = localNoon.addSecs(stopRec.arrival_time);
                        schDepTime = localNoon.addSecs(stopRec.departure_time);
                        break;
                    }
                }
            }

            stu.stopSequence = tri.stop_time_update(stopTimeIdx).stop_sequence();
            fillPredictedTime(tri.stop_time_update(stopTimeIdx), schArrTime, schDepTime, stu.arrTime, stu.depTime);
        } else {
            QDateTime schArrTime;
            QDateTime schDepTime;

            if (!isSupplementalTrip) {
                // Given a stop sequence try and find the scheduled times from the static feed (supplemental trips N/A)
                for (const StopTimeRec &stopRec : tripTimes) {
                    if (stopRec.stop_id == QString::fromStdString(tri.stop_time_update(stopTimeIdx).stop_id())) {
                        schArrTime = localNoon.addSecs(stopRec.arrival_time);
                        schDepTime = localNoon.addSecs(stopRec.departure_time);
                        stu.stopSequence = stopRec.stop_sequence;
                        break;
                    }
                }
            }

            fillPredictedTime(tri.stop_time_update(stopTimeIdx), schArrTime, schDepTime, stu.arrTime, stu.depTime);
        }

        stu.stopSkipped = tri.stop_time_update(stopTimeIdx).schedule_relationship() ==
                          transit_realtime::TripUpdate_StopTimeUpdate_ScheduleRelationship_SKIPPED;

        // Save the information for this trip
        rtStopTimes.push_back(stu);
    }
}

const QString RealTimeTripUpdate::getOperatingVehicle(const QString &tripID) const
{
    qint32 tripUpdateEntity;
    bool   isSupplementalTrip;
    if (!findEntityIndex(tripID, tripUpdateEntity, isSupplementalTrip)) {
        return "";
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();
    return QString::fromStdString(tri.vehicle().label());
}

const QString RealTimeTripUpdate::getTripStartTime(const QString &tripID) const
{
    qint32 tripUpdateEntity;
    bool   isSupplementalTrip;
    if (!findEntityIndex(tripID, tripUpdateEntity, isSupplementalTrip)) {
        return "";
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();
    return QString::fromStdString(tri.trip().start_time());
}

const QString RealTimeTripUpdate::getTripStartDate(const QString &tripID) const
{
    qint32 tripUpdateEntity;
    bool   isSupplementalTrip;
    if (!findEntityIndex(tripID, tripUpdateEntity, isSupplementalTrip)) {
        return "";
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();
    return QString::fromStdString(tri.trip().start_date());
}

void RealTimeTripUpdate::getAllTripsWithPredictions(QHash<QString, QVector<QString> > &addedRouteTrips,
                                                    QHash<QString, QVector<QString> > &activeRouteTrips,
                                                    QHash<QString, QVector<QString> > &cancelledRouteTrips) const
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

void RealTimeTripUpdate::serializeTripUpdates(QString &output) const
{
    std::string data;
//    google::protobuf::TextFormat::PrintToString(_tripUpdate, &data);
    google::protobuf::util::MessageToJsonString(_tripUpdate, &data);

    output = QString::fromStdString(data);
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

void RealTimeTripUpdate::showProtobufData() const
{
    std::string data;
    google::protobuf::TextFormat::PrintToString(_tripUpdate, &data);
    qDebug() << "GTFS-Realtime : LOCAL DEBUGGING MODE";
    qDebug() << "-----[ CONTENTS ]------------------------------------------------------------------------------------";
    qDebug() << data.c_str() << endl
             << "-----------------------------------------------------------------------[ END PROTOBUF CONTENTS ]-----";
    qDebug() << "Protobuf: " << _tripUpdate.ByteSize() << " bytes" << endl;
    qDebug() << "Processing _tripUpdate.entity_size() = " << _tripUpdate.entity_size() << "real-time records.";
}

bool RealTimeTripUpdate::findEntityIndex(const QString &tripID, qint32 &entityIdx, bool &isSupplemental) const
{
    if (_addedTrips.contains(tripID)) {
        entityIdx      = _addedTrips[tripID];
        isSupplemental = true;
        return true;
    } else if (_activeTrips.contains(tripID)) {
        entityIdx      = _activeTrips[tripID];
        isSupplemental = false;
        return true;
    }
    return false;
}

} // namespace GTFS
