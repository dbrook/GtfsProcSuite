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

#include "servicebetweenstops.h"

#include "datagateway.h"

#include <QJsonArray>
#include <QSet>

namespace GTFS {

ServiceBetweenStops::ServiceBetweenStops(const QString &originStop,
                                         const QString &destinationStop,
                                         const QDate   &serviceDate)
    : StaticStatus(),
      _oriStopID(originStop),
      _desStopID(destinationStop),
      _serviceDate(serviceDate)
{
    _service   = GTFS::DataGateway::inst().getServiceDB();
    _stops     = GTFS::DataGateway::inst().getStopsDB();
    _parents   = GTFS::DataGateway::inst().getParentsDB();
    _routes    = GTFS::DataGateway::inst().getRoutesDB();
    _tripDB    = GTFS::DataGateway::inst().getTripsDB();
    _stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
}

void ServiceBetweenStops::fillResponseData(QJsonObject &resp)
{
    // Ensure a date was requested
    if (_serviceDate.isNull()) {
        fillProtocolFields("SBS", 703, resp);
        return;
    }

    // Parent stops can be supported, but we need to track ALL children if they are used
    bool oriIsParent = false;
    bool desIsParent = false;
    QList<QString> oriStopIDs;
    QList<QString> desStopIDs;

    // Deal with parent stops
    if (_parents->contains(_oriStopID)) {
        // Put all the child stops into the origin stop ID list
        oriIsParent = true;
        oriStopIDs = (*_parents)[_oriStopID].toList();
    }
    if (_parents->contains(_desStopID)) {
        // Put all the child stops into the destination stop ID list
        desIsParent = true;
        desStopIDs = (*_parents)[_desStopID].toList();
    }

    // Otherwise it's a single stop
    if (!_stops->contains(_oriStopID) && !oriIsParent) {
        fillProtocolFields("SBS", 701, resp);
        return;
    } else {
        oriStopIDs.push_back(_oriStopID);
    }
    if (!_stops->contains(_desStopID) && !desIsParent) {
        fillProtocolFields("SBS", 702, resp);
        return;
    } else {
        desStopIDs.push_back(_desStopID);
    }

    // With valid stop IDs found, fill in details for the origin and destination
    resp["ori_stop_id"]   = _oriStopID;
    resp["ori_stop_name"] = (*_stops)[_oriStopID].stop_name;
    resp["ori_stop_desc"] = (*_stops)[_oriStopID].stop_desc;
    resp["des_stop_id"]   = _desStopID;
    resp["des_stop_name"] = (*_stops)[_desStopID].stop_name;
    resp["des_stop_desc"] = (*_stops)[_desStopID].stop_desc;

    // Service date for confirmation
    resp["service_date"]  = _serviceDate.toString("ddd dd-MMM-yyyy");

    /*
     * Get the list of stops which service the origin stop, then make a separate list of stops which service the
     * destination stop and take the intersection of these two lists for the service date requested.
     */
    QMap<QString, tripOnDSchedule> tripSchTimes;
    QSet<QString> tripsPickingUpOrigin;
    QSet<QString> tripsDroppingOffDestination;

    for (const QString &childStop : oriStopIDs) {
        tripsForServiceDay(childStop, tripsPickingUpOrigin, true, tripSchTimes);
    }
    for (const QString &childStop : desStopIDs) {
        tripsForServiceDay(childStop, tripsDroppingOffDestination, false, tripSchTimes);
    }
    tripsPickingUpOrigin.intersect(tripsDroppingOffDestination);

    /*
     * Fill origin's headsign (if it exists, otherwise use the whole trip's headsign),
     * arrival/departure at origin, and arrival/departure time at destination.
     */
    QVector<tripOnDSchedule> commonTrips;
    for (const QString &tripID : tripsPickingUpOrigin) {
        tripOnDSchedule singleTrip;
        singleTrip.oriStopSeq   = tripSchTimes[tripID].oriStopSeq;
        singleTrip.desStopSeq   = tripSchTimes[tripID].desStopSeq;

        // Do not show a trip going the wrong way
        if (singleTrip.desStopSeq < singleTrip.oriStopSeq) {
            continue;
        }

        singleTrip.tripID            = tripID;
        singleTrip.routeID           = tripSchTimes[tripID].routeID;
        singleTrip.oriArrival        = tripSchTimes[tripID].oriArrival;
        singleTrip.oriDeparture      = tripSchTimes[tripID].oriDeparture;
        singleTrip.ori_pickup_type   = tripSchTimes[tripID].ori_pickup_type;
        singleTrip.desArrival        = tripSchTimes[tripID].desArrival;
        singleTrip.desDeparture      = tripSchTimes[tripID].desDeparture;
        singleTrip.des_drop_off_type = tripSchTimes[tripID].des_drop_off_type;

        if (tripSchTimes[tripID].headsign != "") {
            singleTrip.headsign = tripSchTimes[tripID].headsign;
        } else {
            singleTrip.headsign = (*_tripDB)[tripID].trip_headsign;
        }

        commonTrips.push_back(singleTrip);
    }

    // Finally, sort by the arrival-then-departure times of the origin stop and put in the JSON structure
    std::sort(commonTrips.begin(), commonTrips.end(),
              [](const tripOnDSchedule &t1, const tripOnDSchedule &t2) {
        QDateTime ori1Time;
        QDateTime ori2Time;
        if (!t1.oriArrival.isNull()) {
            ori1Time = t1.oriArrival;
        } else if (!t1.oriDeparture.isNull()) {
            ori1Time = t1.oriDeparture;
        }
        if (!t2.oriArrival.isNull()) {
            ori2Time = t2.oriArrival;
        } else if (!t2.oriDeparture.isNull()) {
            ori2Time = t2.oriDeparture;
        }
        return ori1Time < ori2Time;
    });

    QJsonArray tripArray;
    for (const tripOnDSchedule &singleTrip : commonTrips) {
        QJsonObject singleOutputTrip;
        singleOutputTrip["trip_id"]          = singleTrip.tripID;
        singleOutputTrip["trip_short_name"]  = (*_tripDB)[singleTrip.tripID].trip_short_name;
        singleOutputTrip["route_id"]         = singleTrip.routeID;
        singleOutputTrip["route_short_name"] = (*_routes)[singleTrip.routeID].route_short_name;
        singleOutputTrip["route_long_name"]  = (*_routes)[singleTrip.routeID].route_long_name;
        singleOutputTrip["headsign"]         = singleTrip.headsign;
        singleOutputTrip["ori_arrival"]      = singleTrip.oriArrival.toString("ddd hh:mm");
        singleOutputTrip["ori_depature"]     = singleTrip.oriDeparture.toString("ddd hh:mm");
        singleOutputTrip["ori_pick_up"]      = singleTrip.ori_pickup_type;
        singleOutputTrip["des_arrival"]      = singleTrip.desArrival.toString("ddd hh:mm");
        singleOutputTrip["des_departure"]    = singleTrip.desDeparture.toString("ddd hh:mm");
        singleOutputTrip["des_drop_off"]     = singleTrip.des_drop_off_type;

        qint64 durationSe = singleTrip.oriDeparture.secsTo(singleTrip.desArrival);
        qint64 durationMi = (durationSe / 60) % 60;
        qint64 durationHr = (durationSe / 3600);

        singleOutputTrip["duration"] = QString("%1:%2").arg(durationHr, 2, 10, QChar('0'))
                                                       .arg(durationMi, 2, 10, QChar('0'));

        tripArray.push_back(singleOutputTrip);
    }
    resp["trips"] = tripArray;

    fillProtocolFields("SBS", 0, resp);
}

void ServiceBetweenStops::tripsForServiceDay(const QString                  &stopID,
                                             QSet<QString>                  &tripSet,
                                             bool                            isOrigin,
                                             QMap<QString, tripOnDSchedule> &tods)
{
    // For each route that serves the stop, load all trips and store the trip IDs into the set
    for (const QString &routeID : (*_stops)[stopID].stopTripsRoutes.keys()) {
        for (const tripStopSeqInfo &tripStop : (*_stops)[stopID].stopTripsRoutes[routeID]) {
            // See that the trip is operating on the requested date ...
            if (!_service->serviceRunning(_serviceDate, (*_tripDB)[tripStop.tripID].service_id)) {
                continue;
            }

            const QString thisTripID = tripStop.tripID;

            // ... but also that it picks up or drops off at the requested station
            if (isOrigin) {
                // Make sure that pickup service is offered for the origin stop
                if ((*_stopTimes)[thisTripID].at(tripStop.tripStopIndex).pickup_type != 1) {
                    tripSet.insert(thisTripID);
                    tods[thisTripID].headsign =
                            (*_stopTimes)[tripStop.tripID].at(tripStop.tripStopIndex).stop_headsign;
                    tods[thisTripID].oriStopSeq =
                            (*_stopTimes)[tripStop.tripID].at(tripStop.tripStopIndex).stop_sequence;
                    tods[thisTripID].ori_pickup_type =
                            (*_stopTimes)[thisTripID].at(tripStop.tripStopIndex).pickup_type;
                    tods[thisTripID].routeID = routeID;

                    // Insert the times for use in the sorting process
                    QTime     localNoon(12, 0, 0);
                    QDateTime localNoonDT(_serviceDate, localNoon, getAgencyTime().timeZone());
                    qint32    arrTime = (*_stopTimes)[tripStop.tripID].at(tripStop.tripStopIndex).arrival_time;
                    qint32    depTime = (*_stopTimes)[tripStop.tripID].at(tripStop.tripStopIndex).departure_time;

                    if (arrTime != StopTimes::kNoTime) {
                        tods[thisTripID].oriArrival = localNoonDT.addSecs(arrTime);
                    }
                    if (depTime != StopTimes::kNoTime) {
                        tods[thisTripID].oriDeparture = localNoonDT.addSecs(depTime);
                    }
                }
            } else {
                // Make sure that drop off service is offered for the destination stop
                if ((*_stopTimes)[thisTripID].at(tripStop.tripStopIndex).drop_off_type != 1) {
                    tripSet.insert(thisTripID);
                    tods[thisTripID].desStopSeq =
                            (*_stopTimes)[tripStop.tripID].at(tripStop.tripStopIndex).stop_sequence;
                    tods[thisTripID].des_drop_off_type =
                            (*_stopTimes)[thisTripID].at(tripStop.tripStopIndex).drop_off_type;

                    // Insert the times for use in the sorting process
                    QTime     localNoon(12, 0, 0);
                    QDateTime localNoonDT(_serviceDate, localNoon, getAgencyTime().timeZone());
                    qint32    arrTime = (*_stopTimes)[tripStop.tripID].at(tripStop.tripStopIndex).arrival_time;
                    qint32    depTime = (*_stopTimes)[tripStop.tripID].at(tripStop.tripStopIndex).departure_time;

                    if (arrTime != StopTimes::kNoTime) {
                        tods[thisTripID].desArrival = localNoonDT.addSecs(arrTime);
                    }
                    if (depTime != StopTimes::kNoTime) {
                        tods[thisTripID].desDeparture = localNoonDT.addSecs(depTime);
                    }
                }
            }
        }
    } // End loop on routes
}

}  // Namespace GTFS
