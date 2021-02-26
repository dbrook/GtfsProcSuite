/*
 * GtfsProc_Server
 * Copyright (C) 2018-2021, Daniel Brook
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

// GTFS logical modules
#include "staticstatus.h"
#include "availableroutes.h"
#include "tripscheduledisplay.h"
#include "tripstopreconciler.h"
#include "tripsservingroute.h"
#include "tripsservingstop.h"
#include "stationdetailsdisplay.h"
#include "stopsservedbyroute.h"
#include "stopswithouttrips.h"
#include "realtimetripinformation.h"
#include "realtimestatus.h"
#include "realtimeproductstatus.h"
#include "upcomingstopservice.h"
#include "servicebetweenstops.h"

// Qt Framework Dependencies
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThreadPool>
#include <QTime>
#include <QDebug>

GtfsRequestProcessor::GtfsRequestProcessor(QString userRequest) : request(userRequest)
{

}

/*
 * Application Kick-Off
 */
void GtfsRequestProcessor::run()
{
    QJsonObject respJson;
    QString     SystemResponse;

//    qDebug() << "GtfsRequestProcessor: Answering on thread " << this;

    // The first three chars are assumed to be that of the application request
    QString userApp = request.left(3);  // 3-digit module code
    QString userReq = request.mid(4);   // Remainder of request (syntax varies)

    // Decode the application request

    // Ensure that any exceptions do not take down the processor
    try {
        if (! userApp.compare("SDS", Qt::CaseInsensitive)) {
            GTFS::StaticStatus SDS;
            SDS.fillResponseData(respJson);
        } else if (! userApp.compare("RTE", Qt::CaseInsensitive)) {
            GTFS::AvailableRoutes RTE;
            RTE.fillResponseData(respJson);
        } else if (! userApp.compare("TRI", Qt::CaseInsensitive)) {
            // RECONCILE flag is ignored for regular trip schedule display
            GTFS::TripScheduleDisplay TRI(userReq, false, QDate(), GTFS::TRIPID_RECONCILE);
            TRI.fillResponseData(respJson);
        } else if (! userApp.compare("TSR", Qt::CaseInsensitive)) {
            QDate noDate;
            GTFS::TripsServingRoute TSR(userReq, noDate);
            TSR.fillResponseData(respJson);
        } else if (! userApp.compare("TRD", Qt::CaseInsensitive)) {
            QString remainingReq;
            QDate reqDate = determineServiceDay(userReq, remainingReq);
            GTFS::TripsServingRoute TSR(remainingReq, reqDate);
            TSR.fillResponseData(respJson);
        } else if (! userApp.compare("TSS", Qt::CaseInsensitive)) {
            QDate noDate;
            GTFS::TripsServingStop TSS(userReq, noDate);
            TSS.fillResponseData(respJson);
        } else if (! userApp.compare("TSD", Qt::CaseInsensitive)) {
            QString remainingReq;
            QDate reqDate = determineServiceDay(userReq, remainingReq);
            GTFS::TripsServingStop TSD(remainingReq, reqDate);
            TSD.fillResponseData(respJson);
        } else if (! userApp.compare("STA", Qt::CaseInsensitive)) {
            GTFS::StationDetailsDisplay STA(userReq);
            STA.fillResponseData(respJson);
        } else if (! userApp.compare("SSR", Qt::CaseInsensitive)) {
            GTFS::StopsServedByRoute SSR(userReq);
            SSR.fillResponseData(respJson);
        } else if (! userApp.compare("NEX", Qt::CaseInsensitive)) {
            QString remainingReq;
            qint32 futureMinutes = determineMinuteRange(userReq, remainingReq);
            QList<QString> decodedStopIDs;
            listifyStopIDs(remainingReq, decodedStopIDs);
            // Requesting future trips for a time range, no limit on occurrences
            GTFS::UpcomingStopService NEX(decodedStopIDs, futureMinutes, false, false);
            NEX.fillResponseData(respJson);
        } else if (! userApp.compare("NCF", Qt::CaseInsensitive)) {
            QString remainingReq;
            qint32 futureMinutes = determineMinuteRange(userReq, remainingReq);
            QList<QString> decodedStopIDs;
            listifyStopIDs(remainingReq, decodedStopIDs);
            // Requesting future trips for a time range, no limit on occurrences
            GTFS::UpcomingStopService NCF(decodedStopIDs, futureMinutes, true, false);
            NCF.fillResponseData(respJson);
        } else if (! userApp.compare("NXR", Qt::CaseInsensitive)) {
            // Just like "NEX" but ONLY with realtime recommendations - useful for the daring? Formatted exactly like
            // NEX so the response is encoded with NEX for ease of parsing on the client side, so it would be up to the
            // client to warn about this particular usage/condition.
            QList<QString> decodedStopIDs;
            listifyStopIDs(userReq, decodedStopIDs);
            GTFS::UpcomingStopService NXR(decodedStopIDs, 4320, false, true);
            NXR.fillResponseData(respJson);
        } else if (! userApp.compare("SNT", Qt::CaseInsensitive)) {
            GTFS::StopsWithoutTrips SNT;
            SNT.fillResponseData(respJson);
        } else if (! userApp.compare("RTS", Qt::CaseInsensitive)) {
            GTFS::TripScheduleDisplay TRI(userReq, true, QDate(), GTFS::TRIPID_RECONCILE);
            TRI.fillResponseData(respJson);
        } else if (! userApp.compare("RTF", Qt::CaseInsensitive)) {
            GTFS::TripScheduleDisplay TRI(userReq, true, QDate(), GTFS::TRIPID_FEED_ONLY);
            TRI.fillResponseData(respJson);
        } else if (! userApp.compare("RTT", Qt::CaseInsensitive)) {
            GTFS::TripScheduleDisplay TRI(userReq, true, QDate(), GTFS::RTTUIDX_FEED_ONLY);
            TRI.fillResponseData(respJson);
        } else if (! userApp.compare("RDS", Qt::CaseInsensitive)) {
            GTFS::RealtimeStatus RDS;
            RDS.fillResponseData(respJson);
        } else if (! userApp.compare("RTI", Qt::CaseInsensitive)) {
            GTFS::RealtimeTripInformation RTI;
            RTI.fillResponseData(respJson);
        } else if (! userApp.compare("SBS", Qt::CaseInsensitive)) {
            QString remainingReq;
            QDate reqDate = determineServiceDay(userReq, remainingReq);
            QList<QString> decodedStopIDs;
            listifyStopIDs(remainingReq, decodedStopIDs);
            if (decodedStopIDs.length() != 2) {
                // TODO: This fixes the segmentation fault, but it is a little bad to do this logic here
                respJson["error"]        = 704;
                respJson["message_type"] = "SBS";
            } else {
                // *****************************
                //  TODO WARNING THIS IS SHITTY ... fix the direct references :-(
                // *****************************
                GTFS::ServiceBetweenStops SBS(decodedStopIDs.at(0), decodedStopIDs.at(1), reqDate);
                SBS.fillResponseData(respJson);
            }
        } else if (! userApp.compare("DRT", Qt::CaseInsensitive)) {
            GTFS::RealtimeTripInformation DRT;
            DRT.dumpRealTime(SystemResponse);
        } else if (! userApp.compare("RPS", Qt::CaseInsensitive)) {
            GTFS::RealtimeProductStatus RPS;
            RPS.fillResponseData(respJson);
        } else {
            // Return ERROR 1: Unknown request (userApp)
            respJson["error"] = 1;
            respJson["user_string"] = this->request;
        }
    } catch (...) {
        // Just catch anything and return an error to the client ... hopefully THIS does not cause an exception too!
        respJson["error"] = 2;
        respJson["user_string"] = this->request;
    }

    // Serialize for Transport (Note: we append "\n" so clients can detect the end of the stream)
    if (!respJson.isEmpty()) {
        QJsonDocument jdoc(respJson);
        SystemResponse = jdoc.toJson(QJsonDocument::Compact) + "\n";
    } else {
        SystemResponse += "\n";
    }
    emit Result(SystemResponse);
}

QDate GtfsRequestProcessor::determineServiceDay(const QString &userReq, QString &remUserQuery)
{
    // First space indicates the separation between the date request and the remainder of the user query
    QString   dayDate = userReq.left(userReq.indexOf(" "));

    const GTFS::Status *data  = GTFS::DataGateway::inst().getStatus();
    const QTimeZone &agencyTZ = data->getAgencyTZ();
    QDateTime userDate = QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch());
    QDate     userDateLocale;
    QDateTime overrideDate = data->getOverrideDateTime();

    if (!overrideDate.isNull()) {
        userDate = overrideDate;
    }

    if (!dayDate.compare("D", Qt::CaseInsensitive)) {
        userDateLocale = userDate.toTimeZone(agencyTZ).date();
    } else if (!dayDate.compare("Y", Qt::CaseInsensitive)) {
        userDateLocale = userDate.toTimeZone(agencyTZ).addDays(-1).date();
    } else if (!dayDate.compare("T", Qt::CaseInsensitive)) {
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

void GtfsRequestProcessor::listifyStopIDs(const QString &remUserQuery, QList<QString> &listStopIDs)
{
    if (remUserQuery.indexOf("|") == -1) {
        // Simple case where no "|" was found, so a single stop ID was probably requested
        listStopIDs.append(remUserQuery);
    } else {
        // Otherwise split the whole request into discrete stop IDs
        listStopIDs = remUserQuery.split('|');
    }
}
