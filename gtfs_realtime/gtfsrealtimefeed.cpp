/*
 * GtfsProc_Server
 * Copyright (C) 2018-2022, Daniel Brook
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
#include <QSet>
#include <QDebug>
#include <fstream>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

namespace GTFS {

RealTimeTripUpdate::RealTimeTripUpdate(const QString      &rtPath,
                                       bool                dumpProtobuf,
                                       rtDateLevel         skipDateMatching,
                                       bool                loosenStopSeqEnf,
                                       const TripData     *tripsDB,
                                       const StopTimeData *stopTimeDB,
                                       QObject            *parent)
    : QObject(parent),
      _tripDB(tripsDB),
      _stopTimeDB(stopTimeDB),
      _dateEnforcement(skipDateMatching),
      _loosenStopSeqEnf(loosenStopSeqEnf)
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

RealTimeTripUpdate::RealTimeTripUpdate(const QByteArray   &gtfsRealTimeData,
                                       bool                dumpProtobuf,
                                       rtDateLevel         skipDateMatching,
                                       bool                loosenStopSeqEnf,
                                       bool                displayBufferInfo,
                                       const TripData     *tripsDB,
                                       const StopTimeData *stopTimeDB,
                                       QObject            *parent)
    : QObject(parent),
      _tripDB(tripsDB),
      _stopTimeDB(stopTimeDB),
      _dateEnforcement(skipDateMatching),
      _loosenStopSeqEnf(loosenStopSeqEnf)
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    QDateTime startUTC = QDateTime::currentDateTimeUtc();

    _tripUpdate.ParseFromArray(gtfsRealTimeData, gtfsRealTimeData.size());

    if (displayBufferInfo) {
        qDebug() << "  (RTTU) GTFS-Realtime : LIVE Protobuf: " << _tripUpdate.ByteSizeLong()
                 << "bytes consisting of" << _tripUpdate.entity_size() << "real-time records.";
    }

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
    // Another problem, an empty buffer will have been initialized with 0 seconds-since-epoch ... we should really
    // be NULL in that case and not 1970-01-01 00:00:00 UTC
    if (_tripUpdate.header().timestamp() == 0) {
        return QDateTime();
    } else {
        return QDateTime::fromSecsSinceEpoch(_tripUpdate.header().timestamp());
    }
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

const QString RealTimeTripUpdate::getRouteID(const QString &trip_id) const
{
    QString routeIDrt;
    qint32  tripUpdateEntity;
    bool    isSupplementalTrip;
    if (!findEntityIndex(trip_id, tripUpdateEntity, isSupplementalTrip)) {
        return routeIDrt;
    }

    routeIDrt = QString::fromStdString(_tripUpdate.entity(tripUpdateEntity).trip_update().trip().route_id());
    return routeIDrt;
}

bool RealTimeTripUpdate::scheduledTripIsRunning(const QString &trip_id,
                                                const QDate   &serviceDate,
                                                const QDate   &actualDate,
                                                QDate         &rtDateUsed) const
{
    bool tripIsRunning = false;

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
            rtDateUsed    = serviceDate;
            tripIsRunning = true;
        }
    }

    return tripIsRunning;
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
            if ((_dateEnforcement == NO_MATCHING) ||
                (_dateEnforcement == SERVICE_DATE && rtStartDateStr == serviceDateStr) ||
                (_dateEnforcement == ACTUAL_DATE  && rtStartDateStr == actualDateStr)) {
                if (!_loosenStopSeqEnf) {
                    if (stopList.first == trip_id && stopList.second == stopSeq) return true;
                } else {
                    if (stopList.first == trip_id) return true;
                }
            }
        }
    }
    return false;
}

bool RealTimeTripUpdate::scheduledTripAlreadyPassed(const QString &trip_id, qint64 stopSeq) const
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
        if (!_loosenStopSeqEnf && tri.stop_time_update(0).has_stop_sequence()) {
            /*
             * If there is a stop sequence present (and the loosener has not been applied for this backend), then we
             * have made the assumption that the agency updates are correctly filled and it is enough to just check the
             * first stop sequence in the update is greater than that of the static feed's
             */
            if (tri.stop_time_update(0).stop_sequence() > stopSeq) {
                return true;
            }
        }
    }

    // Empty trips either haven't been filled in yet or have terminated; it is not immediately obvious (since trip-stop
    // sequence matching is not possible), so we will have to go onto fill the entire trip up and and determine based
    // on system times alone if the trip has actually passed.
    return false;
}

void RealTimeTripUpdate::tripStopActualTime(const QString              &tripID,
                                            qint64                      stopSeq,
                                            const QString              &stop_id,
                                            const QTimeZone            &agencyTZ,
                                            const QVector<StopTimeRec> &tripTimes,
                                            const QDate                &serviceDate,
                                            QDateTime                  &realArrTimeUTC,
                                            QDateTime                  &realDepTimeUTC) const
{
    /*
     * STEP 1) Find the trip update from the realtime feed
     */
    qint32 tripUpdateEntity;
    bool   isSupplementalTrip;
    if (!findEntityIndex(tripID, tripUpdateEntity, isSupplementalTrip)) {
        return;
    }
    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    /*
     * STEP 2) Trip Update has POSIX timestamps: no need to fill entire trip with predictions with offset extrapolation
     *         Find the matching stop in the realtime feed so the times may be filled
     */
    qint32 rtSTUpd;
    for (rtSTUpd = 0; rtSTUpd < tri.stop_time_update_size(); ++rtSTUpd) {
        // Despite the specification indicating otherwise, it is possible that stop sequences do not match the static
        // schedule. So this is put in place to ensure that the sequence is in line. Should there be a mismatch, then
        // a different check (based ONLY on the stop ID) must be attempted. It is not guaranteed to be "as good",
        // especially if a trip visits the same stop more than once.
        // Discovered with CTTransit data, but Google Maps rendering seems to figure it out whereas GtfsProc didn't.
        // (for quality assurance, these kinds of trips will still be considered mimatches as they violate spec)
        QString stopIdRT = QString::fromStdString(tri.stop_time_update(rtSTUpd).stop_id());
        if (tri.stop_time_update(rtSTUpd).has_stop_sequence() &&
            stopSeq == tri.stop_time_update(rtSTUpd).stop_sequence()) {
            // Match Stop ID and Stop Sequence when both are provided
            break;
        } else if ((!tri.stop_time_update(rtSTUpd).has_stop_sequence() || _loosenStopSeqEnf) && stop_id == stopIdRT) {
            // Trip Update has no stop sequence information, just match the stop ID
            break;
        }
    }

    if (rtSTUpd < tri.stop_time_update_size()) {
        if ((tri.stop_time_update(rtSTUpd).has_arrival()   && tri.stop_time_update(rtSTUpd).arrival().has_time()) ||
            (tri.stop_time_update(rtSTUpd).has_departure() && tri.stop_time_update(rtSTUpd).departure().has_time())) {
            QDateTime arrTimeDummy, depTimeDummy;
            QChar     arrBaseDummy, depBaseDummy;
            fillPredictedTime(tri.stop_time_update(rtSTUpd),
                              arrTimeDummy,   depTimeDummy,
                              realArrTimeUTC, realDepTimeUTC,
                              arrBaseDummy,   depBaseDummy);
            return;
        }
    }

    /*
     * STEP 3) It was not possible to match an exact sequence or stop ID. Trip may have offsets that can propagate
     *         for a running trip to the rest of its stop IDs. Use the RTR transactions stop-times-for-trip function
     *         to determine the date/times based on those offsets if possible.
     */
    QVector<rtStopTimeUpdate> rtStopTimes;
    fillStopTimesForTrip(TRIPID_RECONCILE, -1, tripID, agencyTZ, serviceDate, tripTimes, rtStopTimes);

    // Pick out the stop time relevant to the requested stop and return it
    for (const rtStopTimeUpdate &rtst : qAsConst(rtStopTimes)) {
        if (((rtst.stopSequence != -1) && (rtst.stopSequence == stopSeq) && (rtst.stopID == stop_id)) ||
             (rtst.stopID == stop_id)) {
            realArrTimeUTC = rtst.arrTime;
            realDepTimeUTC = rtst.depTime;
            break;
        }
    }
}

void RealTimeTripUpdate::fillPredictedTime(const transit_realtime::TripUpdate_StopTimeUpdate &stu,
                                           const QDateTime                                   &schedArrTimeUTC,
                                           const QDateTime                                   &schedDepTimeUTC,
                                           QDateTime                                         &realArrTimeUTC,
                                           QDateTime                                         &realDepTimeUTC,
                                           QChar                                             &realArrBased,
                                           QChar                                             &realDepBased) const
{
    realArrTimeUTC = QDateTime();
    realDepTimeUTC = QDateTime();

    if (stu.has_arrival()) {
        if (stu.arrival().has_delay() && !schedArrTimeUTC.isNull()) {
            realArrTimeUTC = schedArrTimeUTC.addSecs(stu.arrival().delay());
            realArrBased = 'O';

            // If the arrival offset is known but the departure is NOT, then extrapolate the arrival's delay to this
            // stop's departure time, too - this should almost always be the case
            if (!stu.has_departure() && !stu.departure().has_delay() && !schedDepTimeUTC.isNull()) {
                realDepTimeUTC = schedDepTimeUTC.addSecs(stu.arrival().delay());
                realDepBased = 'E';
            }
        } else if (stu.arrival().has_time()) {
            realArrTimeUTC = QDateTime::fromSecsSinceEpoch(stu.arrival().time(), QTimeZone::utc());
            realArrBased = 'P';
        }
    }

    if (stu.has_departure()) {
        if (stu.departure().has_delay() && !schedDepTimeUTC.isNull()) {
            realDepTimeUTC = schedDepTimeUTC.addSecs(stu.departure().delay());
            realDepBased = 'O';
        } else if (stu.departure().has_time()) {
            realDepTimeUTC = QDateTime::fromSecsSinceEpoch(stu.departure().time(), QTimeZone::utc());
            realDepBased = 'P';
        }
    }
}

void RealTimeTripUpdate::fillStopTimesForTrip(rtUpdateMatch               realTimeMatch,
                                              quint64                     rttuIdx,
                                              const QString              &tripID,
                                              const QTimeZone            &agencyTZ,
                                              const QDate                &serviceDate,
                                              const QVector<StopTimeRec> &tripTimes,
                                              QVector<rtStopTimeUpdate>  &rtStopTimes) const
{
    // We can determine which vector to loop over to fill route_id and stopTimes
    qint32 tripUpdateEntity;
    bool   supplementalStyle;
    if (realTimeMatch == TRIPID_RECONCILE || realTimeMatch == TRIPID_FEED_ONLY) {
        if (!findEntityIndex(tripID, tripUpdateEntity, supplementalStyle)) {
            return;
        }
        if (realTimeMatch == TRIPID_FEED_ONLY) {
            supplementalStyle = true;
        }
    } else {
        tripUpdateEntity  = rttuIdx;
        supplementalStyle = true;
    }

    // Ensure we are in the bound limits of the trip updates
    if (tripUpdateEntity >= _tripUpdate.entity_size()) {
        return;
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    QDateTime localNoon = QDateTime(serviceDate, QTime(12, 0, 0), agencyTZ);

    // If this is not a supplemental trip, then we can match 1-to-1 the offsets or POSIX timestamps to the trip and
    // render the entire thing, showing the schedule vs. prediction for each stop along the trip
    if (!supplementalStyle) {
        bool   tripUsesOffset  = false;
        qint32 lastKnownOffset = 0;
        for (const StopTimeRec &stopRec : tripTimes) {
            rtStopTimeUpdate stu;
            stu.stopSequence = -1;

            // Find the first real-time trip update that pertains to the schedule
            qint32 stUpdIdx;
            for (stUpdIdx = 0; stUpdIdx < tri.stop_time_update_size(); ++stUpdIdx) {
                // Stop-Sequences are preferred per the GTFS-Realtime specification, even if a stop_id is also present
                // (should also enforce that the stop id matches the trip update contents)
                QString stopIdRT = QString::fromStdString(tri.stop_time_update(stUpdIdx).stop_id());
                if (tri.stop_time_update(stUpdIdx).has_stop_sequence() &&
                    static_cast<quint32>(stopRec.stop_sequence) == tri.stop_time_update(stUpdIdx).stop_sequence()) {
                    stu.stopSequence = stopRec.stop_sequence;
                    break;
                } else if ((!tri.stop_time_update(stUpdIdx).has_stop_sequence() || _loosenStopSeqEnf) &&
                           (stopRec.stop_id == stopIdRT)) {
                    // Fill stop sequence from static feed, this could help clients debug in case wrong sequence/id
                    // matched when using the _loosenStopSeqEnf option has been requested.
                    stu.stopSequence = stopRec.stop_sequence;
                    break;
                }
            }

            // Fill in the stop ID
            stu.stopID = stopRec.stop_id;

            // Retrieve scheduled time for storage
            QDateTime schArrTime = localNoon.addSecs(stopRec.arrival_time);
            QDateTime schDepTime = localNoon.addSecs(stopRec.departure_time);

            // With the stop/sequence matched, fill in the matched time (POSIX-style or offset) directly
            if (stUpdIdx < tri.stop_time_update_size()) {
                // Determine if offset-in-seconds are used at all, save once it is found in case it needs extrapolation
                // For the purposes of propagation to the remaining itinerary, the DEPARTURE should be preferred
                if (tri.stop_time_update(stUpdIdx).has_arrival() &&
                    tri.stop_time_update(stUpdIdx).arrival().has_delay()) {
                    lastKnownOffset = tri.stop_time_update(stUpdIdx).arrival().delay();
                    tripUsesOffset  = true;
                }
                if (tri.stop_time_update(stUpdIdx).has_departure() &&
                    tri.stop_time_update(stUpdIdx).departure().has_delay()) {
                    lastKnownOffset = tri.stop_time_update(stUpdIdx).departure().delay();
                    tripUsesOffset  = true;
                }


                // Fill the known offsets for this matched stop
                fillPredictedTime(tri.stop_time_update(stUpdIdx), schArrTime, schDepTime,
                                  stu.arrTime, stu.depTime, stu.arrBased, stu.depBased);

                // The stop may be skipped
                stu.stopSkipped = tri.stop_time_update(stUpdIdx).schedule_relationship() ==
                                  transit_realtime::TripUpdate_StopTimeUpdate_ScheduleRelationship_SKIPPED;
            }

            // But if no match was found AND offset-in-seconds is used, try and fill with what we had before
            else {
                if (tripUsesOffset) {
                    // Try and fill in based on the last-known offsets
                    stu.arrBased     = 'E';
                    stu.depBased     = 'E';
                    stu.arrTime      = schArrTime.addSecs(lastKnownOffset);
                    stu.depTime      = schDepTime.addSecs(lastKnownOffset);
                    stu.stopSkipped  = false;
                } else {
                    // Only POSIX-times were used, so we cannot extrapolate offsets to the rest of the trip
                    stu.arrBased     = 'N';
                    stu.depBased     = 'N';
                    stu.arrTime      = QDateTime();
                    stu.depTime      = QDateTime();
                    stu.stopSkipped  = false;
                }
            }

            // Save the information for this trip
            rtStopTimes.push_back(stu);
        }
    }

    // Otherwise, supplemental trips are far less "interesting" to show since they have no schedule to compare, thus
    // it is probably wisest to simply dump the entire thing out and flag them as all POSIX-based (since they must be?)
    else {
        rtStopTimeUpdate stu;
        for (qint32 stUpdIdx = 0; stUpdIdx < tri.stop_time_update_size(); ++stUpdIdx) {
            if (tri.stop_time_update(stUpdIdx).has_stop_sequence()) {
                stu.stopSequence = tri.stop_time_update(stUpdIdx).stop_sequence();
            }
            if (tri.stop_time_update(stUpdIdx).has_stop_id()) {
                stu.stopID = QString::fromStdString(tri.stop_time_update(stUpdIdx).stop_id());
            }

            // Get arrival/departure times based on POSIX timestamps
            QDateTime schArrTime, schDepTime;
            fillPredictedTime(tri.stop_time_update(stUpdIdx),
                              schArrTime, schDepTime,
                              stu.arrTime, stu.depTime,
                              stu.arrBased, stu.depBased);

            stu.stopSkipped = tri.stop_time_update(stUpdIdx).schedule_relationship() ==
                              transit_realtime::TripUpdate_StopTimeUpdate_ScheduleRelationship_SKIPPED;

            // Save the information for this trip
            rtStopTimes.push_back(stu);
        }
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

void RealTimeTripUpdate::getAllTripsWithPredictions(QHash<QString, QVector<QString>> &addedRouteTrips,
                                                    QHash<QString, QVector<QString>> &activeRouteTrips,
                                                    QHash<QString, QVector<QString>> &cancelledRouteTrips,
                                                    QHash<QString, QVector<QString>> &mismatchRTTrips,
                                                    QHash<QString, QHash<QString, QVector<qint32>>> &duplicateRTTrips,
                                                    QVector<QString> &tripsWithoutRoutes) const
{
    for (const QString &tripID : _addedTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_addedTrips[tripID]);
        QString qRouteId;
        if (entity.trip_update().trip().has_route_id()) {
            qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        } else if (_tripDB->contains(tripID)) {
            qRouteId = (*_tripDB)[tripID].route_id;
        }
        addedRouteTrips[qRouteId].push_back(tripID);
    }

    for (const QString &tripID : _activeTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_activeTrips[tripID]);
        QString qRouteId;
        if (entity.trip_update().trip().has_route_id()) {
            qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        } else if (_tripDB->contains(tripID)) {
            qRouteId = (*_tripDB)[tripID].route_id;
        }
        activeRouteTrips[qRouteId].push_back(tripID);
    }

    for (const QString &tripID : _cancelledTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_cancelledTrips[tripID]);
        QString qRouteId;
        if (entity.trip_update().trip().has_route_id()) {
            qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        } else if (_tripDB->contains(tripID)) {
            qRouteId = (*_tripDB)[tripID].route_id;
        }
        cancelledRouteTrips[qRouteId].push_back(tripID);
    }

    for (const QString &tripID : _duplicateTrips.keys()) {
        for (qint32 rttuIdx : _duplicateTrips[tripID]) {
            const transit_realtime::FeedEntity &entity = _tripUpdate.entity(rttuIdx);
            QString qRouteId = "";
            if (entity.trip_update().trip().has_route_id()) {
                qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
            } else if (_tripDB->contains(tripID)) {
                qRouteId = (*_tripDB)[tripID].route_id;
            }
            duplicateRTTrips[qRouteId][tripID].push_back(rttuIdx);
        }
    }

    mismatchRTTrips    = _stopsMismatchTrips;

    tripsWithoutRoutes = _noRouteTrips;
}

void RealTimeTripUpdate::getActiveTripsForRouteID(const QString &routeID, QVector<QString> &tripsForRoute) const
{
    for (const QString &tripID : _addedTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_addedTrips[tripID]);
        QString qRouteId;
        if (entity.trip_update().trip().has_route_id()) {
            qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        } else if (_tripDB->contains(tripID)) {
            qRouteId = (*_tripDB)[tripID].route_id;
        }
        if (routeID == qRouteId) {
            tripsForRoute.push_back(tripID);
        }
    }

    for (const QString &tripID : _activeTrips.keys()) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_activeTrips[tripID]);
        QString qRouteId;
        if (entity.trip_update().trip().has_route_id()) {
            qRouteId = QString::fromStdString(entity.trip_update().trip().route_id());
        } else if (_tripDB->contains(tripID)) {
            qRouteId = (*_tripDB)[tripID].route_id;
        }
        if (routeID == qRouteId) {
            tripsForRoute.push_back(tripID);
        }
    }
}

QString RealTimeTripUpdate::getNextStopIDInPrediction(const QString &tripID) const
{
    qint32 tripUpdateEntity;
    bool   isSupplementalTrip;
    if (!findEntityIndex(tripID, tripUpdateEntity, isSupplementalTrip)) {
        return "?";
    }

    const transit_realtime::TripUpdate &tri = _tripUpdate.entity(tripUpdateEntity).trip_update();

    if (tri.stop_time_update_size() == 0) {
        return "!";
    }

    return QString::fromStdString(tri.stop_time_update(0).stop_id());
}

void RealTimeTripUpdate::serializeTripUpdates(QString &output) const
{
    std::string data;
//    google::protobuf::TextFormat::PrintToString(_tripUpdate, &data);
    google::protobuf::util::MessageToJsonString(_tripUpdate, &data);

    output = QString::fromStdString(data);
}

quint64 RealTimeTripUpdate::getNbEntities() const
{
    return _tripUpdate.entity_size();
}

QString RealTimeTripUpdate::getTripIdFromEntity(quint64 realtimeTripUpdateEntity) const
{
    if (realtimeTripUpdateEntity >= this->getNbEntities()) {
        return "";
    }
    return QString::fromStdString(_tripUpdate.entity(realtimeTripUpdateEntity).trip_update().trip().trip_id());
}

bool RealTimeTripUpdate::getLoosenStopSeqEnf() const
{
    return _loosenStopSeqEnf;
}

void RealTimeTripUpdate::processUpdateDetails(const QDateTime &startProcTimeUTC)
{
    // Process every trip update entity
    for (qint32 recIdx = 0; recIdx < _tripUpdate.entity_size(); ++recIdx) {
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(recIdx);

        // Each time a trip is attempted to be added, see if it was already added and save it's trip ID + route ID
        QString tripID = QString::fromStdString(entity.trip_update().trip().trip_id());
        if (_addedTrips.contains(tripID)) {
            if (_duplicateTrips[tripID].empty()) {
                // First discovery of a duplicate ... so add the original index comprising the duplicate
                _duplicateTrips[tripID].push_back(_addedTrips[tripID]);
            }
            _duplicateTrips[tripID].push_back(recIdx);
        }

        if (_cancelledTrips.contains(tripID)) {
            if (_duplicateTrips[tripID].empty()) {
                // First discovery of a duplicate ... so add the original index comprising the duplicate
                _duplicateTrips[tripID].push_back(_cancelledTrips[tripID]);
            }
            _duplicateTrips[tripID].push_back(recIdx);
        }

        if (_activeTrips.contains(tripID)) {
            if (_duplicateTrips[tripID].empty()) {
                // First discovery of a duplicate ... so add the original index comprising the duplicate
                _duplicateTrips[tripID].push_back(_activeTrips[tripID]);
            }
            _duplicateTrips[tripID].push_back(recIdx);
        }

        // Store a trip which does not contain the route information, or has a bad route ID
        if (!entity.trip_update().trip().has_route_id() && !_tripDB->contains(tripID)) {
            _noRouteTrips.push_back(tripID);
        } else if (entity.trip_update().trip().has_route_id() && _tripDB->contains(tripID)) {
            const QString routeFromTrip   = QString::fromStdString(entity.trip_update().trip().route_id());
            const QString routeFromUpdate = (*_tripDB)[tripID].route_id;
            if (!routeFromUpdate.isEmpty() && routeFromTrip != routeFromUpdate) {
                _noRouteTrips.push_back(tripID);
            }
        }

        // Put the trip update into its relevant category
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

    // Post-Process the activeTrips and determine if unexpected stop_sequence or stop_ids are present
    for (const QString &tripID : _activeTrips.keys()) {
        // Make a set of all stop_sequences and stop_ids for the trip from the static feed
        QSet<qint64>  staticSequnces;
        QSet<QString> staticStopIDs;
        if (_stopTimeDB->contains(tripID)) {
            for (const StopTimeRec &stopTimeItem : (*_stopTimeDB)[tripID]) {
                staticSequnces.insert(stopTimeItem.stop_sequence);
                staticStopIDs.insert(stopTimeItem.stop_id);
            }
        }

        // The RPS transaction will do a further breakdown per route, so let's to the breakdown here
        // upon reading and not at every request.
        const transit_realtime::FeedEntity &entity = _tripUpdate.entity(_activeTrips[tripID]);
        QString routeID = (*_tripDB)[tripID].route_id;

        // If any sequences or stops in the real-time buffer are not in the static, flag this trip as mismatching
        const transit_realtime::TripUpdate &tri = entity.trip_update();
        for (qint32 stopTimeIdx = 0; stopTimeIdx < tri.stop_time_update_size(); ++stopTimeIdx) {
            if (tri.stop_time_update(stopTimeIdx).has_stop_sequence()) {
                if (!staticSequnces.contains(tri.stop_time_update(stopTimeIdx).stop_sequence())) {
                    // Save the real-time trip-update's index, then move to the next
                    _stopsMismatchTrips[routeID].push_back(tripID);
                    break;
                }
            } else if (tri.stop_time_update(stopTimeIdx).has_stop_id()) {
                if (!staticStopIDs.contains(QString::fromStdString(tri.stop_time_update(stopTimeIdx).stop_id()))) {
                    // Save the real-time trip-update's index, then move to the next
                    _stopsMismatchTrips[routeID].push_back(tripID);
                    break;
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
    qDebug() << data.c_str() << Qt::endl
             << "-----------------------------------------------------------------------[ END PROTOBUF CONTENTS ]-----";
    qDebug() << "Protobuf: " << _tripUpdate.ByteSizeLong() << " bytes" << Qt::endl;
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
