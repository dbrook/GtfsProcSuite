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

#include "tripstopreconciler.h"

#include <QDebug>
#include <algorithm>

namespace GTFS {

TripStopReconciler::TripStopReconciler(const QString &stop_id,
                                       bool           realTimeProcess,
                                       QDate          serviceDate,
                                       QDateTime     &currAgencyTime,
                                       qint32         futureMinutes,
                                       qint32         maxTripsForRoute,
                                       QObject       *parent)
    : QObject(parent), _realTimeMode(realTimeProcess), _svcDate(serviceDate), _stopID(stop_id),
      _lookaheadMins(futureMinutes), _maxTripsPerRoute(maxTripsForRoute), _agencyTime(currAgencyTime)
{
    // Fill the static database headers
    sStatus    = DataGateway::inst().getStatus();
    sService   = DataGateway::inst().getServiceDB();
    sStops     = DataGateway::inst().getStopsDB();
    sRoutes    = DataGateway::inst().getRoutesDB();
    sTripDB    = DataGateway::inst().getTripsDB();
    sStopTimes = DataGateway::inst().getStopTimesDB();

    // Set the current time and other time-based parameters
    // First we will get the 3 days' worth of trips so we can start narrowing them down based on additional critera
    _svcYesterday  = _svcDate.addDays(-1);
    _svcToday      = _svcDate;
    _svcTomorrow   = _svcDate.addDays(1);

    // Determine what time it is now so we can determine: a) lookahead time, b) start cutoff time, c) TripRecStat value
    _lookaheadTime = _agencyTime.addSecs(_lookaheadMins * 60);

    // Setup RealTime Process
    if (_realTimeMode) {
        rActiveFeed = GTFS::RealTimeGateway::inst().getActiveFeed();
    } else {
        rActiveFeed = NULL;
    }
}

bool TripStopReconciler::stopIdExists() const           {return sStops->contains(_stopID);}

QString TripStopReconciler::getStopName() const         {return (*sStops)[_stopID].stop_name;}

QString TripStopReconciler::getStopDesciption() const   {return (*sStops)[_stopID].stop_desc;}

void TripStopReconciler::getTripsByRoute(QMap<QString, StopRecoRouteRec> &routeTrips)
{
    // Retrieve all the trips that could service the stop (from yesterday, today, and tomorrow service days)
    for (const QString &routeID : (*sStops)[_stopID].stopTripsRoutes.keys()) {
        qDebug() << "Processing route id: " << routeID;
        StopRecoRouteRec routeRecord;
        routeRecord.longRouteName  = (*sRoutes)[routeID].route_long_name;
        routeRecord.shortRouteName = (*sRoutes)[routeID].route_short_name;
        routeRecord.routeColor     = (*sRoutes)[routeID].route_color;
        routeRecord.routeTextColor = (*sRoutes)[routeID].route_text_color;

        addTripRecordsForServiceDay(routeID, _svcYesterday, routeRecord);
        addTripRecordsForServiceDay(routeID, _svcToday,     routeRecord);
        addTripRecordsForServiceDay(routeID, _svcTomorrow,  routeRecord);

        // Put the new-found route in the overall map.
        // Pay attention later when more trips could be added, don't overwrite!
        routeTrips[routeID] = routeRecord;
    }

    // Without realtime information, expunge trips which stopped in the past / occur outside the desired time range
    // The presence of realtime information adds some complexity like added trips and new times / countdowns
    if (!_realTimeMode) {
        // STATIC MODE: Invalidate the trips so that the front-end can avoid serializing them
        for (const QString &routeID : routeTrips.keys()) {
            invalidateTrips(routeID, routeTrips);
        }
    } else {
        // REALTIME MODE: Integrate the GTFS Realtime feed information into the requested stop's trips
        // Mark any cancelled trips as such (tripStatus)
        for (const QString &routeID : routeTrips.keys())
            for (StopRecoTripRec &tripRecord : routeTrips[routeID].tripRecos)
                if (rActiveFeed->tripIsCancelled(tripRecord.tripID, tripRecord.tripServiceDate))
                {
                    tripRecord.tripStatus = CANCEL;
                    tripRecord.realTimeDataAvail = true;
                }

        // Mark trips which will skip the stop instead of serve it
        for (const QString &routeID : routeTrips.keys())
            for (StopRecoTripRec &tripRecord : routeTrips[routeID].tripRecos)
                if (rActiveFeed->tripSkipsStop(_stopID, tripRecord.tripID,
                                               tripRecord.stopSequenceNum, tripRecord.tripServiceDate))
                {
                    tripRecord.tripStatus = SKIP;
                    tripRecord.realTimeDataAvail = true;
                }

        // Inject realtime information in scheduled trips (realTimeActual, realTimeOffsetSec, waitTimeSec, tripStatus)
        for (const QString &routeID : routeTrips.keys()) {
            for (StopRecoTripRec &tripRecord : routeTrips[routeID].tripRecos) {
                if (rActiveFeed->scheduledTripIsRunning(tripRecord.tripID, tripRecord.tripServiceDate)) {
                    QDateTime predictedArr, predictedDep;
                    if ((tripRecord.tripStatus != SKIP && tripRecord.tripStatus != CANCEL) &&
                         rActiveFeed->tripStopActualTime(tripRecord.tripID, tripRecord.stopSequenceNum,
                                                         predictedArr, predictedDep))
                    {
                        if (!predictedArr.isNull())
                            tripRecord.realTimeArrival   = predictedArr.toTimeZone(_agencyTime.timeZone());
                        if (!predictedDep.isNull())
                            tripRecord.realTimeDeparture = predictedDep.toTimeZone(_agencyTime.timeZone());

                        if (!tripRecord.schArrTime.isNull() && !predictedArr.isNull()) {
                            tripRecord.realTimeOffsetSec = tripRecord.schArrTime.secsTo(predictedArr);
                            tripRecord.waitTimeSec       = _agencyTime.secsTo(predictedArr);
                        } else if (!tripRecord.schDepTime.isNull() && !predictedDep.isNull()) {
                            tripRecord.realTimeOffsetSec = tripRecord.schDepTime.secsTo(predictedDep);
                            tripRecord.waitTimeSec       = _agencyTime.secsTo(predictedDep);
                        }

                        // Time-based statuses (arriving/early/late/on-time)
                        if (tripRecord.realTimeOffsetSec < -59)
                            tripRecord.tripStatus = EARLY;
                        else if (tripRecord.realTimeOffsetSec > 59)
                            tripRecord.tripStatus = LATE;
                        else
                            tripRecord.tripStatus = ON_TIME;

                        // Trip has an arrival time, so we can mark a trip as arriving withing 30 seconds
                        if (!predictedArr.isNull() && _agencyTime.secsTo(predictedArr) < 30)
                            tripRecord.tripStatus = ARRIVE;

                        // Trip has already departed but still shows up in the feed
                        if (!predictedDep.isNull() && _agencyTime > predictedDep)
                            tripRecord.tripStatus = DEPART;

                        // Trip is boarding (current time is between the drop-off and pickup - requires both times))
                        if (!predictedArr.isNull() && !predictedDep.isNull()) {
                            if (_agencyTime >= predictedArr && _agencyTime < predictedDep)
                                tripRecord.tripStatus = BOARD;
                        }

                        tripRecord.realTimeDataAvail = true;
                    } else {
                        // No time was found for the stop in this trip! There are two ways this is possible:
                        //  1) The trip passed the stop early and we should not display it
                        //  2) The trip is far enough in the future that the realtime information for the entire trip
                        //       is not populated by the data provider
                        //
                        // For #1, just expunge the trip from the arrival information
                        // For #2, don't want to remove a future scheduled trip!
                        //
                        // Essentially, this means there should be a cutoff interval to do this removal. For the MBTA
                        // (the only reference platform so far) it seems to be times beyond 2 hours in the future.
                        // For a general case, assume 1 hour (3600 seconds) is the cutoff
                        if ((!tripRecord.schArrTime.isNull() && _agencyTime.secsTo(tripRecord.schArrTime) < 3600) ||
                            (!tripRecord.schDepTime.isNull() && _agencyTime.secsTo(tripRecord.schDepTime) < 3600)) {
                            tripRecord.tripStatus = IRRELEVANT;
                        }
                    }
                } // End of active (real-time) trip handling condition
            } // End of loop on trips
        } // End of loop on routes

        // Add the 'added' trips (mark with SUPPLEMENT)
        // We first fill a separate compatible data structure which will then merge into the vector of trip-records
        QMap<QString, QVector<QPair<QString, qint32>>> addedTrips;
        rActiveFeed->getAddedTripsServingStop(_stopID, addedTrips);
        for (const QString &routeID : addedTrips.keys()) {
            for (const QPair<QString, qint32> &tripAndIndex : addedTrips[routeID]) {
                // Make a new tripRecord so we have a place to add current trip information
                StopRecoTripRec tripRecord;
                tripRecord.tripStatus = SUPPLEMENT;
                tripRecord.realTimeDataAvail = true;

                // We cannot reliably map information of a real-time-only trip
                tripRecord.beginningOfTrip = false;
                tripRecord.endOfTrip       = false;
                tripRecord.dropoffType     = 0;
                tripRecord.pickupType      = 0;
                tripRecord.tripID          = tripAndIndex.first;

                // Calculate wait time and actual departure/arrivals if available
                QDateTime prArrTime, prDepTime;
                if (rActiveFeed->tripStopActualTime(tripAndIndex.first, tripAndIndex.second, prArrTime, prDepTime)) {
                    // We have to have at least one time
                    if (!prDepTime.isNull()) {
                        tripRecord.realTimeDeparture = prDepTime.toTimeZone(_agencyTime.timeZone());

                        // Fill in the countdown until departure initially ...
                        tripRecord.waitTimeSec = _agencyTime.secsTo(tripRecord.realTimeDeparture);
                    }
                    if (!prArrTime.isNull()) {
                        tripRecord.realTimeArrival = prArrTime.toTimeZone(_agencyTime.timeZone());

                        // ... but we will always prefer the countdown time to show the time until vehicle arrival
                        tripRecord.waitTimeSec = _agencyTime.secsTo(tripRecord.realTimeArrival);
                    }
                }

                // Trip has an arrival time, so we can mark a trip as arriving withing 30 seconds
                if (!prArrTime.isNull() && _agencyTime.secsTo(prArrTime) < 30)
                    tripRecord.tripStatus = ARRIVE;

                // Trip has already departed but still shows up in the feed
                if (!prDepTime.isNull() && _agencyTime > prDepTime)
                    tripRecord.tripStatus = DEPART;

                // Trip is boarding (current time is between the drop-off and pickup - requires both times))
                if (!prArrTime.isNull() && !prDepTime.isNull()) {
                    if (_agencyTime >= prArrTime && _agencyTime < prDepTime)
                        tripRecord.tripStatus = BOARD;
                }

                // Dumb hack to figure out where the train is going (will not properly match the "headsign" field!)
                const QString finalStopID = rActiveFeed->getFinalStopIdForAddedTrip(tripRecord.tripID);
                qDebug() << "Final Stop for tripID " << tripRecord.tripID << " is: " << finalStopID;
                tripRecord.headsign = (*sStops)[finalStopID].stop_name;

                routeTrips[routeID].tripRecos.push_back(tripRecord);
            }
        }

        // Sort the vector by the service time (order of importance: arrival time THEN departure time)
        // RealTime trips may have their actual time modified by so much that the scheduling order breaks down.
        // This is much more common on some systems/routes than others. As this application is primarily used to show
        // countdown / wait timers, we will prefer sorting based on actual times and adjust the output as needed.
        for (const QString &routeID : routeTrips.keys())
            std::sort(routeTrips[routeID].tripRecos.begin(),
                      routeTrips[routeID].tripRecos.end(),
                      TripStopReconciler::arrivalThenDepartureSortRealTime);

        // Invalidate trips which fall outside of the requested parameters (_maxTripsPerRoute or _lookaheadTime)
        for (const QString &routeID : routeTrips.keys())
            invalidateTrips(routeID, routeTrips);
    }
}

void TripStopReconciler::addTripRecordsForServiceDay(const QString    &routeID,
                                                     const QDate      &serviceDay,
                                                     StopRecoRouteRec &routeRecord) const
{
    // Go through all the trips from the static feed and find all that hit this stop
    for (qint32 tripIdx = 0; tripIdx < (*sStops)[_stopID].stopTripsRoutes[routeID].length(); ++tripIdx)
    {
        // Offset into the individual stop-trip array (so the entire trip information can be found)
        qint32        stopTripIdx  = (*sStops)[_stopID].stopTripsRoutes[routeID].at(tripIdx).tripStopIndex;
        const QString curTripId    = (*sStops)[_stopID].stopTripsRoutes[routeID].at(tripIdx).tripID;

        // Ensure that the trip actually runs for this service day
        if (! sService->serviceRunning(serviceDay, (*sTripDB)[curTripId].service_id))
            continue;

        // Populate the trip-record with all the pertinent / necessary details
        StopRecoTripRec tripRec;
        tripRec.tripID          = curTripId;
        tripRec.stopSequenceNum = (*sStopTimes)[curTripId].at(stopTripIdx).stop_sequence;
        tripRec.beginningOfTrip = (stopTripIdx == 0) ? true : false;
        tripRec.endOfTrip       = (stopTripIdx == (*sStopTimes)[curTripId].length() - 1) ? true : false;
        tripRec.dropoffType     = (*sStopTimes)[curTripId].at(stopTripIdx).drop_off_type;
        tripRec.pickupType      = (*sStopTimes)[curTripId].at(stopTripIdx).pickup_type;
        tripRec.headsign        = ((*sStopTimes)[curTripId].at(stopTripIdx).stop_headsign != "")
                                                            ? (*sStopTimes)[curTripId].at(stopTripIdx).stop_headsign
                                                            : (*sTripDB)[curTripId].trip_headsign;
        tripRec.stopTimesIndex  = stopTripIdx;
        tripRec.tripServiceDate = serviceDay;
        tripRec.waitTimeSec     = 0;

        // The schedule times are always offset from the local noon (to handle DST fluctuations)
        bool scheduleTimeAvail     = false;
        QDateTime localNoon        = QDateTime(serviceDay, QTime(12, 0, 0), _agencyTime.timeZone());
        if ((*sStopTimes)[curTripId].at(stopTripIdx).departure_time != -1) {
            tripRec.schDepTime  = localNoon.addSecs((*sStopTimes)[curTripId].at(stopTripIdx).departure_time);
            tripRec.waitTimeSec = _agencyTime.secsTo(tripRec.schDepTime);
            scheduleTimeAvail   = true;
        } else {
            tripRec.schDepTime  = QDateTime();
        }
        if ((*sStopTimes)[curTripId].at(stopTripIdx).arrival_time != -1) {
            tripRec.schArrTime  = localNoon.addSecs((*sStopTimes)[curTripId].at(stopTripIdx).arrival_time);
            // NOTE: Prefer the arrival time for the wait-time calculations, so that's process arr. after dep.!
            tripRec.waitTimeSec = _agencyTime.secsTo(tripRec.schArrTime);
            scheduleTimeAvail   = true;
        } else {
            tripRec.schArrTime  = QDateTime();
        }

        // There is neither a departure nor arrival time from which to countdown
        // Some stops aren't timed at all, so the "next possible time" is used (called the sort time)
        if ((*sStopTimes)[curTripId].at(stopTripIdx).arrival_time == -1 &&
            (*sStopTimes)[curTripId].at(stopTripIdx).departure_time == -1) {
            QDateTime localNoon(tripRec.tripServiceDate, QTime(12, 0, 0), _agencyTime.timeZone());
            tripRec.schSortTime = localNoon.addSecs((*sStops)[_stopID].stopTripsRoutes[routeID].at(tripIdx).sortTime);
            tripRec.waitTimeSec = _agencyTime.secsTo(tripRec.schSortTime);
            scheduleTimeAvail   = false;
        }

        // Trip status will always start as "SCHEDULE" because all other values are based on real-time processing
        tripRec.tripStatus = scheduleTimeAvail ? SCHEDULE : NOSCHEDULE;

        // Also we won't start out with real-time availability from a static schedule stop-time
        tripRec.realTimeDataAvail = false;

        // Add the trip to the trips-for-route
        routeRecord.tripRecos.push_back(tripRec);
    }
}

void TripStopReconciler::invalidateTrips(const QString &routeID, QMap<QString, StopRecoRouteRec> &routeTrips)
{
    qint32 nbTripsForRoute = 0;
    for (StopRecoTripRec &tripRecord : routeTrips[routeID].tripRecos) {
        QDateTime stopTime;
        if (!tripRecord.schArrTime.isNull()) {
            stopTime = tripRecord.schArrTime;
        } else if (!tripRecord.schDepTime.isNull()) {
            stopTime = tripRecord.schDepTime;
        }

        // Mark the trip-stop as invalid if it occurred in the past
        // There are two notions: scheduled and unscheduled times. Unscheduled times should NOT display a time
        // but we allow a countdown (probably the client should warn that the data is missing). If realtime
        // data were to be associated with these 'untimed' stops, then hopefully that would supplement it :) )
        if ((tripRecord.tripStatus == SCHEDULE   && _agencyTime > stopTime) ||
            (tripRecord.tripStatus == NOSCHEDULE && _agencyTime > tripRecord.schSortTime)) {
            tripRecord.tripStatus = IRRELEVANT;
        }

        // With realtime data available, don't "irrelevant-ify" skipped stops (might be interesting to show)
        // We won't get rid of trips immediately, let them live for a few minutes before expunging from view.
        else if (tripRecord.realTimeDataAvail) {
            if (tripRecord.tripStatus == ON_TIME || tripRecord.tripStatus == LATE   ||
                tripRecord.tripStatus == EARLY   || tripRecord.tripStatus == DEPART ||
                tripRecord.tripStatus == BOARD   || tripRecord.tripStatus == ARRIVE ||
                tripRecord.tripStatus == SUPPLEMENT) {
                if (!tripRecord.realTimeArrival.isNull() &&
                    ((_agencyTime.secsTo(tripRecord.realTimeArrival) < -60) ||
                     (_lookaheadMins != 0 && tripRecord.realTimeArrival > _lookaheadTime))) {
                    tripRecord.tripStatus = IRRELEVANT;
                } else if (!tripRecord.realTimeDeparture.isNull() &&
                         ((_agencyTime.secsTo(tripRecord.realTimeDeparture) < -60) ||
                          (_lookaheadMins != 0 && tripRecord.realTimeDeparture > _lookaheadTime))) {
                    tripRecord.tripStatus = IRRELEVANT;
                }
            }

            // The excpetion is for cancelled trips, which can show for 15 minutes past the scheduled departure
            if (tripRecord.tripStatus == CANCEL)
                if (_agencyTime.secsTo(tripRecord.realTimeDeparture) > -900)
                    tripRecord.tripStatus = IRRELEVANT;
        }

        // If we didn't mark the trip as irrelevant, then it shall count against the maxTripsForRoute
        if (tripRecord.tripStatus != IRRELEVANT)
            ++nbTripsForRoute;

        // Mark trips as invalid if they are outside of the number of trips requested
        if (_maxTripsPerRoute != 0 && nbTripsForRoute > _maxTripsPerRoute) {
            tripRecord.tripStatus = IRRELEVANT;
        }

        // Mark trips as invalid if they are outside the time window requested
        if (_lookaheadMins != 0 &&
            ((tripRecord.tripStatus == SCHEDULE   && stopTime               > _lookaheadTime) ||
             (tripRecord.tripStatus == NOSCHEDULE && tripRecord.schSortTime > _lookaheadTime)   )) {
            tripRecord.tripStatus = IRRELEVANT;
        }
    }
}

bool TripStopReconciler::arrivalThenDepartureSortRealTime(StopRecoTripRec &left, StopRecoTripRec &right)
{
    // There can be several notions of time on which we could sort depending on the amount of data available
    QDateTime leftSort;
    QDateTime rightSort;

    // Because the primary purpose of the countdown display is arrival times, we will always try to prefer them
    if (left.tripStatus == SCHEDULE || left.tripStatus == CANCEL  ||
        left.tripStatus == SKIP     || left.tripStatus == IRRELEVANT) {
        if (!left.schArrTime.isNull())
            leftSort = left.schArrTime;
        else if (!left.schDepTime.isNull())
            leftSort = left.schDepTime;
    } else if (left.tripStatus == NOSCHEDULE) {
        leftSort = left.schSortTime;
    } else {
        // Everything else should have real time data available for sorting
        // Give preference for arrival time, but for trips that begin, it might be unreliable, so fallback to departure
        if (!left.realTimeArrival.isNull())
            leftSort = left.realTimeArrival;
        else
            leftSort = left.realTimeDeparture;
    }

    if (right.tripStatus == SCHEDULE || right.tripStatus == CANCEL  ||
        right.tripStatus == SKIP     || right.tripStatus == IRRELEVANT) {
        if (!right.schArrTime.isNull())
            rightSort = right.schArrTime;
        else if (!right.schDepTime.isNull())
            rightSort = right.schDepTime;
    } else if (right.tripStatus == NOSCHEDULE) {
        rightSort = right.schSortTime;
    } else {
        // Everything else should have real time data available for sorting
        // Give preference for arrival time, but for trips that begin, it might be unreliable, so fallback to departure
        if (!right.realTimeArrival.isNull())
            rightSort = right.realTimeArrival;
        else
            rightSort = right.realTimeDeparture;
    }

    return leftSort < rightSort;
}

} // Namespace GTFS
