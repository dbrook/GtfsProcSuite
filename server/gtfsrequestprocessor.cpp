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

#include "gtfsrequestprocessor.h"
#include "datagateway.h"
#include "gtfsrealtimegateway.h"

// TODO: We will replace all of the above with other object which will interface with the gateways. Both needed for now
#include "tripstopreconciler.h"

#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThreadPool>
#include <QTime>
#include <QDebug>

// Append a valid trip belonging to a route to the response
static void fillUnifiedTripDetailsForArray(const QString            &tripID,
                                           qint32                    stopTripIdx,
                                           const GTFS::OperatingDay *svc,
                                           const GTFS::StopTimeData *stopTimes,
                                           const GTFS::TripData     *tripDB,
                                           const QDate              &serviceDate,
                                           bool                      skipServiceDetail,
                                           bool                      addWaitTime,
                                           const QDateTime          &currAgency,
                                           qint64                    waitTimeSec,
                                           QJsonObject              &singleStopJSON);

GtfsRequestProcessor::GtfsRequestProcessor(QString userRequest) : request(userRequest)
{

}

/*
 * Application Kick-Off
 */
void GtfsRequestProcessor::run()
{
    QJsonObject respJson;
    QString SystemResponse;

    qDebug() << "GtfsRequestProcessor: Answering on thread " << this;

    // The first three chars are assumed to be that of the application request
    QString userApp = request.left(3);  // 3-digit application code
    QString userReq = request.mid(4);   // Remainder of request (syntax varies)

    // Decode the application request
    if (! userApp.compare("SDS", Qt::CaseInsensitive)) {
        applicationStatus(respJson);
    } else if (! userApp.compare("RTE", Qt::CaseInsensitive)) {
        availableRoutes(respJson);
    } else if (! userApp.compare("TRI", Qt::CaseInsensitive)) {
        tripStopsDisplay(userReq, false, respJson);
    } else if (! userApp.compare("TSR", Qt::CaseInsensitive)) {
        QDate noDate;
        tripsServingRoute(userReq, noDate, respJson);
    } else if (! userApp.compare("TRD", Qt::CaseInsensitive)) {
        QString remainingReq;
        QDate reqDate = determineServiceDay(userReq, remainingReq);
        tripsServingRoute(remainingReq, reqDate, respJson);
    } else if (! userApp.compare("TSS", Qt::CaseInsensitive)) {
        QDate noDate;
        tripsServingStop(userReq, noDate, respJson);
    } else if (! userApp.compare("TSD", Qt::CaseInsensitive)) {
        QString remainingReq;
        QDate reqDate = determineServiceDay(userReq, remainingReq);
        tripsServingStop(remainingReq, reqDate, respJson);
    } else if (! userApp.compare("STA", Qt::CaseInsensitive)) {
        stationDetailsDisplay(userReq, respJson);
    } else if (! userApp.compare("SSR", Qt::CaseInsensitive)) {
        stopsServedByRoute(userReq, respJson);
    } else if (! userApp.compare("NEX", Qt::CaseInsensitive)) {
        QString remainingReq;
        qint32 futureMinutes = determineMinuteRange(userReq, remainingReq);
        if (futureMinutes < 0) {
            // Requesting a maximum number of future trips per route
            // (cap at 72-hours so we can see today + tomorrow even through hopefully all of its after-midnight trips)
            nextTripsAtStop(remainingReq, 4320, futureMinutes * -1, false, respJson);
        } else {
            // Requesting future trips for a time range, no limit on occurrences
            nextTripsAtStop(remainingReq, futureMinutes, 0, false, respJson);
        }
    } else if (! userApp.compare("NXR", Qt::CaseInsensitive)) {
        nextTripsAtStop(userReq, 4320, 0, true, respJson);
    } else if (! userApp.compare("SNT", Qt::CaseInsensitive)) {
        stopsNoTrips(respJson);
    } else if (! userApp.compare("RTR", Qt::CaseInsensitive)) {
        tripStopsDisplay(userReq, true, respJson);
    } else if (! userApp.compare("RDS", Qt::CaseInsensitive)) {
        realtimeDataStatus(respJson);
    } else {
        // Return ERROR 1: Unknown request (userApp)
        SystemResponse = "{\"error\":\"1\",\"user_string\":\"" + this->request + "\"}";
    }

    // Serialize for Transport (Note: we append "\n" so clients can detect the end of the stream)
    QJsonDocument jdoc(respJson);
    emit Result(jdoc.toJson(QJsonDocument::Compact) + "\n");
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * INDIVIDUAL SERVER-SIDE APPLICATIONS
 *
 * THIS WILL PROBABLY GET TOO HUGE TO DO JUST HERE, SO REFACTORING IS ALMOST GUARANTEED
 *
 * Probably a good idea to put the data-heavy stuff just over on the data gateway's side in the GTFS:: world...
 * it will allow us to not mess around with all the individual databases here at the data serialization level.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * "STA" --> Application Status
 */
void GtfsRequestProcessor::applicationStatus(QJsonObject &resp)
{
    QJsonArray          agencyArray;
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    qint64              reqsHandled = GTFS::DataGateway::inst().incrementHandledRequests();
    QDateTime           currUTC     = QDateTime::currentDateTimeUtc();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    resp["message_type"]     = "SDS";   // Indicate it is a status message
    resp["error"]            = 0;                                         // TODO: Nice and robust error code system :-)
    resp["message_time"]     = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");
    resp["records"]          = data->getRecordsLoaded();
    resp["appuptime_ms"]     = data->getServerStartTimeUTC().msecsTo(currUTC);
    resp["dataloadtime_ms"]  = data->getServerStartTimeUTC().msecsTo(data->getLoadFinishTimeUTC());
    resp["threadpool_count"] = QThreadPool::globalInstance()->maxThreadCount();
    resp["processed_reqs"]   = reqsHandled;
    resp["feed_publisher"]   = data->getPublisher();
    resp["feed_url"]         = data->getUrl();
    resp["feed_lang"]        = data->getLanguage();
    resp["feed_valid_start"] = (!data->getStartDate().isNull()) ? data->getStartDate().toString("dd-MMM-yyyy")
                                                                : "__-___-____";
    resp["feed_valid_end"]   = (!data->getEndDate().isNull())   ? data->getEndDate().toString("dd-MMM-yyyy")
                                                                : "__-___-____";
    resp["feed_version"]     = data->getVersion();

    // Fill the agency information
    const QVector<GTFS::AgencyRecord> agencyVec = data->getAgencies();
    for (const GTFS::AgencyRecord &agency : agencyVec) {
        QJsonObject singleAgencyJSON;
        singleAgencyJSON["id"]    = agency.agency_id;
        singleAgencyJSON["name"]  = agency.agency_name;
        singleAgencyJSON["url"]   = agency.agency_url;
        singleAgencyJSON["tz"]    = agency.agency_timezone;
        singleAgencyJSON["lang"]  = agency.agency_lang;
        singleAgencyJSON["phone"] = agency.agency_phone;
        agencyArray.push_back(singleAgencyJSON);
    }
    resp["agencies"]         = agencyArray;

    // Note how long query took
    resp["proc_time_ms"]     = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
}

void GtfsRequestProcessor::availableRoutes(QJsonObject &resp)
{
    QJsonArray          routeArray;
    QDateTime           currUTC     = QDateTime::currentDateTimeUtc();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"] = "RTE";   // Indicate it is a status message
    resp["error"]        = 0;                                         // TODO: Nice and robust error code system :-)
    resp["message_time"] = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // Fill all routes associated
    const GTFS::RouteData *routes = GTFS::DataGateway::inst().getRoutesDB();
    for (const QString routeID : routes->keys()) {
        QJsonObject singleRouteJSON;
        singleRouteJSON["id"]         = routeID;
        singleRouteJSON["agency_id"]  = (*routes)[routeID].agency_id;
        singleRouteJSON["short_name"] = (*routes)[routeID].route_short_name;
        singleRouteJSON["long_name"]  = (*routes)[routeID].route_long_name;
        singleRouteJSON["desc"]       = (*routes)[routeID].route_desc;
        singleRouteJSON["type"]       = (*routes)[routeID].route_type;
        singleRouteJSON["url"]        = (*routes)[routeID].route_url;
        singleRouteJSON["color"]      = (*routes)[routeID].route_color;
        singleRouteJSON["text_color"] = (*routes)[routeID].route_text_color;
        singleRouteJSON["nb_trips"]   = (*routes)[routeID].trips.size();
        routeArray.push_back(singleRouteJSON);
    }
    resp["routes"] = routeArray;

    // Note query lookup time
    resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
}

void GtfsRequestProcessor::tripStopsDisplay(QString tripID, bool useRealTime, QJsonObject &resp)
{
    QJsonArray  tripStopArray;
    QDateTime   currUTC            = QDateTime::currentDateTimeUtc();
    const GTFS::Status *data       = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency = currUTC.toTimeZone(data->getAgencyTZ());
    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"] = "TRI";
    resp["error"]        = 0;
    resp["message_time"] = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // Fill the information for the trip from the data structures
    const GTFS::TripData     *tripDB    = GTFS::DataGateway::inst().getTripsDB();
    const GTFS::OperatingDay *serviceDB = GTFS::DataGateway::inst().getServiceDB();

    // Make sure the requested Trip ID actually Exists (otherwise we will make an entry for EVERY SINGLE REQUEST :-O)
    if (!useRealTime) {
        if (!(*tripDB).contains(tripID)) {
            resp["error"] = 101;   // The trip ID doesn't exist in the static database
            return;
        }

        // The Trip ID actually exists so fill the JSON
        resp["real_time"]    = false;
        resp["route_id"]     = (*tripDB)[tripID].route_id;
        resp["trip_id"]      = tripID;
        resp["headsign"]     = (*tripDB)[tripID].trip_headsign;
        resp["service_id"]   = (*tripDB)[tripID].service_id;
        resp["operate_days"] = serviceDB->serializeOpDays((*tripDB)[tripID].service_id);

        // Provide the associated route name / details to the response
        const GTFS::RouteData *routes = GTFS::DataGateway::inst().getRoutesDB();
        resp["route_short_name"] = (*routes)[(*tripDB)[tripID].route_id].route_short_name;
        resp["route_long_name"]  = (*routes)[(*tripDB)[tripID].route_id].route_long_name;

        // String together all the dates in range for which the trip_id requested SPECIFICALLY does not operate
        resp["exception_dates"] = serviceDB->serializeNoServiceDates((*tripDB)[tripID].service_id);

        // String together all the dates in range for which the trip_id requested operates additionally
        resp["added_dates"] = serviceDB->serializeAddedServiceDates((*tripDB)[tripID].service_id);

        // Provide the validity range
        resp["svc_start_date"] = serviceDB->getServiceStartDate((*tripDB)[tripID].service_id).toString("dd-MMM-yyyy");
        resp["svc_end_date"]   = serviceDB->getServiceEndDate((*tripDB)[tripID].service_id).toString("dd-MMM-yyyy");

        // Populate information about the trip's actual stopping points and times
        const GTFS::StopData *stops = GTFS::DataGateway::inst().getStopsDB();
        const GTFS::StopTimeData *stopTimes = GTFS::DataGateway::inst().getStopTimesDB();

        const QVector<GTFS::StopTimeRec> tripStops = (*stopTimes)[tripID];

        for (GTFS::StopTimeRec stop : tripStops) {
            QJsonObject singleStopJSON;
            QTime localNoon(12, 0, 0);

            if (stop.arrival_time != -1) {
                QTime arrivalTime = localNoon.addSecs(stop.arrival_time);
                singleStopJSON["arr_time"] = arrivalTime.toString("hh:mm");
            } else {
                singleStopJSON["arr_time"] = "-";
            }

            if (stop.departure_time != -1) {
                QTime departureTime = localNoon.addSecs(stop.departure_time);
                singleStopJSON["dep_time"]  = departureTime.toString("hh:mm");
            } else {
                singleStopJSON["dep_time"]  = "-";
            }

            singleStopJSON["sequence"]      = stop.stop_sequence;
            singleStopJSON["stop_id"]       = stop.stop_id;
            singleStopJSON["stop_name"]     = (*stops)[stop.stop_id].stop_name;
            singleStopJSON["drop_off_type"] = stop.drop_off_type;
            singleStopJSON["pickup_type"]   = stop.pickup_type;
            tripStopArray.push_back(singleStopJSON);
        }
    }

    // Process information for a real-time trip (if requested)
    GTFS::RealTimeTripUpdate *realTimeProc = GTFS::RealTimeGateway::inst().getActiveFeed();
    if (useRealTime && realTimeProc != NULL) {
        if (!realTimeProc->tripExists(tripID)) {
            resp["error"] = 102;   // The trip ID doesn't exist in the real-time data feed
            return;
        }

        // We will populate directly from the real-time feed so it will have somewhat fewer details than the static one
        // The other big difference from the static feed is we have to use date-times (since we get literal seconds
        // since the UNIX epoch) as the type from the GTFS-RealTime feed)
        QString route_id;
        QVector<GTFS::rtStopTimeUpdate> stopTimes;
        realTimeProc->fillStopTimesForTrip(tripID, route_id, stopTimes);

        resp["real_time"] = true;

        // Data information
        resp["real_time_data_time"] =
                         realTimeProc->getFeedTime().toTimeZone(currAgency.timeZone()).toString("dd-MMM-yyyy hh:mm:ss");

        // Fill in some details (route specifically ... everything else should be added if I feel like it is useful)
        const GTFS::RouteData *routes = GTFS::DataGateway::inst().getRoutesDB();
        const GTFS::StopData *stops = GTFS::DataGateway::inst().getStopsDB();
        resp["route_id"]         = route_id;
        resp["trip_id"]          = tripID;
        resp["route_short_name"] = (*routes)[(*tripDB)[tripID].route_id].route_short_name;
        resp["route_long_name"]  = (*routes)[(*tripDB)[tripID].route_id].route_long_name;

        for (const GTFS::rtStopTimeUpdate &rtsu : stopTimes) {
            QJsonObject singleStopJSON;
            singleStopJSON["arr_time"]  = (rtsu.arrTime.isNull()) ?
                                                 "-" : rtsu.arrTime.toTimeZone(currAgency.timeZone()).toString("hh:mm");
            singleStopJSON["dep_time"]  = (rtsu.depTime.isNull()) ?
                                                 "-" : rtsu.depTime.toTimeZone(currAgency.timeZone()).toString("hh:mm");
            singleStopJSON["stop_id"]   = rtsu.stopID;
            singleStopJSON["stop_name"] = (*stops)[rtsu.stopID].stop_name;
            singleStopJSON["sequence"]  = rtsu.stopSequence;
            tripStopArray.push_back(singleStopJSON);
        }
    }

    resp["stops"] = tripStopArray;
    resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
}

void GtfsRequestProcessor::tripsServingRoute(QString routeID, QDate onlyDate, QJsonObject &resp)
{
    QJsonArray  routeTripArray;
    QDateTime           currUTC    = QDateTime::currentDateTimeUtc();
    const GTFS::Status *data       = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency = currUTC.toTimeZone(data->getAgencyTZ());
    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"] = "TSR";
    resp["error"]        = 0;
    resp["message_time"] = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // See if the route requested actually exists
    const GTFS::RouteData *routes = GTFS::DataGateway::inst().getRoutesDB();

    if (routes->contains(routeID)) {
        const GTFS::TripData     *tripDB = GTFS::DataGateway::inst().getTripsDB();
        const GTFS::OperatingDay *svc    = GTFS::DataGateway::inst().getServiceDB();

        resp["route_id"]         = routeID;
        resp["route_short_name"] = (*routes)[routeID].route_short_name;
        resp["route_long_name"]  = (*routes)[routeID].route_long_name;
        resp["service_date"]     = onlyDate.toString("ddd dd-MMM-yyyy");

        for (const QPair<QString, qint32> tripIDwTime : (*routes)[routeID].trips) {
            // Loop on each route that serves the stop
            QString tripID    = tripIDwTime.first;
            QString serviceID = (*tripDB)[tripID].service_id;

            // If only a certain day is requested, check if the service is actually running before appending a trip
            if (!onlyDate.isNull() && !svc->serviceRunning(onlyDate, (*tripDB)[tripID].service_id)) {
                continue;
            }

            QJsonObject singleStopJSON;
            singleStopJSON["trip_id"]                = tripID;
            singleStopJSON["headsign"]               = (*tripDB)[tripID].trip_headsign;
            singleStopJSON["service_id"]             = serviceID;
            singleStopJSON["svc_start_date"]         = svc->getServiceStartDate(serviceID).toString("ddMMMyyyy");
            singleStopJSON["svc_end_date"]           = svc->getServiceEndDate(serviceID).toString("ddMMMyyyy");
            singleStopJSON["operate_days_condensed"] = svc->shortSerializeOpDays(serviceID);
            singleStopJSON["supplements_other_days"] = svc->serviceAddedOnOtherDates(serviceID);
            singleStopJSON["exceptions_present"]     = svc->serviceRemovedOnDates(serviceID);

            // Since it is the beginning of the trip's time that will be displayed, we assume it's not -1
            if (onlyDate.isNull()) {
                QTime localNoon    = QTime(12, 0, 0);
                QTime firstStopDep = localNoon.addSecs(tripIDwTime.second);
                singleStopJSON["first_stop_departure"] = firstStopDep.toString("hh:mm");
            } else {
                QDateTime localNoon(onlyDate, QTime(12, 0, 0), data->getAgencyTZ());
                QDateTime firstStopDep = localNoon.addSecs(tripIDwTime.second);
                singleStopJSON["first_stop_departure"] = firstStopDep.toString("hh:mm");
                singleStopJSON["dst_on"] = firstStopDep.isDaylightTime();
            }

            routeTripArray.push_back(singleStopJSON);
        }

        resp["trips"] = routeTripArray;

        // Note the lookup time
        resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
    }
    else {
        resp["error"] = 201;  // The route ID does not exist in the database
    }
}

void GtfsRequestProcessor::tripsServingStop(QString stopID, QDate onlyDate, QJsonObject &resp)
{
    QJsonArray  stopRouteArray;
    QDateTime           currUTC    = QDateTime::currentDateTimeUtc();
    const GTFS::Status *data       = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency = currUTC.toTimeZone(data->getAgencyTZ());
    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"] = "TSS";
    resp["error"]        = 0;
    resp["message_time"] = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // See if the route requested actually exists
    const GTFS::StopData *stops = GTFS::DataGateway::inst().getStopsDB();

    if (stops->contains(stopID)) {
        const GTFS::TripData     *tripDB    = GTFS::DataGateway::inst().getTripsDB();
        const GTFS::OperatingDay *svc       = GTFS::DataGateway::inst().getServiceDB();
        const GTFS::StopTimeData *stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
        const GTFS::RouteData    *routes    = GTFS::DataGateway::inst().getRoutesDB();

        resp["stop_id"]      = stopID;
        resp["stop_name"]    = (*stops)[stopID].stop_name;
        resp["stop_desc"]    = (*stops)[stopID].stop_desc;
        resp["parent_sta"]   = (*stops)[stopID].parent_station;
        resp["service_date"] = onlyDate.toString("ddd dd-MMM-yyyy");

        for (const QString routeID : (*stops)[stopID].stopTripsRoutes.keys()) {
            QJsonObject singleRouteJSON;
            QJsonArray routeTripArray;

            singleRouteJSON["route_id"]         = routeID;
            singleRouteJSON["route_short_name"] = (*routes)[routeID].route_short_name;
            singleRouteJSON["route_long_name"]  = (*routes)[routeID].route_long_name;

            for (qint32 tripLoopIdx = 0;
                 tripLoopIdx < (*stops)[stopID].stopTripsRoutes[routeID].length();
                 ++tripLoopIdx) {
                GTFS::tripStopSeqInfo tssi = (*stops)[stopID].stopTripsRoutes[routeID].at(tripLoopIdx);
                QString tripID      = tssi.tripID;
                qint32  stopTripIdx = tssi.tripStopIndex;

                // If only a certain day is requested, check if the service is actually running before appending a trip
                if (!onlyDate.isNull() && !svc->serviceRunning(onlyDate, (*tripDB)[tripID].service_id)) {
                    continue;
                }

                QJsonObject tripDetails;
                fillUnifiedTripDetailsForArray(tripID, stopTripIdx, svc, stopTimes, tripDB, onlyDate,
                                               false, false, currAgency, 0, tripDetails);

                routeTripArray.push_back(tripDetails);
            }

            // Add the individual route's worth of trips to the main array
            singleRouteJSON["trips"] = routeTripArray;

            // Add the whole route into the main return message
            stopRouteArray.push_back(singleRouteJSON);
        }

        resp["routes"] = stopRouteArray;

        // Note the lookup time
        resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
    }
    else {
        resp["error"] = 301;  // The trip ID does not exist in the database
    }
}

void GtfsRequestProcessor::stationDetailsDisplay(QString stopID, QJsonObject &resp)
{
    QJsonArray  stopRouteArray;
    QDateTime           currUTC    = QDateTime::currentDateTimeUtc();
    const GTFS::Status *data       = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency = currUTC.toTimeZone(data->getAgencyTZ());
    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"] = "STA";
    resp["error"]        = 0;
    resp["message_time"] = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // See if the route requested actually exists
    const GTFS::StopData       *stops  = GTFS::DataGateway::inst().getStopsDB();
    const GTFS::ParentStopData *parSta = GTFS::DataGateway::inst().getParentsDB();
    const GTFS::RouteData      *routes = GTFS::DataGateway::inst().getRoutesDB();

    QList<QString> routesServed;
    bool stopIsParent;

    if (parSta->contains(stopID)) {
        // If we find a parent station, populate all the routes for all of the "sub-stations" as well
        // TODO: Avoid duplicates when trips/routes serve several of the parent station's "platforms" / "sub-stops"
        for (const QString &subStop : (*parSta)[stopID]) {
            routesServed.append((*stops)[subStop].stopTripsRoutes.keys());
        }
        stopIsParent = true;
    } else if (stops->contains(stopID)) {
        // If it's a standalone station, populate the routes JUST for this station
        routesServed = (*stops)[stopID].stopTripsRoutes.keys();
        stopIsParent = false;
    } else {
        // StopID doesn't exist in either database, then an error must be raised
        resp["error"] = 401;  // The stop ID does not exist in the database
        return;
    }

    if (stops->contains(stopID)) {
        resp["stop_id"]    = stopID;
        resp["stop_name"]  = (*stops)[stopID].stop_name;
        resp["stop_desc"]  = (*stops)[stopID].stop_desc;
        resp["parent_sta"] = (*stops)[stopID].parent_station;
        resp["loc_lat"]    = (*stops)[stopID].stop_lat;
        resp["loc_lon"]    = (*stops)[stopID].stop_lon;

        // Find more details about all the routes served by the station
        for (const QString &routeID : routesServed) {
            QJsonObject singleRouteJSON;
            singleRouteJSON["route_id"]         = routeID;
            singleRouteJSON["route_short_name"] = (*routes)[routeID].route_short_name;
            singleRouteJSON["route_long_name"]  = (*routes)[routeID].route_long_name;
            stopRouteArray.push_back(singleRouteJSON);
        }
        resp["routes"] = stopRouteArray;

        // If there is a valid parent, display the other associated stations
        QJsonArray stopsSharingParent;
        QString parentStation = (stopIsParent) ? stopID : (*stops)[stopID].parent_station;
        if (parentStation != "") {
            for (const QString &subStopID : (*parSta)[parentStation]) {
                QJsonObject singleSubStop;
                singleSubStop["stop_id"]   = subStopID;
                singleSubStop["stop_name"] = (*stops)[subStopID].stop_name;
                singleSubStop["stop_desc"] = (*stops)[subStopID].stop_desc;
                stopsSharingParent.push_back(singleSubStop);
            }
        }
        resp["stops_sharing_parent"] = stopsSharingParent;

        // Note the lookup time
        resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
    }
}

void GtfsRequestProcessor::stopsServedByRoute(QString routeID, QJsonObject &resp)
{
    QJsonArray  routeStopArray;
    QDateTime           currUTC    = QDateTime::currentDateTimeUtc();
    const GTFS::Status *data       = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency = currUTC.toTimeZone(data->getAgencyTZ());
    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"] = "SSR";
    resp["error"]        = 0;
    resp["message_time"] = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    const GTFS::StopData  *stops  = GTFS::DataGateway::inst().getStopsDB();
    const GTFS::RouteData *routes = GTFS::DataGateway::inst().getRoutesDB();

    // See if the route requested actually exists
    if (routes->contains(routeID)) {
        resp["route_id"]         = routeID;
        resp["route_short_name"] = (*routes)[routeID].route_short_name;
        resp["route_long_name"]  = (*routes)[routeID].route_long_name;
        resp["route_desc"]       = (*routes)[routeID].route_desc;
        resp["route_type"]       = (*routes)[routeID].route_type;
        resp["route_url"]        = (*routes)[routeID].route_url;
        resp["route_color"]      = (*routes)[routeID].route_color;
        resp["route_text_color"] = (*routes)[routeID].route_text_color;

        // Find more details about all the routes served by the station
        for (const QString &stopID : (*routes)[routeID].stopService.keys()) {
            QJsonObject singleStopJSON;
            singleStopJSON["stop_id"]    = stopID;
            singleStopJSON["stop_name"]  = (*stops)[stopID].stop_name;
            singleStopJSON["stop_desc"]  = (*stops)[stopID].stop_desc;
            singleStopJSON["stop_lat"]   = (*stops)[stopID].stop_lat;
            singleStopJSON["stop_lon"]   = (*stops)[stopID].stop_lon;
            singleStopJSON["trip_count"] = (*routes)[routeID].stopService[stopID];
            routeStopArray.push_back(singleStopJSON);
        }
        resp["stops"] = routeStopArray;

        // Note the lookup time
        resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
    }
    else {
        resp["error"] = 501;  // The route ID does not exist in the database
    }
}

void GtfsRequestProcessor::nextTripsAtStop(QString      stopID,
                                           qint32       futureMinutes,
                                           qint32       maxTripsPerRoute,
                                           bool         realTimeOnly,
                                           QJsonObject &resp)
{
    // Inform global status that a request was received
    GTFS::DataGateway::inst().incrementHandledRequests();

    // Prepare for tripStopLoader
    QTimeZone agencyTimezone = GTFS::DataGateway::inst().getStatus()->getAgencyTZ();;
    QDate     serviceDate    = QDate::currentDate();
    QDateTime agencyTime     = QDateTime::currentDateTimeUtc().toTimeZone(agencyTimezone);
//    QDate serviceDate        = QDate(2019, 2, 8);
//    QDateTime agencyTime     = QDateTime(serviceDate, QTime(17, 21, 37), agencyTimezone);
    QDateTime beforeProcess  = QDateTime::currentDateTimeUtc().toTimeZone(agencyTimezone);

    // Real time processing
    GTFS::RealTimeTripUpdate *realTimeProc = GTFS::RealTimeGateway::inst().getActiveFeed();
    bool rtData = false;
    if (realTimeProc != NULL)
        rtData = true;

    // Boilerplate response details
    resp["message_type"] = "NEX";
    resp["error"]        = 0;
    resp["message_time"] = agencyTime.toString("dd-MMM-yyyy hh:mm:ss t");

    // Array to populate based on the response of the tripStopLoader
    GTFS::TripStopReconciler tripStopLoader(stopID, rtData, serviceDate, agencyTime, futureMinutes, maxTripsPerRoute);

    if (! tripStopLoader.stopIdExists()) {
        resp["error"] = 601;
        return;
    }

    resp["stop_id"]   = stopID;
    resp["stop_name"] = tripStopLoader.getStopName();
    resp["stop_desc"] = tripStopLoader.getStopDesciption();

    // Populate the valid upcoming routes with trips for the stop_id requested
    QJsonArray  stopRouteArray;
    QMap<QString, GTFS::StopRecoRouteRec> tripsForStopByRouteID;
    tripStopLoader.getTripsByRoute(tripsForStopByRouteID);

    // Loop over the responses to fill in the JSON responses
    for (const QString &routeID : tripsForStopByRouteID.keys()) {
        QJsonObject singleRouteJSON;
        singleRouteJSON["route_id"]         = routeID;
        singleRouteJSON["route_short_name"] = tripsForStopByRouteID[routeID].shortRouteName;
        singleRouteJSON["route_long_name"]  = tripsForStopByRouteID[routeID].longRouteName;
        singleRouteJSON["route_color"]      = tripsForStopByRouteID[routeID].routeColor;
        singleRouteJSON["route_text_color"] = tripsForStopByRouteID[routeID].routeTextColor;

        // Fetch the trips
        QJsonArray stopTrips;
        for (const GTFS::StopRecoTripRec &rts : tripsForStopByRouteID[routeID].tripRecos) {
            GTFS::TripRecStat tripStat = rts.tripStatus;

            if (tripStat == GTFS::IRRELEVANT)
                continue;

            if (realTimeOnly && (tripStat == GTFS::SCHEDULE || tripStat == GTFS::NOSCHEDULE))
                continue;

            QJsonObject stopTripItem;
            stopTripItem["trip_id"]         = rts.tripID;
            stopTripItem["dep_time"]        = rts.schDepTime.isNull() ? "-" : rts.schDepTime.toString("ddd hh:mm");
            stopTripItem["arr_time"]        = rts.schArrTime.isNull() ? "-" : rts.schArrTime.toString("ddd hh:mm");
            stopTripItem["wait_time_sec"]   = rts.waitTimeSec;
            stopTripItem["headsign"]        = rts.headsign;
            stopTripItem["pickup_type"]     = rts.pickupType;
            stopTripItem["drop_off_type"]   = rts.dropoffType;
            stopTripItem["trip_begins"]     = rts.beginningOfTrip;
            stopTripItem["trip_terminates"] = rts.endOfTrip;

            if (rts.realTimeDataAvail) {
                QJsonObject realTimeData;
                QString statusString;
                if (tripStat == GTFS::ARRIVE)
                    statusString = "ARRV";
                else if (tripStat == GTFS::BOARD)
                    statusString = "BRDG";
                else if (tripStat == GTFS::DEPART)
                    statusString = "DPRT";
                else if (tripStat == GTFS::ON_TIME)
                    statusString = "ONTM";
                else if (tripStat == GTFS::LATE)
                    statusString = "LATE";
                else if (tripStat == GTFS::EARLY)
                    statusString = "ERLY";
                else if (tripStat == GTFS::SUPPLEMENT)
                    statusString = "SPLM";
                else if (tripStat == GTFS::SKIP)
                    statusString = "SKIP";
                else if (tripStat == GTFS::CANCEL)
                    statusString = "CNCL";

                realTimeData["status"]         = statusString;
                realTimeData["actual_time"]    = rts.realTimeActual.toString("ddd hh:mm");
                realTimeData["offset_seconds"] = rts.realTimeOffsetSec;
                stopTripItem["realtime_data"]  = realTimeData;
            }

            stopTrips.push_back(stopTripItem);
        }

        singleRouteJSON["trips"] = stopTrips;
        stopRouteArray.push_back(singleRouteJSON);
    }

    // Attach the route list
    resp["routes"] = stopRouteArray;

    // Note the lookup time
    resp["proc_time_ms"] = beforeProcess.msecsTo(QDateTime::currentDateTimeUtc());
}

void GtfsRequestProcessor::stopsNoTrips(QJsonObject &resp)
{
    QJsonArray  stopArray;
    QDateTime                   currUTC    = QDateTime::currentDateTimeUtc();
    const GTFS::Status         *data       = GTFS::DataGateway::inst().getStatus();
    const GTFS::ParentStopData *par        = GTFS::DataGateway::inst().getParentsDB();
    const GTFS::StopData       *stops      = GTFS::DataGateway::inst().getStopsDB();
    QDateTime                   currAgency = currUTC.toTimeZone(data->getAgencyTZ());
    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"] = "SNT";
    resp["error"]        = 0;
    resp["message_time"] = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // Run through the entire stops database and find any that have no trips associated with them
    for (const QString &stopID : (*stops).keys()) {
        if (par->contains(stopID)) {
            // Don't bother listing parent stations, as their childless children will be printed anyway
            continue;
        }

        if ((*stops)[stopID].stopTripsRoutes.size() == 0) {
            QJsonObject unassocStop;
            unassocStop["stop_id"]    = stopID;
            unassocStop["stop_name"]  = (*stops)[stopID].stop_name;
            unassocStop["stop_desc"]  = (*stops)[stopID].stop_desc;
            unassocStop["loc_lat"]    = (*stops)[stopID].stop_lat;
            unassocStop["loc_lon"]    = (*stops)[stopID].stop_lon;
            unassocStop["parent_sta"] = (*stops)[stopID].parent_station;
            stopArray.push_back(unassocStop);
        }
    }

    resp["stops"] = stopArray;
}

void GtfsRequestProcessor::realtimeDataStatus(QJsonObject &resp)
{
    const GTFS::Status    *data       = GTFS::DataGateway::inst().getStatus();
    GTFS::RealTimeGateway &rg         = GTFS::RealTimeGateway::inst();
    QDateTime              currUTC    = QDateTime::currentDateTimeUtc();
    QDateTime              currAgency = currUTC.toTimeZone(data->getAgencyTZ());

    resp["message_type"]  = "RDS";   // Indicate it is a status message
    resp["error"]         = 0;
    resp["message_time"]  = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    resp["seconds_to_next_fetch"]    = rg.secondsToFetch();

    QString activeSideStr, inactiveSideStr;
    GTFS::RealTimeDataRepo    active = rg.activeBuffer();
    GTFS::RealTimeTripUpdate *rTrips = rg.getActiveFeed();
    if (active == GTFS::SIDE_A) {
        activeSideStr   = "A";
        inactiveSideStr = "B";
    } else if (active == GTFS::SIDE_B) {
        activeSideStr   = "B";
        inactiveSideStr = "A";
    } else {
        activeSideStr = inactiveSideStr = "N/A";
    }

    if (rTrips == NULL) {
        resp["active_side"] = activeSideStr;
    } else {
        // Active and inactive data information
        QDateTime activeFeedTime        = rTrips->getFeedTime();
        resp["active_side"]             = activeSideStr;
        resp["active_age_sec"]          = activeFeedTime.secsTo(currUTC);
        resp["active_feed_time"]        = activeFeedTime.toString("ddMMMyyyy hh:mm:ss t");
        resp["active_download_ms"]      = rTrips->getDownloadTimeMSec();
        resp["active_integration_ms"]   = rTrips->getIntegrationTimeMSec();

        // Cancelled Trips


        // Added Trips


        // Running Trips

    }

    // Note how long query took
    resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
}

void fillUnifiedTripDetailsForArray(const QString            &tripID,
                                    qint32                    stopTripIdx,
                                    const GTFS::OperatingDay *svc,
                                    const GTFS::StopTimeData *stopTimes,
                                    const GTFS::TripData     *tripDB,
                                    const QDate              &serviceDate,
                                    bool                      skipServiceDetail,
                                    bool                      addWaitTime,
                                    const QDateTime          &currAgency,
                                    qint64                    waitTimeSec,
                                    QJsonObject              &singleStopJSON)
{
    QString serviceID = (*tripDB)[tripID].service_id;
    singleStopJSON["trip_id"]       = tripID;
    singleStopJSON["headsign"]      = ((*stopTimes)[tripID].at(stopTripIdx).stop_headsign != "")
                                         ? (*stopTimes)[tripID].at(stopTripIdx).stop_headsign
                                         : (*tripDB)[tripID].trip_headsign;
    singleStopJSON["drop_off_type"] = (*stopTimes)[tripID].at(stopTripIdx).drop_off_type;
    singleStopJSON["pickup_type"]   = (*stopTimes)[tripID].at(stopTripIdx).pickup_type;

    // The service (applicable days, dates, exceptions, additions) details of each trip are not very interesting
    // on transactions where a specific time is requested, so we provide the option to NOT bother sending it in the
    // interest of saving data transport needs
    if (!skipServiceDetail) {
        singleStopJSON["service_id"]             = serviceID;
        singleStopJSON["svc_start_date"]         = svc->getServiceStartDate(serviceID).toString("ddMMMyyyy");
        singleStopJSON["svc_end_date"]           = svc->getServiceEndDate(serviceID).toString("ddMMMyyyy");
        singleStopJSON["operate_days_condensed"] = svc->shortSerializeOpDays(serviceID);
        singleStopJSON["supplements_other_days"] = svc->serviceAddedOnOtherDates(serviceID);
        singleStopJSON["exceptions_present"]     = svc->serviceRemovedOnDates(serviceID);
    }

    // Fill the time of service. Note that it could be distinct from the countdown when we have no scheduled time(s)!
    if (serviceDate.isNull()) {
        // No particular day requested, we will just show the "offset" times
        QTime localNoon(12, 0, 0);
        if ((*stopTimes)[tripID].at(stopTripIdx).departure_time != -1) {
            QTime stopDep = localNoon.addSecs((*stopTimes)[tripID].at(stopTripIdx).departure_time);
            singleStopJSON["dep_time"] = stopDep.toString("hh:mm");
        } else {
            singleStopJSON["dep_time"] = "-";
        }
        if ((*stopTimes)[tripID].at(stopTripIdx).arrival_time != -1) {
            QTime stopArr = localNoon.addSecs((*stopTimes)[tripID].at(stopTripIdx).arrival_time);
            singleStopJSON["arr_time"] = stopArr.toString("hh:mm");
        } else {
            singleStopJSON["arr_time"] = "-";
        }
    } else {
        // A service day was specified so we should respect the actual DateTime (in case of daylight saving, etc.)
        // Given that we are using QDateTime creation from a QDate and a QTime, we SHOULD be immune from time-zone
        // and daylight-saving bugs
        QTime     localNoon(12, 0, 0);
        QDateTime localNoonDT(serviceDate, localNoon, currAgency.timeZone());

//        localNoonDT = localNoonDT.toUTC();

        if ((*stopTimes)[tripID].at(stopTripIdx).departure_time != -1) {
            QDateTime stopDep = localNoonDT.addSecs((*stopTimes)[tripID].at(stopTripIdx).departure_time);
            singleStopJSON["dep_time"] = stopDep.toString("hh:mm");
            singleStopJSON["dst_on"]   = stopDep.isDaylightTime();
        } else {
            singleStopJSON["dep_time"] = "-";
        }
        if ((*stopTimes)[tripID].at(stopTripIdx).arrival_time != -1) {
            QDateTime stopArr = localNoonDT.addSecs((*stopTimes)[tripID].at(stopTripIdx).arrival_time);
            singleStopJSON["arr_time"] = stopArr.toString("hh:mm");
            singleStopJSON["dst_on"]   = stopArr.isDaylightTime();
        } else {
            singleStopJSON["arr_time"] = "-";
        }
    }

    // For the "NEX" display, we need to actually show how long somebody will wait
    // Just showing a time in the NEX display is also ambiguous, so we should also add the ACTUAL day with it
    if (addWaitTime) {
        singleStopJSON["wait_time_sec"] = waitTimeSec;

        QDateTime tripStopTime = currAgency.addSecs(waitTimeSec);
        singleStopJSON["actual_day"]    = tripStopTime.toString("ddd");
    }

    // Indicate that the trip actually terminates at the requested stop (i.e. a client may wish to omit
    // service that stops running at the current station because nobody could board that trip.
    // DO NOTE HOWEVER: Some trips (depending on the agency, i.e. SEPTA Regional Rail) terminate in
    // Center City, but they actually continue on under a different route and trip ID. Some agencies account
    // for this using the drop_off_type and pickup_type.
    if (stopTripIdx == (*stopTimes)[tripID].length() - 1) {
        singleStopJSON["trip_terminates"] = true;
    } else {
        singleStopJSON["trip_terminates"] = false;
    }

    // ... and just for sanity, it is also helpful to know if the stop is at the beginning of the trip in question
    // since some agencies put a flag that the start of a trip will not drop off any passengers, but the "flag" to
    // indicate that might otherwise standout despite how normal it is to do. This hopes to provide a rationale.
    if (stopTripIdx == 0) {
        singleStopJSON["trip_begins"] = true;
    } else {
        singleStopJSON["trip_begins"] = false;
    }
}

QDate GtfsRequestProcessor::determineServiceDay(const QString &userReq, QString &remUserQuery)
{
    // First space indicates the separation between the date request and the remainder of the user query
    QString dayDate = userReq.left(userReq.indexOf(" "));
    QDateTime userDate;
    QDate     userDateLocale;

    const GTFS::Status *data  = GTFS::DataGateway::inst().getStatus();
    const QTimeZone &agencyTZ = data->getAgencyTZ();

    if (!dayDate.compare("D", Qt::CaseInsensitive)) {
        userDate = QDateTime::currentDateTimeUtc();
        userDateLocale = userDate.toTimeZone(agencyTZ).date();
    } else if (!dayDate.compare("Y", Qt::CaseInsensitive)) {
        userDate = QDateTime::currentDateTimeUtc();
        userDateLocale = userDate.toTimeZone(agencyTZ).addDays(-1).date();
    } else if (!dayDate.compare("T", Qt::CaseInsensitive)) {
        userDate = QDateTime::currentDateTimeUtc();
        userDateLocale = userDate.toTimeZone(agencyTZ).addDays(1).date();
    } else {
        // Assume that they wrote a date correctly
        // TODO: WAY MORE ROBUST ERROR HANDLING
        userDateLocale = QDate::fromString(dayDate, "ddMMMyyyy");
    }

    remUserQuery = userReq.mid(userReq.indexOf(" ") + 1);

    return userDateLocale;
}

qint32 GtfsRequestProcessor::determineMinuteRange(const QString &userReq, QString &remUserQuery)
{
    // First space determines the amount of time to request
    qint32 futureMinutes = userReq.left(userReq.indexOf(" ")).toInt();
    remUserQuery = userReq.mid(userReq.indexOf(" ") + 1);
    return futureMinutes;
}
