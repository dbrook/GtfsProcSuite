#include "gtfsrequestprocessor.h"
#include "datagateway.h"

#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThreadPool>
#include <QTime>

#include <QDebug>


/*
 * For Trips Serving Stop (TSS/TSD) and Upcoming Trips (NEX) there was a great deal of repeated code
 */
// Determine the ideal time to use to determine if a trip occurs before/after the requested time range
static qint32 determineServiceTime(qint32 departureTime, qint32 arrivalTime, qint32 sortTime);


// Append a valid trip belonging to a route to the response
static void addTripToRouteArray(const QString                                   &tripID,
                                qint32                                           stopTripIdx,
                                const GTFS::OperatingDay                        *svc,
                                const QMap<QString, QVector<GTFS::StopTimeRec>> *stopTimes,
                                const QMap<QString, GTFS::TripRec>              *tripDB,
                                bool                                             skipServiceDetail,
                                bool                                             addWaitTime, qint32 waitTime,
                                QJsonArray                                      &routeTripArray);


GtfsRequestProcessor::GtfsRequestProcessor(QString userRequest) : request(userRequest)
{

}

/*
 * Application Kick-Off
 */
void GtfsRequestProcessor::run()
{
    QString SystemResponse;

    qDebug() << "GtfsRequestProcessor: Answering on thread " << this;

    // The first three chars are assumed to be that of the application request
    QString userApp = this->request.left(3);  // 3-digit application code
    QString userReq = this->request.mid(4);   // Remainder of request (syntax varies)

    // Decode the application request
    if (! userApp.compare("SDS", Qt::CaseInsensitive)) {
        SystemResponse = this->applicationStatus();
    } else if (! userApp.compare("RTE", Qt::CaseInsensitive)) {
        SystemResponse = this->availableRoutes();
    } else if (! userApp.compare("TRI", Qt::CaseInsensitive)) {
        SystemResponse = this->tripStopsDisplay(userReq);
    } else if (! userApp.compare("TSR", Qt::CaseInsensitive)) {
        QDate noDate;
        SystemResponse = this->tripsServingRoute(userReq, noDate);
    } else if (! userApp.compare("TRD", Qt::CaseInsensitive)) {
        QString remainingReq;
        QDate reqDate = determineServiceDay(userReq, remainingReq);
        SystemResponse = this->tripsServingRoute(remainingReq, reqDate);
    } else if (! userApp.compare("TSS", Qt::CaseInsensitive)) {
        QDate noDate;
        SystemResponse = this->tripsServingStop(userReq, noDate);
    } else if (! userApp.compare("TSD", Qt::CaseInsensitive)) {
        QString remainingReq;
        QDate reqDate = determineServiceDay(userReq, remainingReq);
        SystemResponse = this->tripsServingStop(remainingReq, reqDate);
    } else if (! userApp.compare("STA", Qt::CaseInsensitive)) {
        SystemResponse = this->stationDetailsDisplay(userReq);
    } else if (! userApp.compare("SSR", Qt::CaseInsensitive)) {
        SystemResponse = this->stopsServedByRoute(userReq);
    } else if (! userApp.compare("NEX", Qt::CaseInsensitive)) {
        QString remainingReq;
        qint32 futureMinutes = determineMinuteRange(userReq, remainingReq);
        SystemResponse = this->nextTripsAtStop(remainingReq, futureMinutes);
    } else {
        // Return ERROR 1: Unknown request (userApp)
        SystemResponse = "{\"error\":\"1\",\"user_string\":\"" + this->request + "\"}";
    }

    // (Note: we append "\n" so clients can detect the end of the stream)
    emit Result(SystemResponse + "\n");
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * INDIVIDUAL SERVER-SIDE APPLICATIONS
 *
 * THIS WILL PROBABLY GET TOO HUGE TO DO JUST HERE, SO REFACTORING IS ALMOST GUARANTEED
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * "STA" --> Application Status
 */
QString GtfsRequestProcessor::applicationStatus()
{
    QJsonObject         resp;
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

    // Serialize for transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);
}

QString GtfsRequestProcessor::availableRoutes()
{
    QJsonObject         resp;
    QJsonArray          routeArray;
    QDateTime           currUTC    = QDateTime::currentDateTimeUtc();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    GTFS::DataGateway::inst().incrementHandledRequests();

    resp["message_type"]     = "RTE";   // Indicate it is a status message
    resp["error"]            = 0;                                         // TODO: Nice and robust error code system :-)
    resp["message_time"]     = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // Fill routes associated to each agency (TODO: integrate the two... for now just dump JSON to play with)
    const QMap<QString, GTFS::RouteRec> *routes = GTFS::DataGateway::inst().getRoutesDB();
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

    // Serialize for transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);
}

QString GtfsRequestProcessor::tripStopsDisplay(QString tripID)
{
    QJsonObject resp;
    QJsonArray  tripStopArray;
    QDateTime   currUTC       = QDateTime::currentDateTimeUtc();
    GTFS::DataGateway::inst().incrementHandledRequests();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    resp["message_type"]       = "TRI";
    resp["error"]              = 0;
    resp["message_time"]       = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // Fill the information for the trip from the data structures
    const QMap<QString, GTFS::TripRec> *tripDB    = GTFS::DataGateway::inst().getTripsDB();
    const GTFS::OperatingDay           *serviceDB = GTFS::DataGateway::inst().getServiceDB();

    // Make sure the requested Trip ID actually Exists (otherwise we will make an entry for EVERY SINGLE REQUEST :-O)
    if ((*tripDB).contains(tripID)) {
        // The Trip ID actually exists so fill the JSON
        resp["route_id"]           = (*tripDB)[tripID].route_id;
        resp["trip_id"]            = tripID;
        resp["headsign"]           = (*tripDB)[tripID].trip_headsign;
        resp["service_id"]         = (*tripDB)[tripID].service_id;
        resp["operate_days"]       = serviceDB->serializeOpDays((*tripDB)[tripID].service_id);

        // Provide the associated route name / details to the response
        const QMap<QString, GTFS::RouteRec> *routes = GTFS::DataGateway::inst().getRoutesDB();
        resp["route_short_name"] = (*routes)[(*tripDB)[tripID].route_id].route_short_name;
        resp["route_long_name"]  = (*routes)[(*tripDB)[tripID].route_id].route_long_name;

        // String together all the dates in range for which the trip_id requested SPECIFICALLY does not operate
        resp["exception_dates"]  = serviceDB->serializeNoServiceDates((*tripDB)[tripID].service_id);

        // String together all the dates in range for which the trip_id requested operates additionally
        resp["added_dates"]  = serviceDB->serializeAddedServiceDates((*tripDB)[tripID].service_id);

        // Provide the validity range
        resp["svc_start_date"] = serviceDB->getServiceStartDate((*tripDB)[tripID].service_id).toString("dd-MMM-yyyy");
        resp["svc_end_date"]   = serviceDB->getServiceEndDate((*tripDB)[tripID].service_id).toString("dd-MMM-yyyy");

        // Populate information about the trip's actual stopping points and times
        const QMap<QString, GTFS::StopRec> *stops = GTFS::DataGateway::inst().getStopsDB();
        const QMap<QString, QVector<GTFS::StopTimeRec>> *stopTimes = GTFS::DataGateway::inst().getStopTimesDB();

        const QVector<GTFS::StopTimeRec> tripStops = (*stopTimes)[tripID];

        for (GTFS::StopTimeRec stop : tripStops) {
            QJsonObject singleStopJSON;
            QTime localMidnight(0, 0, 0);

            if (stop.arrival_time != -1) {
                QTime arrivalTime = localMidnight.addSecs(stop.arrival_time);
                singleStopJSON["arr_time"] = arrivalTime.toString("hh:mm");
            } else {
                singleStopJSON["arr_time"] = "-";
            }

            if (stop.departure_time != -1) {
                QTime departureTime = localMidnight.addSecs(stop.departure_time);
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

        resp["stops"] = tripStopArray;

        // Note the lookup time
        resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
    }
    else {
        resp["error"] = 101;   // The trip ID doesn't exist in the database
    }

    // Serialize for Transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);
}

QString GtfsRequestProcessor::tripsServingRoute(QString routeID, QDate onlyDate)
{
    QJsonObject resp;
    QJsonArray  routeTripArray;
    QDateTime   currUTC        = QDateTime::currentDateTimeUtc();
    GTFS::DataGateway::inst().incrementHandledRequests();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    resp["message_type"]       = "TSR";
    resp["error"]              = 0;
    resp["message_time"]       = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // See if the route requested actually exists
    const QMap<QString, GTFS::RouteRec> *routes = GTFS::DataGateway::inst().getRoutesDB();

    if (routes->contains(routeID)) {
        const QMap<QString, GTFS::TripRec>              *tripDB    = GTFS::DataGateway::inst().getTripsDB();
        const GTFS::OperatingDay                        *svc       = GTFS::DataGateway::inst().getServiceDB();

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
            singleStopJSON["trip_id"]                = (*tripDB)[tripID].trip_id;
            singleStopJSON["headsign"]               = (*tripDB)[tripID].trip_headsign;
            singleStopJSON["service_id"]             = serviceID;
            singleStopJSON["svc_start_date"]         = svc->getServiceStartDate(serviceID).toString("ddMMMyyyy");
            singleStopJSON["svc_end_date"]           = svc->getServiceEndDate(serviceID).toString("ddMMMyyyy");
            singleStopJSON["operate_days_condensed"] = svc->shortSerializeOpDays(serviceID);
            singleStopJSON["supplements_other_days"] = svc->serviceAddedOnOtherDates(serviceID);
            singleStopJSON["exceptions_present"]     = svc->serviceRemovedOnDates(serviceID);

            // Since it is the beginning of the trip's time that will be displayed, we assume it's not -1
            QTime localMidnight(0, 0, 0);
            QTime firstStopDep = localMidnight.addSecs(tripIDwTime.second);
            singleStopJSON["first_stop_departure"]   = firstStopDep.toString("hh:mm");

            routeTripArray.push_back(singleStopJSON);
        }

        resp["trips"] = routeTripArray;

        // Note the lookup time
        resp["proc_time_ms"] = currUTC.msecsTo(QDateTime::currentDateTimeUtc());
    }
    else {
        resp["error"] = 201;  // The route ID does not exist in the database
    }

    // Serialize for Transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);
}

QString GtfsRequestProcessor::tripsServingStop(QString stopID, QDate onlyDate)
{
    QJsonObject resp;
    QJsonArray  stopRouteArray;
    QDateTime   currUTC        = QDateTime::currentDateTimeUtc();
    GTFS::DataGateway::inst().incrementHandledRequests();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    resp["message_type"]       = "TSS";
    resp["error"]              = 0;
    resp["message_time"]       = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // See if the route requested actually exists
    const QMap<QString, GTFS::StopRec> *stops = GTFS::DataGateway::inst().getStopsDB();

    if (stops->contains(stopID)) {
        const QMap<QString, GTFS::TripRec>              *tripDB    = GTFS::DataGateway::inst().getTripsDB();
        const GTFS::OperatingDay                        *svc       = GTFS::DataGateway::inst().getServiceDB();
        const QMap<QString, QVector<GTFS::StopTimeRec>> *stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
        const QMap<QString, GTFS::RouteRec>             *routes    = GTFS::DataGateway::inst().getRoutesDB();

        resp["stop_id"]   = stopID;
        resp["stop_name"] = (*stops)[stopID].stop_name;
        resp["stop_desc"] = (*stops)[stopID].stop_desc;
        resp["parent_sta"] = (*stops)[stopID].parent_station;
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
                qint32  stopTripIdx = tssi.tripStopSeq;

                // If only a certain day is requested, check if the service is actually running before appending a trip
                if (!onlyDate.isNull() && !svc->serviceRunning(onlyDate, (*tripDB)[tripID].service_id)) {
                    continue;
                }

                addTripToRouteArray(tripID, stopTripIdx, svc, stopTimes, tripDB, false, false, 0, routeTripArray);
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

    // Serialize for Transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);
}

QString GtfsRequestProcessor::stationDetailsDisplay(QString stopID)
{
    QJsonObject resp;
    QJsonArray  stopRouteArray;
    QDateTime   currUTC = QDateTime::currentDateTimeUtc();
    GTFS::DataGateway::inst().incrementHandledRequests();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    resp["message_type"]       = "STA";
    resp["error"]              = 0;
    resp["message_time"]       = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // See if the route requested actually exists
    const QMap<QString, GTFS::StopRec>    *stops  = GTFS::DataGateway::inst().getStopsDB();
    const QMap<QString, QVector<QString>> *parSta = GTFS::DataGateway::inst().getParentStationDB();
    const QMap<QString, GTFS::RouteRec>   *routes = GTFS::DataGateway::inst().getRoutesDB();

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
        QJsonDocument jdoc(resp);
        return jdoc.toJson(QJsonDocument::Compact);
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

    // Serialize for Transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);
}

QString GtfsRequestProcessor::stopsServedByRoute(QString routeID)
{
    QJsonObject resp;
    QJsonArray  routeStopArray;
    QDateTime   currUTC = QDateTime::currentDateTimeUtc();
    GTFS::DataGateway::inst().incrementHandledRequests();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    resp["message_type"]       = "SSR";
    resp["error"]              = 0;
    resp["message_time"]       = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    const QMap<QString, GTFS::StopRec>    *stops  = GTFS::DataGateway::inst().getStopsDB();
    const QMap<QString, GTFS::RouteRec>   *routes = GTFS::DataGateway::inst().getRoutesDB();

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

    // Serialize for Transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);
}

QString GtfsRequestProcessor::nextTripsAtStop(QString stopID, qint32 futureMinutes)
{
    QJsonObject resp;
    QJsonArray  stopRouteArray;
    QDateTime   currUTC        = QDateTime::currentDateTimeUtc();
    GTFS::DataGateway::inst().incrementHandledRequests();
    const GTFS::Status *data        = GTFS::DataGateway::inst().getStatus();
    QDateTime           currAgency  = currUTC.toTimeZone(data->getAgencyTZ());

    // Determine the range of time for which we must look for trips - use the local (transit agency's) time
    // We have to look back to the previous operating day ALWAYS (well ... ok, it's easier to always do it...) in order
    // to cover scenarios where we are after midnight but trips from the previous operating day are still active
    QDate prevDay = QDate::currentDate().addDays(-1);
    QDateTime prevOpDayStart(prevDay);
    prevOpDayStart.setTimeZone(data->getAgencyTZ());
    qint32 curSecSinceStartYesterday = prevOpDayStart.secsTo(currAgency);
    qint32 totSecSinceStartYesterday = curSecSinceStartYesterday + futureMinutes * 60;

    // Of course process any of the current service day's trips
    QDate today  = QDate::currentDate();
    QDateTime todaysOpDayStart(today);
    todaysOpDayStart.setTimeZone(data->getAgencyTZ());
    qint32 curSecSinceStartToday     = todaysOpDayStart.secsTo(currAgency);
    qint32 totSecSinceStartToday     = curSecSinceStartToday + futureMinutes * 60;

    // ... and lastly, anything that is running on tomorrow's service since we can request into the future, that future
    // could include an additional service day. We have to lop off any remaining seconds to the end of today when
    // applying future time in tomorrow's service day.
    QDate tomorrow = QDate::currentDate().addDays(1);
    QDateTime tomorrowOpDayStart(tomorrow);
    tomorrowOpDayStart.setTimeZone(data->getAgencyTZ());
    qint32 secUntilTomorrow          = currAgency.secsTo(tomorrowOpDayStart);
    qint32 totSecSinceTomorrow       = futureMinutes * 60 - secUntilTomorrow;

    resp["message_type"]       = "NEX";
    resp["error"]              = 0;
    resp["message_time"]       = currAgency.toString("dd-MMM-yyyy hh:mm:ss t");

    // See if the route requested actually exists
    const QMap<QString, GTFS::StopRec> *stops = GTFS::DataGateway::inst().getStopsDB();

    if (stops->contains(stopID)) {
        const QMap<QString, GTFS::TripRec>              *tripDB    = GTFS::DataGateway::inst().getTripsDB();
        const GTFS::OperatingDay                        *svc       = GTFS::DataGateway::inst().getServiceDB();
        const QMap<QString, QVector<GTFS::StopTimeRec>> *stopTimes = GTFS::DataGateway::inst().getStopTimesDB();
        const QMap<QString, GTFS::RouteRec>             *routes    = GTFS::DataGateway::inst().getRoutesDB();

        resp["stop_id"]   = stopID;
        resp["stop_name"] = (*stops)[stopID].stop_name;
        resp["stop_desc"] = (*stops)[stopID].stop_desc;

        for (const QString routeID : (*stops)[stopID].stopTripsRoutes.keys()) {
            QJsonObject singleRouteJSON;
            QJsonArray routeTripArray;

            singleRouteJSON["route_id"]         = routeID;
            singleRouteJSON["route_short_name"] = (*routes)[routeID].route_short_name;
            singleRouteJSON["route_long_name"]  = (*routes)[routeID].route_long_name;
            singleRouteJSON["route_color"]      = (*routes)[routeID].route_color;
            singleRouteJSON["route_text_color"] = (*routes)[routeID].route_text_color;

            /*
             * Let's find all the trips that ran yesterday which could still be running (after-midnight trips)
             */
            for (qint32 tripLoopIdx = 0;
                 tripLoopIdx < (*stops)[stopID].stopTripsRoutes[routeID].length();
                 ++tripLoopIdx) {
                GTFS::tripStopSeqInfo tssi = (*stops)[stopID].stopTripsRoutes[routeID].at(tripLoopIdx);
                QString tripID      = tssi.tripID;
                qint32  stopTripIdx = tssi.tripStopSeq;
                qint32  serviceTime = determineServiceTime((*stopTimes)[tripID].at(stopTripIdx).departure_time,
                                                           (*stopTimes)[tripID].at(stopTripIdx).arrival_time,
                                                           tssi.sortTime);

                // Skip trips that we don't care about showing
                if (!svc->serviceRunning(prevDay, (*tripDB)[tripID].service_id)) {
                    // Trip didn't run this day
                    continue;
                } else if (serviceTime < curSecSinceStartYesterday) {
                    // Trip has arrived / departed already
                    continue;
                } else if (serviceTime > totSecSinceStartYesterday) {
                    // Trip arrival / departure occurs outside of the requested time range
                    continue;
                }

                qint32 countdown = serviceTime - curSecSinceStartYesterday;
                addTripToRouteArray(tripID, stopTripIdx, svc, stopTimes, tripDB, true, true, countdown, routeTripArray);
            }

            /*
             * Now rerun the scan of the trips for today's service day
             */
            for (qint32 tripLoopIdx = 0;
                 tripLoopIdx < (*stops)[stopID].stopTripsRoutes[routeID].length();
                 ++tripLoopIdx) {
                GTFS::tripStopSeqInfo tssi = (*stops)[stopID].stopTripsRoutes[routeID].at(tripLoopIdx);
                QString tripID      = tssi.tripID;
                qint32  stopTripIdx = tssi.tripStopSeq;
                qint32  serviceTime = determineServiceTime((*stopTimes)[tripID].at(stopTripIdx).departure_time,
                                                           (*stopTimes)[tripID].at(stopTripIdx).arrival_time,
                                                           tssi.sortTime);

                // Skip trips that we don't care about showing
                if (!svc->serviceRunning(today, (*tripDB)[tripID].service_id)) {
                    // Trip didn't run this day
                    continue;
                } else if (serviceTime < curSecSinceStartToday) {
                    // Trip has arrived / departed already
                    continue;
                } else if (serviceTime > totSecSinceStartToday) {
                    // Trip arrival / departure occurs outside of the requested time range
                    continue;
                }

                qint32 countdown = serviceTime - curSecSinceStartToday;
                addTripToRouteArray(tripID, stopTripIdx, svc, stopTimes, tripDB, true, true, countdown, routeTripArray);
            }

            /*
             * Finally, rerun the scan of the trips for tomorrow's service day
             */
            for (qint32 tripLoopIdx = 0;
                 tripLoopIdx < (*stops)[stopID].stopTripsRoutes[routeID].length();
                 ++tripLoopIdx) {
                GTFS::tripStopSeqInfo tssi = (*stops)[stopID].stopTripsRoutes[routeID].at(tripLoopIdx);
                QString tripID      = tssi.tripID;
                qint32  stopTripIdx = tssi.tripStopSeq;
                qint32  serviceTime = determineServiceTime((*stopTimes)[tripID].at(stopTripIdx).departure_time,
                                                           (*stopTimes)[tripID].at(stopTripIdx).arrival_time,
                                                           tssi.sortTime);

                // Skip trips that we don't care about showing
                if (!svc->serviceRunning(tomorrow, (*tripDB)[tripID].service_id)) {
                    // Trip didn't run this day
                    continue;
                } else if (serviceTime > totSecSinceTomorrow) {
                    // *** NOTE: We assume that no trips of "tomorrow's" operating day can have completed yet
                    //           (otherwise "tomorrow" would actually be today ... good luck figuring out what I meant!)
                    // Trip arrival / departure occurs outside of the requested time range
                    continue;
                }

                qint32 countdown = serviceTime + secUntilTomorrow;
                addTripToRouteArray(tripID, stopTripIdx, svc, stopTimes, tripDB, true, true, countdown, routeTripArray);
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
        resp["error"] = 601;  // The trip ID does not exist in the database
    }

    // Serialize for Transport
    QJsonDocument jdoc(resp);
    return jdoc.toJson(QJsonDocument::Compact);

}

qint32 determineServiceTime(qint32 departureTime, qint32 arrivalTime, qint32 sortTime)
{
    /*
     * There are several notions of time we could compare with the current system time, so let's clarify
     * the order in which we will evaluate:
     * 1) Departure time is the most common, indeed, we care about being able to REACH the trip-stop
     *    before it departs.
     * 2) Arrival time might be different (if it's defined at all), but it is less interesting for a
     *    countdown-style display which this transaction tends to be.
     * 3) If neither time was defined, it was probably one of the stops which is between two defined
     *    times (this is referred to as the "sort time" elsewhere), so we will compare on that as a last
     *    resort. Note that the sort time is never displayed, but for the sake of computing whether or not
     *    the trip should still display on a countdown, it might be the only option we have!
     */
    if (departureTime != -1)
        return departureTime;
    else if (arrivalTime != -1)
        return arrivalTime;
    else
        return sortTime;
}

void addTripToRouteArray(const QString                                   &tripID,
                         qint32                                           stopTripIdx,
                         const GTFS::OperatingDay                        *svc,
                         const QMap<QString, QVector<GTFS::StopTimeRec>> *stopTimes,
                         const QMap<QString, GTFS::TripRec>              *tripDB,
                         bool                                             skipServiceDetail,
                         bool                                             addWaitTime,
                         qint32                                           waitTime,
                         QJsonArray                                      &routeTripArray)
{
    QJsonObject singleStopJSON;
    QString serviceID = (*tripDB)[tripID].service_id;
    singleStopJSON["trip_id"]                = (*tripDB)[tripID].trip_id;
    singleStopJSON["headsign"]               = ((*stopTimes)[tripID].at(stopTripIdx).stop_headsign != "")
                                                  ? (*stopTimes)[tripID].at(stopTripIdx).stop_headsign
                                                  : (*tripDB)[tripID].trip_headsign;
    singleStopJSON["drop_off_type"]          = (*stopTimes)[tripID].at(stopTripIdx).drop_off_type;
    singleStopJSON["pickup_type"]            = (*stopTimes)[tripID].at(stopTripIdx).pickup_type;

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

    // TODO: This may be a problem! We can have stops with no stop time (i.e. the time should be
    //       considered as part of a whole trip and it not a 'reference' stop with a full time.
    //       Showing times for a non-reference stop means no times will ever be displayed!
    QTime localMidnight(0, 0, 0);
    if ((*stopTimes)[tripID].at(stopTripIdx).departure_time != -1) {
        QTime stopDep = localMidnight.addSecs((*stopTimes)[tripID].at(stopTripIdx).departure_time);
        singleStopJSON["dep_time"] = stopDep.toString("hh:mm");
    } else {
        singleStopJSON["dep_time"] = "-";
    }
    if ((*stopTimes)[tripID].at(stopTripIdx).arrival_time != -1) {
        QTime stopArr = localMidnight.addSecs((*stopTimes)[tripID].at(stopTripIdx).arrival_time);
        singleStopJSON["arr_time"] = stopArr.toString("hh:mm");
    } else {
        singleStopJSON["arr_time"] = "-";
    }

    if (addWaitTime) {
        singleStopJSON["wait_time_sec"] = waitTime;
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

    // Add this individual trip to the route
    routeTripArray.push_back(singleStopJSON);
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
