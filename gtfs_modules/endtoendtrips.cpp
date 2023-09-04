/*
 * GtfsProc_Server
 * Copyright (C) 2023, Daniel Brook
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

EndToEndTrips::EndToEndTrips(qint32 futureMinutes, bool realtimeOnly, QList<QString> argList)
    : StaticStatus(),
      _futureMinutes(futureMinutes),
      _realtimeOnly(realtimeOnly),
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
            bool numberOK;
            qint32 cnxTimeMin = _tripCnx[i].toInt(&numberOK);
            if (!numberOK || cnxTimeMin < 0) {
                fillProtocolFields("E2E", 902, resp);  // Non-numeric OR negative connection time specified
                return;
            }
        } else {
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
    fillRecoOD(0, 0, _tripCnx[0], _tripCnx[1], deadRecos, travelRecoTimes);

    if (argLength > 2) {
        quint32 totalConnections = (argLength - 2) / 3;
        for (quint32 connection = 0; connection < totalConnections; ++connection) {
            qint32 cnxTime = _tripCnx[2 + connection * 3].toInt();
            QString originStopId = _tripCnx[3 + connection * 3];
            QString destinationStopId = _tripCnx[4 + connection * 3];
            fillRecoOD(connection + 1, cnxTime, originStopId, destinationStopId, deadRecos, travelRecoTimes);
        }
    }

    // Get stop information from every stop encountered - then save them to JSON
    QSet<QString> stopIds;
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

    // Attach the routes and trips collections
    resp["trips"]  = stopRouteArray;

    fillProtocolFields("E2E", 0, resp);
}

void EndToEndTrips::fillRecoOD(quint8                             legNum,
                               quint32                            xferMin,
                               QString                            oriStopId,
                               QString                            desStopId,
                               QSet<qsizetype>                   &deadRecos,
                               QVector<QVector<StopRecoTripRec>> &allRecos)
{
    QHash<QString, GTFS::StopRecoRouteRec> tripsForOriStopByRouteID;
    qint32 originLookaheadTime = (legNum == 0) ? _futureMinutes : 720;
    QList<QString> startStopIds;
    if (_parentSta->contains(oriStopId)) {
        startStopIds = (*_parentSta)[oriStopId].toList();
    } else {
        startStopIds.push_back(oriStopId);
    }
    GTFS::TripStopReconciler oriStopTripLoader(startStopIds, _rtData, _systemDate, getAgencyTime(), originLookaheadTime,
                                               _status, _service, _stops, _routes, _tripDB, _stopTimes, _realTimeProc);
    oriStopTripLoader.getTripsByRoute(tripsForOriStopByRouteID);

    QHash<QString, GTFS::StopRecoRouteRec> tripsForDesStopByRouteID;
    QList<QString> destStopIds;
    if (_parentSta->contains(desStopId)) {
        destStopIds = (*_parentSta)[desStopId].toList();
    } else {
        destStopIds.push_back(desStopId);
    }
    GTFS::TripStopReconciler desStopTripLoader(destStopIds, _rtData, _systemDate, getAgencyTime(), 720,  // 12 hour L-A
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
                        if (dep < earliestDep) {
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
