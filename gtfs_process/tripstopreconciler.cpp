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

#include "tripstopreconciler.h"

#include <QDebug>
#include <algorithm>

namespace GTFS {

TripStopReconciler::TripStopReconciler(const QList<QString>     &stop_ids,
                                       bool                      realTimeProcess,
                                       QDate                     serviceDate,
                                       const QDateTime          &currAgencyTime,
                                       qint32                    futureMinutes,
                                       qint32                    maxTripsForRoute,
                                       const Status             *status,
                                       const OperatingDay       *services,
                                       const StopData           *stopDB,
                                       const RouteData          *routeDB,
                                       const TripData           *tripDB,
                                       const StopTimeData       *stopTimeDB,
                                       const RealTimeTripUpdate *activeFeed,
                                       QObject                  *parent)
    : QObject(parent), _realTimeMode(realTimeProcess), _svcDate(serviceDate), _stopIDs(stop_ids),
      _lookaheadMins(futureMinutes), _maxTripsPerRoute(maxTripsForRoute), _agencyTime(currAgencyTime),
      sStatus(status), sService(services), sStops(stopDB), sRoutes(routeDB), sTripDB(tripDB), sStopTimes(stopTimeDB),
      rActiveFeed(activeFeed)
{
    // Set the current time and other time-based parameters
    // First we will get the 3 days' worth of trips so we can start narrowing them down based on additional critera
    _svcYesterday  = _svcDate.addDays(-1);
    _svcToday      = _svcDate;
    _svcTomorrow   = _svcDate.addDays(1);

    // Determine what time it is now so we can determine: a) lookahead time, b) start cutoff time, c) TripRecStat value
    _lookaheadTime = _agencyTime.addSecs(_lookaheadMins * 60);
}

bool TripStopReconciler::stopIdExists() const
{
    for (const QString &stopID : _stopIDs) {
        if (!sStops->contains(stopID)) {
            return false;
        }
    }
    return true;
}

QString TripStopReconciler::getStopName() const
{
    if (_stopIDs.size() == 1) {
        return (*sStops)[_stopIDs.at(0)].stop_name;
    }

    QString concatStopNames;
    for (const QString &stopID : _stopIDs) {
        concatStopNames += (*sStops)[stopID].stop_name + " | ";
    }
    return concatStopNames;
}

QString TripStopReconciler::getStopDesciption() const
{
    if (_stopIDs.size() == 1) {
        return (*sStops)[_stopIDs.at(0)].stop_desc;
    } else {
        return "Multiple Stops";
    }
}

void TripStopReconciler::getTripsByRoute(QMap<QString, StopRecoRouteRec> &routeTrips)
{
    // Trips will be built locally per route per stop ID, then invalidated based on the timeframe or number of stops
    // requested. Only the "relevant" trips will be stored in routeTrips for the JSON-building services to use.
    QMap<QString, StopRecoRouteRec> fullTrips;

    // For every stop requested, find all relevant trips
    for (const QString &stopID : _stopIDs) {

    // Retrieve all the trips that could service the stop (from yesterday, today, and tomorrow service days)
    for (const QString &routeID : (*sStops)[stopID].stopTripsRoutes.keys()) {
        StopRecoRouteRec &routeRecord = routeTrips[routeID];
        routeRecord.longRouteName  = (*sRoutes)[routeID].route_long_name;
        routeRecord.shortRouteName = (*sRoutes)[routeID].route_short_name;
        routeRecord.routeColor     = (*sRoutes)[routeID].route_color;
        routeRecord.routeTextColor = (*sRoutes)[routeID].route_text_color;

        StopRecoRouteRec &fullRouteRecord = fullTrips[routeID];
        addTripRecordsForServiceDay(routeID, _svcYesterday, stopID, fullRouteRecord);
        addTripRecordsForServiceDay(routeID, _svcToday,     stopID, fullRouteRecord);
        addTripRecordsForServiceDay(routeID, _svcTomorrow,  stopID, fullRouteRecord);
    }

    /*
     * REALTIME MODE: Integrate the GTFS Realtime feed information into the requested stop's trips
     * Without realtime information, expunge trips which stopped in the past / occur outside the desired time range
     * The presence of realtime information adds some complexity like added trips and new times / countdowns
     */
    if (_realTimeMode) {
        // Mark any cancelled trips as such (tripStatus)
        for (const QString &routeID : fullTrips.keys()) {
            for (StopRecoTripRec &tripRecord : fullTrips[routeID].tripRecos) {
                if (rActiveFeed->tripIsCancelled(tripRecord.tripID, tripRecord.tripServiceDate)) {
                    tripRecord.tripStatus = CANCEL;
                    tripRecord.realTimeDataAvail = true;
                    tripRecord.supplementalTrip  = false;
                }
            }
        }

        // Mark trips which will skip the stop instead of serve it
        for (const QString &routeID : fullTrips.keys()) {
            for (StopRecoTripRec &tripRecord : fullTrips[routeID].tripRecos) {
                if (rActiveFeed->tripSkipsStop(stopID, tripRecord.tripID,
                                               tripRecord.stopSequenceNum, tripRecord.tripServiceDate)) {
                    tripRecord.tripStatus = SKIP;
                    tripRecord.realTimeDataAvail = true;
                    tripRecord.supplementalTrip  = false;
                }
            }
        }

        // Inject realtime information in scheduled trips (realTimeActual, realTimeOffsetSec, waitTimeSec, tripStatus)
        for (const QString &routeID : fullTrips.keys()) {
            for (StopRecoTripRec &tripRecord : fullTrips[routeID].tripRecos) {
                if (rActiveFeed->scheduledTripIsRunning(tripRecord.tripID, tripRecord.tripServiceDate)) {
                    // Scheduled trip arrival/departure converted to UTC for offset calculation standardization
                    // TODO: this might be unnecessary, but for the sake of all the subsequent time maths, it makes
                    //       more sense to leave the real-time library as "UTC-only" (for now at least)
                    QDateTime schedArrUTC = tripRecord.schArrTime.toUTC();
                    QDateTime schedDepUTC = tripRecord.schDepTime.toUTC();

                    // Parameters filled by the "actual time" service (returned in UTC)
                    QDateTime predictedArr = QDateTime();
                    QDateTime predictedDep = QDateTime();
                    if (tripRecord.tripStatus != SKIP && tripRecord.tripStatus != CANCEL) {
                        if (rActiveFeed->scheduledTripAlreadyPassed(tripRecord.tripID,
                                                                    tripRecord.stopSequenceNum,
                                                                    (*sStopTimes)[tripRecord.tripID])) {
                            // A scheduled trip with realtime data might have left the stop in question early so we
                            // need to account for that and expunge the trip from displaying. This is not needed for
                            // addred trips (as there is nothing to compare them to).
                            tripRecord.tripStatus = IRRELEVANT;
                            continue;
                        }

                        tripRecord.supplementalTrip = false;

                        // It is possible that a trip only has partial real-time information, and this stop isn't
                        // covered by it even though it is on the trip's static schedule and NOT explicitly skipped.
                        rActiveFeed->tripStopActualTime(tripRecord.tripID,
                                                        tripRecord.stopSequenceNum,
                                                        tripRecord.stopID,
                                                        schedArrUTC,
                                                        schedDepUTC,
                                                        predictedArr,
                                                        predictedDep);

                        bool rtMissingForStop   = true;
                        bool rtArrivalMissing   = true;
                        bool rtDepartureMissing = true;

                        if (!predictedArr.isNull()) {
                            tripRecord.realTimeArrival = predictedArr.toTimeZone(_agencyTime.timeZone());
                            rtArrivalMissing           = false;
                        }
                        if (!predictedDep.isNull()) {
                            tripRecord.realTimeDeparture = predictedDep.toTimeZone(_agencyTime.timeZone());
                            rtDepartureMissing           = false;
                        }

                        tripRecord.realTimeOffsetSec = 0;
                        if (!tripRecord.schArrTime.isNull() && !predictedArr.isNull()) {
                            rtMissingForStop = false;
                            tripRecord.realTimeOffsetSec = tripRecord.schArrTime.secsTo(predictedArr);
                            tripRecord.waitTimeSec       = _agencyTime.secsTo(predictedArr);
                        } else if (!tripRecord.schDepTime.isNull() && !predictedDep.isNull()) {
                            rtMissingForStop = false;
                            tripRecord.realTimeOffsetSec = tripRecord.schDepTime.secsTo(predictedDep);
                            tripRecord.waitTimeSec       = _agencyTime.secsTo(predictedDep);
                        }

                        // Time-based statuses (arriving/early/late/on-time), or no real-time data for stop
                        if ((rtArrivalMissing && rtDepartureMissing) || rtMissingForStop)
                            tripRecord.tripStatus = MISSING;
                        else
                            tripRecord.tripStatus = RUNNING;

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

                        // Vehicle id and real-time indicators for output to indicate it is actually available
                        tripRecord.vehicleRealTime   = rActiveFeed->getOperatingVehicle(tripRecord.tripID);
                        tripRecord.realTimeDataAvail = true;

                        // Make a stop irrelevant if it has no arrival nor departure data and its schedule time is
                        // purely in the past -- Could this be handled in invalidateTrips() better?
                        if (rtArrivalMissing && rtDepartureMissing) {
                            if (!tripRecord.schDepTime.isNull() && _agencyTime > tripRecord.schDepTime) {
                                tripRecord.tripStatus = IRRELEVANT;
                            }
                            if (!tripRecord.schArrTime.isNull() && _agencyTime > tripRecord.schArrTime) {
                                tripRecord.tripStatus = IRRELEVANT;
                            }
                        }
                    } // End of processing an actual trip with real-time information (non-cancelled, non-stop-skip)
                } // End of active (real-time) trip handling condition
            } // End of loop on trips
        } // End of loop on routes

        // Add the 'added' trips (mark with SUPPLEMENT)
        // We first fill a separate compatible data structure which will then merge into the vector of trip-records
        QMap<QString, QVector<QPair<QString, quint32>>> addedTrips;
        rActiveFeed->getAddedTripsServingStop(stopID, addedTrips);
        for (const QString &routeID : addedTrips.keys()) {
            for (const QPair<QString, quint32> &tripAndIndex : addedTrips[routeID]) {
                // Make a new tripRecord so we have a place to add current trip information
                StopRecoTripRec tripRecord;
                tripRecord.tripStatus        = RUNNING;
                tripRecord.supplementalTrip  = true;
                tripRecord.realTimeDataAvail = true;

                // We cannot reliably map information of a real-time-only trip
                tripRecord.beginningOfTrip = false;
                tripRecord.endOfTrip       = false;
                tripRecord.dropoffType     = 0;
                tripRecord.pickupType      = 0;
                tripRecord.tripID          = tripAndIndex.first;
                tripRecord.vehicleRealTime = rActiveFeed->getOperatingVehicle(tripRecord.tripID);

                // Calculate wait time and actual departure/arrivals if available
                QDateTime prArrTime, prDepTime;
                if (rActiveFeed->tripStopActualTime(tripAndIndex.first,
                                                    tripAndIndex.second,
                                                    tripRecord.stopID,
                                                    QDateTime(),
                                                    QDateTime(),
                                                    prArrTime,
                                                    prDepTime)) {

                    // We have to have at least one time
                    if (!prDepTime.isNull()) {
                        // Fill in the countdown until departure initially ...
                        tripRecord.realTimeDeparture = prDepTime.toTimeZone(_agencyTime.timeZone());
                        tripRecord.waitTimeSec = _agencyTime.secsTo(tripRecord.realTimeDeparture);
                    }
                    if (!prArrTime.isNull()) {
                        // ... but we will always prefer the countdown time to show the time until vehicle arrival
                        tripRecord.realTimeArrival = prArrTime.toTimeZone(_agencyTime.timeZone());
                        tripRecord.waitTimeSec = _agencyTime.secsTo(tripRecord.realTimeArrival);
                    }
                }

                // There is not really a schedule offset (since no schedule exists for added trips)
                tripRecord.realTimeOffsetSec = 0;

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
                tripRecord.headsign = (*sStops)[finalStopID].stop_name;

                fullTrips[routeID].tripRecos.push_back(tripRecord);
            }
        }
    }  // End of real-time data integration block
    }  // End of loop on stop ID list

    // Now that all requested stops and routes have been filled, each trip must be sorted by arrival time per route
    // or else the number-of-trips style of invalidation could miss supplemental trips. It is also possible that the
    // real-time data has altered the ordering of trip arrivals, so the sort is just a necessary evil.
    for (const QString &routeID : fullTrips.keys()) {
        std::sort(fullTrips[routeID].tripRecos.begin(),
                  fullTrips[routeID].tripRecos.end(),
                  [](const StopRecoTripRec &rt1, const StopRecoTripRec &rt2) {
            return rt1.waitTimeSec < rt2.waitTimeSec;
        });
    }

    // Invalidate trips which fall outside of the requested parameters (_maxTripsPerRoute or _lookaheadTime)
    for (const QString &routeID : fullTrips.keys()) {
        invalidateTrips(routeID, fullTrips, routeTrips);
    }
}

void TripStopReconciler::addTripRecordsForServiceDay(const QString    &routeID,
                                                     const QDate      &serviceDay,
                                                     const QString    &stopID,
                                                     StopRecoRouteRec &routeRecord) const
{
    // Go through all the trips from the static feed and find all that hit this stop
    for (qint32 tripIdx = 0; tripIdx < (*sStops)[stopID].stopTripsRoutes[routeID].length(); ++tripIdx)
    {
        // Offset into the individual stop-trip array (so the entire trip information can be found)
        qint32        stopTripIdx  = (*sStops)[stopID].stopTripsRoutes[routeID].at(tripIdx).tripStopIndex;
        const QString curTripId    = (*sStops)[stopID].stopTripsRoutes[routeID].at(tripIdx).tripID;

        // Ensure that the trip actually runs for this service day
        if (! sService->serviceRunning(serviceDay, (*sTripDB)[curTripId].service_id))
            continue;
        // Populate the trip-record with all the pertinent / necessary details
        StopRecoTripRec tripRec;
        tripRec.tripID          = curTripId;
        tripRec.stopID          = (*sStopTimes)[curTripId].at(stopTripIdx).stop_id;
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
        if ((*sStopTimes)[curTripId].at(stopTripIdx).departure_time != StopTimes::kNoTime) {
            tripRec.schDepTime  = localNoon.addSecs((*sStopTimes)[curTripId].at(stopTripIdx).departure_time);
            tripRec.waitTimeSec = _agencyTime.secsTo(tripRec.schDepTime);
            scheduleTimeAvail   = true;
        } else {
            tripRec.schDepTime  = QDateTime();
        }
        if ((*sStopTimes)[curTripId].at(stopTripIdx).arrival_time != StopTimes::kNoTime) {
            tripRec.schArrTime  = localNoon.addSecs((*sStopTimes)[curTripId].at(stopTripIdx).arrival_time);
            // NOTE: Prefer the arrival time for the wait-time calculations, so that's process arr. after dep.!
            tripRec.waitTimeSec = _agencyTime.secsTo(tripRec.schArrTime);
            scheduleTimeAvail   = true;
        } else {
            tripRec.schArrTime  = QDateTime();
        }

        // There is neither a departure nor arrival time from which to countdown
        // Some stops aren't timed at all, so the "next possible time" is used (called the sort time)
        if ((*sStopTimes)[curTripId].at(stopTripIdx).arrival_time == StopTimes::kNoTime &&
            (*sStopTimes)[curTripId].at(stopTripIdx).departure_time == StopTimes::kNoTime) {
            QDateTime localNoon(tripRec.tripServiceDate, QTime(12, 0, 0), _agencyTime.timeZone());
            tripRec.schSortTime = localNoon.addSecs((*sStops)[stopID].stopTripsRoutes[routeID].at(tripIdx).sortTime);
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

void TripStopReconciler::invalidateTrips(const QString                   &routeID,
                                         QMap<QString, StopRecoRouteRec> &fullTrips,
                                         QMap<QString, StopRecoRouteRec> &relevantRouteTrips)
{
    qint32 nbTripsForRoute = 0;
    for (StopRecoTripRec &tripRecord : fullTrips[routeID].tripRecos) {
        QDateTime stopTime;
        if (tripRecord.realTimeDataAvail && tripRecord.tripStatus != MISSING) {
            // Use real-time data for the lookahead determination unless the trip is running but no data is present...
            if (!tripRecord.realTimeArrival.isNull()) {
                stopTime = tripRecord.realTimeArrival;
            } else if (!tripRecord.realTimeDeparture.isNull()) {
                stopTime = tripRecord.realTimeDeparture;
            }
        } else {
            // ... unless it is not available
            if (!tripRecord.schArrTime.isNull()) {
                stopTime = tripRecord.schArrTime;
            } else if (!tripRecord.schDepTime.isNull()) {
                stopTime = tripRecord.schDepTime;
            }
        }

        // Mark trips as invalid if they are outside of the number of trips requested
        if (_maxTripsPerRoute != 0 && nbTripsForRoute >= _maxTripsPerRoute) {
            tripRecord.tripStatus = IRRELEVANT;
        }

        // Mark trips as invalid if they are outside the time window requested
        if (_lookaheadMins != 0 &&
            (((tripRecord.tripStatus == SCHEDULE || tripRecord.tripStatus == MISSING) && stopTime > _lookaheadTime) ||
             (tripRecord.tripStatus == NOSCHEDULE && tripRecord.schSortTime > _lookaheadTime)   )) {
            tripRecord.tripStatus = IRRELEVANT;
        }

        // Mark the trip-stop as invalid if it occurred in the past
        // There are two notions: scheduled and unscheduled times. Unscheduled times should NOT display a time
        // but we allow a countdown (probably the client should warn that the data is missing). If realtime
        // data were to be associated with these 'untimed' stops, then hopefully that would supplement it :) )
        if ((tripRecord.tripStatus == SCHEDULE   && _agencyTime.secsTo(stopTime) < 0) ||
            (tripRecord.tripStatus == NOSCHEDULE && _agencyTime > tripRecord.schSortTime)) {
            tripRecord.tripStatus = IRRELEVANT;
        }

        // With realtime data available, the invalidation process needs to take extra parameters into consideration
        else if (tripRecord.realTimeDataAvail) {
            if (tripRecord.tripStatus == RUNNING || tripRecord.tripStatus == DEPART ||
                tripRecord.tripStatus == BOARD   || tripRecord.tripStatus == ARRIVE) {
                if (!tripRecord.realTimeArrival.isNull() &&
                    ((_agencyTime.secsTo(tripRecord.realTimeArrival) < -60) ||
                     (_lookaheadMins != 0 && tripRecord.realTimeArrival > _lookaheadTime))) {
                    tripRecord.tripStatus = IRRELEVANT;
                } else if (!tripRecord.realTimeDeparture.isNull() &&
                         ((_agencyTime.secsTo(tripRecord.realTimeDeparture) < -60) ||
                          (_lookaheadMins != 0 && tripRecord.realTimeDeparture > _lookaheadTime))) {
                    tripRecord.tripStatus = IRRELEVANT;
                }
            } else if (tripRecord.tripStatus == CANCEL || tripRecord.tripStatus == SKIP) {
                // Excpetion for cancelled and stop-skip trips: show for 2 minutes past the scheduled time
                if (!tripRecord.schArrTime.isNull()) {
                    qint64 secUntilSchArr = _agencyTime.secsTo(tripRecord.schArrTime);
                    if (secUntilSchArr < -120 || secUntilSchArr > _lookaheadMins * 60) {
                        tripRecord.tripStatus = IRRELEVANT;
                    }
                } else if (!tripRecord.schDepTime.isNull()) {
                    qint64 secUntilSchDep = _agencyTime.secsTo(tripRecord.schDepTime);
                    if (secUntilSchDep < -120 || secUntilSchDep > _lookaheadMins * 60) {
                        tripRecord.tripStatus = IRRELEVANT;
                    }
                }
            }
        }

        // If we didn't mark the trip as irrelevant, then it shall count against the maxTripsForRoute and it can
        // be added to the output structure for output rendering
        if (tripRecord.tripStatus != IRRELEVANT) {
            ++nbTripsForRoute;
            relevantRouteTrips[routeID].tripRecos.push_back(tripRecord);
        }
    }
}

} // Namespace GTFS
