/*
 * GtfsProc_Server
 * Copyright (C) 2023-2026, Daniel Brook
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

#include "endtoendtrips.h"

#include "upcomingstopservice.h"

#include <QJsonObject>
#include <QJsonArray>

namespace GTFS {

EndToEndTrips::EndToEndTrips(qint32 futureMinutes, bool realtimeOnly, bool firstIsTripId, QList<QString> argList)
    : StaticStatus(),
      _futureMinutes(futureMinutes),
      _realtimeOnly(realtimeOnly),
      _firstIsTripId(firstIsTripId),
      _tripCnx(argList),
      _rtData(false)
{
    // Realtime Data Determination
    RealTimeGateway::inst().realTimeTransactionHandled();
    _realTimeProc = GTFS::RealTimeGateway::inst().getActiveFeed();
    if (_realTimeProc != nullptr) {
        _rtData = true;
    }

    // Static Datasets for the TripStopReconciler
    _status    = GTFS::DataGateway::inst().getStatus();
    _service   = GTFS::DataGateway::inst().getServiceDB();
    _stops     = GTFS::DataGateway::inst().getStopsDB();
    _parentSta = GTFS::DataGateway::inst().getParentsDB();
    _routes    = GTFS::DataGateway::inst().getRoutesDB();
    _stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
    _tripDB    = GTFS::DataGateway::inst().getTripsDB();

    // Override the service date if in the fixed debugging date mode
    if (!_status->getOverrideDateTime().isNull()) {
        _systemDate = _status->getOverrideDateTime().date();
    } else {
        _systemDate = getAgencyTime().date();
    }
}

void EndToEndTrips::fillResponseData(QJsonObject &resp)
{
    // Supported arguments analysis
    // 2: origin and destination, no transfer
    // 5: 1 connection: [0] and [1] first leg ori/des, [2] connection time, [3] and [4] second leg ori/des
    // 8: 2 connections O1=[0], D1=[1], Connect=[2], O2=[3], D2=[4], Connect=[5], O3=[6], D3=[7]
    // 11, 14, 17, etc.!
    quint32 argLength = _tripCnx.length();
    if (argLength != 2 && (argLength - 2) % 3 != 0) {
        fillProtocolFields("E2E", 901, resp);  // Wrong number of arguments
        return;
    }

    // Ensure the validity of input arguments
    for (quint32 i = 0; i < argLength; ++i) {
        if (i >= 2 && (i - 2) % 3 == 0) {
            QList<QString> connectionRange = _tripCnx[i].split("-");
            if (connectionRange.length() > 2) {
                fillProtocolFields("E2E", 904, resp);  // Only allowed a minimum and maximum connection time, ex: "1-5"
                return;
            } else if (connectionRange.length() == 2) {
                bool minCnxTimeOK;
                qint32 cnxTimeMin = connectionRange[0].toInt(&minCnxTimeOK);
                bool maxCnxTimeOK;
                qint32 cnxTimeMax = connectionRange[1].toInt(&maxCnxTimeOK);
                if (!minCnxTimeOK || cnxTimeMin < 0 || !maxCnxTimeOK || cnxTimeMax < 0) {
                    fillProtocolFields("E2E", 902, resp);  // Non-numeric OR negative connection time specified
                    return;
                } else if (cnxTimeMax < cnxTimeMin) {
                    fillProtocolFields("E2E", 905, resp);  // Maximum connection time is less than the minimum
                    return;
                }
            } else if (connectionRange.length() == 1) {
                bool numberOK;
                qint32 cnxTimeMin = _tripCnx[i].toInt(&numberOK);
                if (!numberOK || cnxTimeMin < 0) {
                    fillProtocolFields("E2E", 902, resp);  // Non-numeric OR negative connection time specified
                    return;
                }
            } else {
                fillProtocolFields("E2E", 906, resp);  // Unknown issue parsing the connection time
                return;
            }
        } else {
            if (i == 0 && _firstIsTripId) {
                // Don't validate the first entry if it's expected to be a trip ID (ETS/ETR)
                continue;
            }
            if (!_stops->contains(_tripCnx[i])) {
                fillProtocolFields("E2E", 903, resp);  // Non-existent stop id specified
                return;
            }
        }
    }

    // Store dataset modification times (used by long-running clients to see if new route details should be retrieved)
    resp["static_data_modif"] = _status->getStaticDatasetModifiedTime().toString("dd-MMM-yyyy hh:mm:ss t");

    if (_rtData) {
        QDateTime activeFeedTime = _realTimeProc->getFeedTime();
        if (activeFeedTime.isNull()) {
            resp["realtime_age_sec"] = "-";
        } else {
            resp["realtime_age_sec"] = activeFeedTime.secsTo(getAgencyTime());
        }
    }

    // Get upcoming arrivals like a normal NCF request ... BUT: filter out trips that do not pickup at origin and
    // drop-off at destination. Filter out anything that skips either stop, as well.
    QVector<QVector<StopRecoTripRec>> travelRecoTimes;
    QSet<qsizetype>                   deadRecos;
    StopRecoTripRec                   currentTrip;
    bool                              foundCurrentTrip = false;

    if (_firstIsTripId) {
        QHash<QString, GTFS::StopRecoRouteRec> tripInProgDest;
        QList<QString> startStopIds;
        if (_parentSta->contains(_tripCnx[1])) {
            startStopIds = (*_parentSta)[_tripCnx[1]].toList();
        } else {
            startStopIds.push_back(_tripCnx[1]);
        }
        GTFS::TripStopReconciler desStopTripLoader(startStopIds, _rtData, _systemDate, getAgencyTime(),
                                                   _futureMinutes, _status, _service, _stops, _routes,
                                                   _tripDB, _stopTimes, _realTimeProc);
        desStopTripLoader.getTripsByRoute(tripInProgDest);
        QString routeId;
        qsizetype tripIdx = -1;
        for (const QString &route : tripInProgDest.keys()) {
            for (qsizetype trip = 0; trip < tripInProgDest[route].tripRecos.length(); ++trip) {
                if (tripInProgDest[route].tripRecos[trip].tripID == _tripCnx[0]) {
                    routeId = route;
                    tripIdx = trip;
                    break;
                }
            }
        }
        if (tripIdx != -1) {
            foundCurrentTrip = true;
            currentTrip = tripInProgDest[routeId].tripRecos[tripIdx];
        }
    }

    QDateTime nullQDT;
    QDateTime firstConnection;
    quint32 xferMin = 0;
    quint32 xferMax = 0;
    quint8 cnxOriStartIdx = 0;
    quint8 cnxDesStartIdx = 1;
    if (_firstIsTripId && argLength > 2) {
        QList<QString> connectionRange = _tripCnx[2].split("-");
        // Note: these were already validated at the start of this function
        if (connectionRange.length() == 2) {
            xferMin = connectionRange[0].toInt();
            xferMax = connectionRange[1].toInt();
        } else {
            xferMin = connectionRange[0].toInt();
            xferMax = 0;
        }

        // xferMin = _tripCnx[2].toUInt();
        cnxOriStartIdx = 3;
        cnxDesStartIdx = 4;
        if (foundCurrentTrip) {
            // FIXME: Maybe make more robust ... (not just using arr/rt-arr time?) if issues arise
            firstConnection = !currentTrip.realTimeArrival.isNull()
                                ? currentTrip.realTimeArrival.addSecs(xferMin * 60)
                                : currentTrip.schArrTime.addSecs(xferMin * 60);
        }
    }
    if (!(_firstIsTripId && argLength == 2)) {
        fillRecoOD(0, firstConnection, xferMin, xferMax,
                   _tripCnx[cnxOriStartIdx], _tripCnx[cnxDesStartIdx],
                   deadRecos, travelRecoTimes);
    }

    if ((_firstIsTripId && argLength > 5) || (!_firstIsTripId && argLength > 2)) {
        quint32 totalConnections = _firstIsTripId ? (argLength - 5) / 3 : (argLength - 2) / 3;
        for (quint32 connection = 0; connection < totalConnections; ++connection) {
            QList<QString> connectionRange = _tripCnx[2 + cnxOriStartIdx + connection * 3].split("-");
            // Note: these were already validated at the start of this function
            if (connectionRange.length() == 2) {
                xferMin = connectionRange[0].toInt();
                xferMax = connectionRange[1].toInt();
            } else {
                xferMin = connectionRange[0].toInt();
                xferMax = 0;
            }
            // qint32 cnxTime = _tripCnx[2 + cnxOriStartIdx + connection * 3].toInt();
            QString originStopId = _tripCnx[3 + cnxOriStartIdx + connection * 3];
            QString destinationStopId = _tripCnx[4 + cnxOriStartIdx + connection * 3];
            fillRecoOD(connection + 1, nullQDT, xferMin, xferMax,
                       originStopId, destinationStopId, deadRecos, travelRecoTimes);
        }
    }

    // Get stop information from every stop encountered - then save them to JSON
    QSet<QString> stopIds;
    if (foundCurrentTrip) {
        stopIds.insert(currentTrip.stopID);
    }
    for (qsizetype r = 0; r < travelRecoTimes.length(); ++r) {
        if (deadRecos.contains(r)) {
            continue;
        }
        for (const StopRecoTripRec &srtr : travelRecoTimes[r]) {
            stopIds.insert(srtr.stopID);
        }
    }
    QJsonObject stopsAssocArray;
    for (const QString &stopId : stopIds) {
        QJsonObject stopDetails;
        stopDetails["stop_desc"] = (*_stops)[stopId].stop_desc;
        stopDetails["stop_name"] = (*_stops)[stopId].stop_name;
        stopsAssocArray[stopId] = stopDetails;
    }
    resp["stops"] = stopsAssocArray;

    // Place into JSON style output now that it is sorted together by wait time
    QJsonArray stopRouteArray;
    for (qsizetype r = 0; r < travelRecoTimes.length(); ++r) {
        if (deadRecos.contains(r)) {
            continue;
        }

        QJsonArray connectionArray;
        for (const StopRecoTripRec &srtr : travelRecoTimes[r]) {
            QJsonObject stopTripItem;
            UpcomingStopService::fillTripData(srtr, stopTripItem, getStatus()->format12h(),
                                              (*_tripDB)[srtr.tripID].trip_short_name);
            connectionArray.push_back(stopTripItem);
        }
        stopRouteArray.push_back(connectionArray);
    }

    if (foundCurrentTrip) {
        QJsonObject stopTripItem;
        UpcomingStopService::fillTripData(currentTrip, stopTripItem, getStatus()->format12h(),
                                          (*_tripDB)[currentTrip.tripID].trip_short_name);
        resp["current_trip"] = stopTripItem;
    } else if (_firstIsTripId) {
        // This is an ETS/ETR request but no initial connection rendered, keep the field but make it null
        resp["current_trip"] = QJsonValue();
    }

    // Attach the routes and trips collections
    resp["trips"]  = stopRouteArray;

    fillProtocolFields("E2E", 0, resp);
}

void EndToEndTrips::fillRecoOD(quint8                             legNum,
                               QDateTime                          initialCnx,
                               quint32                            xferMin,
                               quint32                            xferMax,
                               QString                            oriStopId,
                               QString                            desStopId,
                               QSet<qsizetype>                   &deadRecos,
                               QVector<QVector<StopRecoTripRec>> &allRecos)
{
    QHash<QString, GTFS::StopRecoRouteRec> tripsForOriStopByRouteID;
    QList<QString> startStopIds;
    if (_parentSta->contains(oriStopId)) {
        startStopIds = (*_parentSta)[oriStopId].toList();
    } else {
        startStopIds.push_back(oriStopId);
    }
    GTFS::TripStopReconciler oriStopTripLoader(startStopIds, _rtData, _systemDate, getAgencyTime(), _futureMinutes,
                                               _status, _service, _stops, _routes, _tripDB, _stopTimes, _realTimeProc);
    oriStopTripLoader.getTripsByRoute(tripsForOriStopByRouteID);

    QHash<QString, GTFS::StopRecoRouteRec> tripsForDesStopByRouteID;
    QList<QString> destStopIds;
    if (_parentSta->contains(desStopId)) {
        destStopIds = (*_parentSta)[desStopId].toList();
    } else {
        destStopIds.push_back(desStopId);
    }
    GTFS::TripStopReconciler desStopTripLoader(destStopIds, _rtData, _systemDate, getAgencyTime(), _futureMinutes,
                                               _status, _service, _stops, _routes, _tripDB, _stopTimes, _realTimeProc);
    desStopTripLoader.getTripsByRoute(tripsForDesStopByRouteID);

    // Only work with trips the hit both stops appropriately
    QHash<QString, GTFS::StopRecoRouteRec> tripsForBothStopsOri;
    QHash<QString, GTFS::StopRecoRouteRec> tripsForBothStopsDes;
    for (const QString &routeID : tripsForOriStopByRouteID.keys()) {
        tripsForBothStopsOri[routeID] = tripsForOriStopByRouteID[routeID];
        tripsForBothStopsOri[routeID].tripRecos.clear();
        tripsForBothStopsDes[routeID] = tripsForDesStopByRouteID[routeID];
        tripsForBothStopsDes[routeID].tripRecos.clear();

        for (qsizetype o = 0; o < tripsForOriStopByRouteID[routeID].tripRecos.length(); ++o) {
            for (qsizetype d = 0; d < tripsForDesStopByRouteID[routeID].tripRecos.length(); ++d) {
                //
                // FIXME: probably more criteria for matching but this gets us started?
                //
                if (( //tripServiceDate
                        tripsForOriStopByRouteID[routeID].tripRecos[o].tripID ==
                        tripsForDesStopByRouteID[routeID].tripRecos[d].tripID
                    ) && (
                        tripsForOriStopByRouteID[routeID].tripRecos[o].pickupType != 1 &&
                        tripsForDesStopByRouteID[routeID].tripRecos[d].dropoffType != 1
                    ) && (
                        tripsForOriStopByRouteID[routeID].tripRecos[o].tripServiceDate ==
                        tripsForDesStopByRouteID[routeID].tripRecos[d].tripServiceDate
                    ) && (
                        tripsForOriStopByRouteID[routeID].tripRecos[o].tripStatus != SKIP &&
                        tripsForDesStopByRouteID[routeID].tripRecos[d].tripStatus != SKIP
                    ) && (
                        tripsForOriStopByRouteID[routeID].tripRecos[o].tripStatus != CANCEL &&
                        tripsForDesStopByRouteID[routeID].tripRecos[d].tripStatus != CANCEL
                    ))
                {
                    tripsForBothStopsOri[routeID].tripRecos.push_back(tripsForOriStopByRouteID[routeID].tripRecos[o]);
                    tripsForBothStopsDes[routeID].tripRecos.push_back(tripsForDesStopByRouteID[routeID].tripRecos[d]);
                }
            }
        }
    }

    // If this is the first set of recommendations, setup the recommendation vectors
    for (const QString &routeID : tripsForBothStopsOri.keys()) {
        // Flatten trips into a single array (as opposed to the nesting by route present) to sorted times together
        for (const GTFS::StopRecoTripRec &rtorigin : tripsForBothStopsOri[routeID].tripRecos) {
            GTFS::TripRecStat tripStat = rtorigin.tripStatus;
            if (tripStat == GTFS::IRRELEVANT ||
                (_realtimeOnly && (tripStat == GTFS::SCHEDULE || tripStat == GTFS::NOSCHEDULE))) {
                continue;
            }

            if (legNum == 0) {
                QVector<StopRecoTripRec> connection;
                connection.push_back(rtorigin);

                // If the first connection has a specified transfer time, only add trips if they can be caught
                if (!initialCnx.isNull()) {
                    QDateTime dep = rtorigin.realTimeDeparture.isNull() ? rtorigin.schDepTime : rtorigin.realTimeDeparture;
                    if (dep < initialCnx) {
                        continue;
                    }
                }

                // Add destination arrival info as second item
                for (const GTFS::StopRecoTripRec &rtdest : tripsForBothStopsDes[routeID].tripRecos) {
                    if (rtdest.tripID != rtorigin.tripID) {
                        // Are we even looking at the same tripId?
                        continue;
                    }
                    if (rtorigin.stopSequenceNum > rtdest.stopSequenceNum) {
                        // Is the origin preceding the destination in stop-sequence number?
                        continue;
                    }
                    connection.push_back(rtdest);
                    break;
                }
                allRecos.push_back(connection);

                // Sort by arrival time at origin stop
                std::sort(allRecos.begin(), allRecos.end(),
                          [](const QVector<StopRecoTripRec> &v1, const QVector<StopRecoTripRec> &v2) {
                              return v1[0].waitTimeSec < v2[0].waitTimeSec;
                          });
            } else {
                // Subsequent connections for a trip must be connectable by comparing the arrival time of the end of
                // the previous leg and the departure time of the origin of this leg (plus any buffer time in minutes)
                for (qsizetype r = 0; r < allRecos.length(); ++r) {
                    // Check that the trip doesn't have this leg already applied
                    if (allRecos[r].length() == 2 + legNum * 2) {
                        continue;
                    }

                    // Check that the trip isn't "dead" (missing connection)
                    if (deadRecos.contains(r)) {
                        continue;
                    }

                    // Earliest-possible departure time
                    QDateTime arrivalPreviousDest = allRecos[r].last().realTimeArrival.isNull() ?
                                                    allRecos[r].last().schArrTime : allRecos[r].last().realTimeArrival;

                    QDateTime earliestDep = arrivalPreviousDest.addSecs(xferMin * 60);
                    QDateTime latestDep   = arrivalPreviousDest.addSecs(xferMax * 60);

                    // Pick the first "reachable" trip that covers the origin-and-destination
                    for (const GTFS::StopRecoTripRec &rtdest : tripsForBothStopsDes[routeID].tripRecos) {
                        if (rtdest.tripID != rtorigin.tripID) {
                            continue;
                        }
                        if (rtorigin.stopSequenceNum > rtdest.stopSequenceNum) {
                            continue;
                        }
                        QDateTime dep = rtorigin.realTimeDeparture.isNull() ?
                                        rtorigin.schDepTime : rtorigin.realTimeDeparture;
                        if (dep < earliestDep || (xferMax != 0 && dep > latestDep)) {
                            continue;
                        }
                        allRecos[r].push_back(rtorigin);
                        allRecos[r].push_back(rtdest);
                        break;
                    }
                }
            }
        }
    }

    // Ensure that each trip had a connection added.
    // If for some reason it could not be added, take the trip out of consideration.
    for (qsizetype r = 0; r < allRecos.length(); ++r) {
        if (allRecos[r].length() != 2 + legNum * 2) {
            deadRecos.insert(r);
        }
    }
}

}  // namespace GTFS
