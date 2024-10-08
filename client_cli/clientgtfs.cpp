/*
 * GtfsProc_Server
 * Copyright (C) 2018-2023, Daniel Brook
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

#include "clientgtfs.h"

#include <QTextStream>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

ClientGtfs::ClientGtfs(QObject *parent) : QObject(parent), disp()
{
}

bool ClientGtfs::startConnection(QString hostname, quint16 port, int userTimeout)
{
    // Start talking to the server to hold open a connection
    // Should be automatically cleaned by destructor
    this->commSocket.connectToHost(hostname, port);

    // Have a problem?
    if (!this->commSocket.waitForConnected(userTimeout)) {
        this->disp.showError(this->commSocket.errorString());
        return false;
    }

    return true;
}

void ClientGtfs::once(bool prettyPrint)
{
    QTextStream screen(stdout);
    QTextStream stdinquery(stdin);
    stdinquery.skipWhiteSpace();
    QString query = stdinquery.readLine(1024);
    QString response;

    // Assume here that the script / client knows what they're doing, so send the request to the server
    QByteArray serverRequest = query.toUtf8();
    this->commSocket.write(serverRequest);

    // Socket communication is a pain, so there is "an understanding" with the server that a newline character
    // indicates the end of a transmission, read text off the socket until a newline is found.
    while (1) {
        // Wait for response (15 seconds)
        if (! this->commSocket.waitForReadyRead(15000)) {
            this->disp.showError(this->commSocket.errorString());
            break;
        }

        QString tempRespStr = QString(this->commSocket.readAll());
        response += tempRespStr;
        //qDebug() << "==> " << responseStr;
        if (response.indexOf('\n') != -1) {
            break;
        }
    }

    // Dump the JSON response to standard output
    if (prettyPrint) {
        QJsonDocument respDoc = QJsonDocument::fromJson(response.toUtf8());
        QString formattedDoc  = respDoc.toJson(QJsonDocument::Indented);
        screen << formattedDoc;
    } else {
        screen << response;
    }
}

void ClientGtfs::repl()
{
    QTextStream screen(stdout);
    disp.displayWelcome();

    QString previousEntry = "HOM";                         // Track the last query so resubmitting is easier

    while (1) {
        screen << "% ";
        screen.flush();                                    // Forces the text above to print / make a "prompt"

        QTextStream query(stdin);
        query.skipWhiteSpace();                            // Important so that the user can 'flush' with \n !
        QString qquery = query.readLine(1024);             // This should be more than enough characters

        //
        // Control Options: BYE to quit, HOM to display home screen
        //
        if (! qquery.compare("BYE", Qt::CaseInsensitive)) {
            break;
        } else if (! qquery.compare("HOM", Qt::CaseInsensitive)) {
            this->disp.reInitTerm();
            this->disp.clearTerm();
            this->disp.displayWelcome();
            previousEntry = qquery;
            continue;
        } else if (! qquery.compare("R", Qt::CaseInsensitive)) {
            qquery = previousEntry;
        } else {
            // Store the query so it can be used again with "R"
            previousEntry = qquery;
        }

        //
        // Send query to remote system
        //
        QByteArray serverRequest = qquery.toUtf8();
        this->commSocket.write(serverRequest);

        //
        // Process remote system's response
        //
        QString responseStr;
        while (1) {
            // Wait for response (15 seconds)
            if (! this->commSocket.waitForReadyRead(15000)) {
                this->disp.showError(this->commSocket.errorString());
                break;
            }

            QString tempRespStr = QString(this->commSocket.readAll());
            responseStr += tempRespStr;
            //qDebug() << "==> " << responseStr;
            if (responseStr.indexOf('\n') != -1) {
                // We're done reading the stream if we found a newline
                break;
            }
        }

        // Deserialize from JSON so we can process the response type
        QJsonObject respObj;
        QJsonDocument respDoc = QJsonDocument::fromJson(responseStr.toUtf8());
        if (!respDoc.isNull()) {
            if (respDoc.isObject()) {
                respObj = respDoc.object();
            } else {
                this->disp.showError("Server response was badly encoded (Not an Object)");
            }
        } else {
            this->disp.showError("Server response was badly encoded (Invalid JSON)");
        }

        //
        // GtfsProc valid response decoding
        // TODO: It would be nice to break this into separate classes but the terminal view is just for debugging
        //

        /*
         * SDS Response
         */
        if (respObj["message_type"] == "SDS") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            qint32 c1 = 10;               // Always 10 positions for the ID column
            qint32 c4 = 20;               // Always 20 positions for the PHONE number column
            totalCol -= 33;               // Take the fixed width columns out of the calculation (+ all 3 spacers)
            qint32 c2 = totalCol * 0.50;  // 50% of non-fixed columns goes to agency "NAME"
            qint32 c3 = totalCol - c2;    // Whatever's left to the TimeZone column (truncation due to integer division)

            this->disp.clearTerm();

            screen << "GTFS Server Status"
                   << qSetFieldWidth(this->disp.getCols() - 18)      // 18 == "GTFS Server Status".length()
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            // Easier to understand uptime
            qint64 uptimeMs = respObj["appuptime_ms"].toInt();
            qint32 days     = uptimeMs / 86400000;
            qint8  hours    = (uptimeMs - days * 86400000) / 3600000;
            qint8  mins     = (uptimeMs - days * 86400000 - hours * 3600000) / 60000;
            qint8  secs     = (uptimeMs - days * 86400000 - hours * 3600000 - mins * 60000) / 1000;

            screen << "[ Backend ]" << Qt::endl;
            screen << "Processed Reqs . . " << respObj["processed_reqs"].toInt() << Qt::endl;
            screen << "Uptime . . . . . . " << days  << "d " << qSetFieldWidth(2)
                                            << hours << "h "
                                            << mins  << "m "
                                            << secs  << "s " << qSetFieldWidth(0) << Qt::endl;
            screen << "Data Load Time . . " << respObj["dataloadtime_ms"].toInt() << "ms" << Qt::endl;
            screen << "Thread Pool  . . . " << respObj["threadpool_count"].toInt() << Qt::endl;

            screen << "Override Opts  . . " << respObj["overrides"].toString() << Qt::endl;
            screen << "Term Trips . . . . " << (respObj["hide_terminating"].toBool() ? "Hidden" : "Shown") << Qt::endl;
            screen << "NEX Trips/Rte  . . " << respObj["nb_nex_trips"].toInt() << Qt::endl;
            screen << "RT Date Match  . . " << respObj["rt_date_match"].toInt() << Qt::endl;
            screen << "RT Trip Match  . . " << (respObj["rt_trip_seq_match"].toBool() ? "Sequence Numbers" : "Stop ID Only") << Qt::endl;

            screen << "System Version . . " << respObj["application"].toString() << Qt::endl << Qt::endl;

            screen << "[ Static Feed Information ]" << Qt::endl;
            screen << "Publisher  . . . . " << respObj["feed_publisher"].toString() << Qt::endl;
            screen << "URL  . . . . . . . " << respObj["feed_url"].toString() << Qt::endl;
            screen << "Language . . . . . " << respObj["feed_lang"].toString() << Qt::endl;
            screen << "Valid Time . . . . " << "Start: " << respObj["feed_valid_start"].toString() << ", End: "
                                            << respObj["feed_valid_end"].toString() << Qt::endl;
            screen << "Version Text . . . " << respObj["feed_version"].toString() << Qt::endl;
            screen << "Recs Loaded  . . . " << respObj["records"].toInt() << Qt::endl;
            screen << Qt::endl;

            screen << "[ Agency Load ]" << Qt::endl;

            screen.setFieldAlignment(QTextStream::AlignLeft);
            screen << qSetFieldWidth(c1) << "ID"        << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "NAME"      << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c3) << "TIMEZONE"  << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c4) << "PHONE"     << qSetFieldWidth(0) << Qt::endl;

            QJsonArray agencies = respObj["agencies"].toArray();
            for (const QJsonValue &ag : agencies) {
                screen << qSetFieldWidth(c1) << ag["id"].toString().left(c1)   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << ag["name"].toString().left(c2) << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << ag["tz"].toString().left(c3)   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4) << ag["phone"].toString().left(c4)
                       << qSetFieldWidth( 0) << Qt::endl;
            }
            screen.setFieldAlignment(QTextStream::AlignRight);
            screen << Qt::endl;
        }

        /*
         * RTE Response
         */
        else if (respObj["message_type"] == "RTE") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 3;                          // Take the 3 spacer characters out of the calculation
            qint32 c1 = totalCol * 0.23;            // 23% for the Route ID
            qint32 c2 = totalCol * 0.26;            // 26% for the short name
            qint32 c4 = 5;                          // 5 spaces for # of trips associated to route(s)
            qint32 c3 = totalCol - (c2 + c1 + c4);  // Whatever's left for the long name

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Route List"
                   << qSetFieldWidth(this->disp.getCols() - 10)      // 10 == "Route List".length()
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            // Useful data?
            screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << Qt::endl << Qt::endl;

            // Start spitting out the routes
            screen.setFieldAlignment(QTextStream::AlignLeft);
            screen << qSetFieldWidth(c1) << "ID"         << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "SHORT NAME" << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c3) << "LONG NAME"  << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c4) << "TRIPS"      << qSetFieldWidth(0) << Qt::endl;

            QJsonArray routes = respObj["routes"].toArray();
            for (const QJsonValue &ro : routes) {
                screen << qSetFieldWidth(c1) << ro["id"].toString().left(c1)         << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << ro["short_name"].toString().left(c2) << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << ro["long_name"].toString().left(c3)  << qSetFieldWidth(1) << " ";
                screen.setFieldAlignment(QTextStream::AlignRight);
                screen << qSetFieldWidth(c4) << ro["nb_trips"].toInt()               << qSetFieldWidth(0) << Qt::endl;
                screen.setFieldAlignment(QTextStream::AlignLeft);
            }

            screen << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * TRI Response
         */
        else if (respObj["message_type"] == "TRI") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 26;                          // Remove 3 for SEQ + 14 for times + 2 for P/D + 5 for spacers + 1 for SKIP
            qint32 c1 = 3;
            qint32 c2 = totalCol * 0.30;             // 30% of remaining space for STOP-ID
            qint32 c3 = totalCol - c2;               // All remaining space for STOP-NAME
            qint32 c4 = 1;
            qint32 c5 = 7;
            qint32 c6 = 7;

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Trip Schedule"
                   << qSetFieldWidth(this->disp.getCols() - 13)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            if (respObj["error"].toInt() == 101) {
                screen << "Trip not found in static database." << Qt::endl;
            } else if (respObj["error"].toInt() == 102) {
                screen << "Trip not found in real-time data feed." << Qt::endl;
            } else {
                bool realTimeData = respObj["real_time"].toBool();

                screen << "Trip ID  . . . . . " << respObj["trip_id"].toString() << Qt::endl;
                if (!realTimeData) {
                    screen << "Service ID . . . . " << respObj["service_id"].toString() << Qt::endl;
                    screen << "Validity . . . . . " << respObj["svc_start_date"].toString() << " - "
                                                    << respObj["svc_end_date"].toString() << Qt::endl;
                    screen << "Operating Days . . " << respObj["operate_days"].toString() << Qt::endl;
                    screen << "Exceptions . . . . " << respObj["exception_dates"].toString() << Qt::endl;
                    screen << "Additions  . . . . " << respObj["added_dates"].toString() << Qt::endl;
                }
                screen << "Route ID . . . . . " << respObj["route_id"].toString() << Qt::endl;
                screen << "Route Name . . . . " << respObj["route_short_name"].toString() << ", \""
                                                << respObj["route_long_name"].toString()  << "\"" << Qt::endl;
                if (!realTimeData) {
                    screen << "Headsign . . . . . " << respObj["headsign"].toString() << Qt::endl;
                } else {
                    screen << "Vehicle  . . . . . " << respObj["vehicle"].toString() << Qt::endl;
                    screen << "Start Date&Time  . " << respObj["start_date"].toString() << " "
                                                    << respObj["start_time"].toString() << Qt::endl;
                    screen << "Real Time Data . . " << respObj["real_time_data_time"].toString() << Qt::endl;
                }

                screen << "Short Name . . . . " << respObj["short_name"].toString() << Qt::endl << Qt::endl;

                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)   << "SEQ"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)   << "STOP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3)   << "STOP-NAME" << qSetFieldWidth(1) << " ";

                if (!realTimeData) {
                    screen << qSetFieldWidth(c4*2) << "PD"        << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5)   << "SCH-A"     << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c6)   << "SCH-D"     << qSetFieldWidth(0) << Qt::endl;
                } else {
                    screen << qSetFieldWidth(c4*2) << "  "        << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5)   << "PRE-A"     << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c6)   << "PRE-D"     << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c4)   << "S"         << qSetFieldWidth(0) << Qt::endl;
                }

                QJsonArray stops = respObj["stops"].toArray();
                for (const QJsonValue &st : stops) {
                    QString dropOff = dropoffToChar(st["drop_off_type"].toInt());
                    QString pickUp  = pickupToChar(st["pickup_type"].toInt());

                    screen << qSetFieldWidth(c1) << st["sequence"].toInt()              << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << st["stop_id"].toString().left(c2)   << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << st["stop_name"].toString().left(c3) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c4) << pickUp << dropOff                   << qSetFieldWidth(1) << " ";
                    screen.setFieldAlignment(QTextStream::AlignRight);
                    screen << qSetFieldWidth(c5) << st["arr_time"].toString()           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c6) << st["dep_time"].toString()           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c4) << (st["skipped"].toBool() ? "X" : "") << qSetFieldWidth(0) << Qt::endl;
                    screen.setFieldAlignment(QTextStream::AlignLeft);
                }
            }
            screen << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * TSR Response
         */
        else if (respObj["message_type"] == "TSR") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 47;               // Remove 47 columns for ValidTime, Op-Days, ES, SCH-D + 2 space
            qint32 c1 = totalCol * 0.5;   // 60% of open space for trip ID
            qint32 c2 = totalCol - c1;    // Remaining scalable range (~40%) for Headsign
            qint32 c3 = 9;                //  9 spaces (x2) for valid date range
            qint32 c4 = 14;               // 14 spaces for operating date range
            qint32 c5 = 1;                //  1 space  (x2) for exemption/supplement
            qint32 c6 = 7;                //  7 spaces for first-scheduled-departure

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Trips Serving Route"
                   << qSetFieldWidth(this->disp.getCols() - 19)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            if (respObj["error"].toInt() == 201) {
                screen << "Route not found." << Qt::endl;
            }
            else {
                screen << "Route ID . . . . . " << respObj["route_id"].toString() << Qt::endl;
                screen << "Route Name . . . . " << respObj["route_short_name"].toString() << ", \""
                                                << respObj["route_long_name"].toString()  << "\"" << Qt::endl;
                screen << "Service Date . . . " << respObj["service_date"].toString() << Qt::endl << Qt::endl;

                // Loop on all the trips
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)     << "TRIP-ID"        << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)     << "HEADSIGN"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3*2+1) << "VALID-DURATION" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4)     << "OPERATING-DAYS" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5*2)   << "ES"             << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c6)     << "SCH-D"          << qSetFieldWidth(0) << Qt::endl;

                QJsonArray trips = respObj["trips"].toArray();
                for (const QJsonValue &tr : trips) {
                    QString svcExempt = (tr["exceptions_present"].toBool())     ? "E": " ";
                    QString svcSplmnt = (tr["supplements_other_days"].toBool()) ? "S": " ";
                    QString dstOn     = " ";
                    screen << qSetFieldWidth(c1) << tr["trip_id"].toString().left(c1)  << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << tr["headsign"].toString().left(c2) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << tr["svc_start_date"].toString()    << qSetFieldWidth(1) << "-"
                           << qSetFieldWidth(c3) << tr["svc_end_date"].toString()      << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c4) << tr["operate_days_condensed"].toString().left(c4)
                           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5) << svcExempt << svcSplmnt
                           << qSetFieldWidth(1) << dstOn;
                    screen.setFieldAlignment(QTextStream::AlignRight);
                    screen << qSetFieldWidth(c6) << tr["first_stop_departure"].toString()
                           << qSetFieldWidth(0) << Qt::endl;
                    screen.setFieldAlignment(QTextStream::AlignLeft);
                }

                screen << Qt::endl << "Query took " << respObj["proc_time_ms"].toInt() << " ms, "
                       << trips.size() << " records loaded" << Qt::endl;
            }

            screen << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * TSS Response
         */
        else if (respObj["message_type"] == "TSS") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 52;               // Remove 52 columns for ValidTime, Op-Days, ES, TPD, SCH-D + 3 space
            qint32 c1 = totalCol * 0.4;   // 40% of open space for trip ID
            qint32 c2 = (totalCol - c1) * 0.3;   // Remaining scalable range split between short name and headsign
            qint32 c3 = totalCol - (c1 + c2);
            qint32 c4 = 9;                //  9 spaces (x2) for valid date range
            qint32 c5 = 14;               // 14 spaces for operating date range
            qint32 c6 = 1;                //  1 space  (x2) for exemption/supplement
            qint32 c7 = 7;                //  7 spaces for first-scheduled-departure

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Trips Serving Stop"
                   << qSetFieldWidth(this->disp.getCols() - 18)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            if (respObj["error"].toInt() == 301) {
                screen << "Stop not found." << Qt::endl;
            }
            else {
                // Count how many records were returned
                qint32 tripsLoaded = 0;

                screen << "Stop ID  . . . . . " << respObj["stop_id"].toString() << Qt::endl
                       << "Stop Name  . . . . " << respObj["stop_name"].toString() << Qt::endl
                       << "Stop Desc  . . . . " << respObj["stop_desc"].toString() << Qt::endl
                       << "Parent Station . . " << respObj["parent_sta"].toString() << Qt::endl
                       << "Service Date . . . " << respObj["service_date"].toString() << Qt::endl << Qt::endl;

                // ... then all the trips within each route
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)     << "TRIP-ID"        << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)     << "NAME"           << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3)     << "HEADSIGN"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4*2+1) << "VALID-DURATION" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5)     << "OPERATING-DAYS" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c6*5)   << "ES TPD"         << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c7)     << "SCH-D"          << qSetFieldWidth(0) << Qt::endl << Qt::endl;

                // Loop on all the routes
                QJsonArray routes = respObj["routes"].toArray();
                for (const QJsonValue &ro : routes) {
                    screen << "[ Route ID " << ro["route_id"].toString() << " :: "
                                            << ro["route_short_name"].toString() << " :: "
                                            << ro["route_long_name"].toString()  << " ]"   << Qt::endl;

                    // Loop on all the trips
                    QJsonArray trips = ro["trips"].toArray();
                    for (const QJsonValue &tr : trips) {
                        QString headsign   = tr["headsign"].toString().left(c3);
                        QString svcExempt  = (tr["exceptions_present"].toBool())     ? "E": " ";
                        QString svcSplmnt  = (tr["supplements_other_days"].toBool()) ? "S": " ";
                        QString tripTerm;
                        if (tr["trip_begins"].toBool()) {
                            tripTerm = "S";
                        } else if (tr["trip_terminates"].toBool()) {
                            tripTerm = "T";
                        } else {
                            tripTerm = " ";
                        }
                        QString dropOff    = dropoffToChar(tr["drop_off_type"].toInt());
                        QString pickUp     = pickupToChar(tr["pickup_type"].toInt());
                        QString dstIndic   = " ";
//                        if (tr["dst_on"].isBool()) {
//                            if (tr["dst_on"].toBool()) {
//                                dstIndic = "D";
//                            } else {
//                                dstIndic = "S";
//                            }
//                        }

                        screen << qSetFieldWidth(c1) << tr["trip_id"].toString().left(c1)   << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c2) << tr["short_name"].toString().left(c2)<< qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c3) << headsign                            << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c4) << tr["svc_start_date"].toString()     << qSetFieldWidth(1) << "-"
                               << qSetFieldWidth(c4) << tr["svc_end_date"].toString()       << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c5) << tr["operate_days_condensed"].toString().left(c5)
                               << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c6) << svcExempt << svcSplmnt << " " << tripTerm << pickUp << dropOff
                               << qSetFieldWidth(1) << dstIndic;
                        screen.setFieldAlignment(QTextStream::AlignRight);
                        screen << qSetFieldWidth(c7) << tr["dep_time"].toString() << qSetFieldWidth(0) << Qt::endl;
                        screen.setFieldAlignment(QTextStream::AlignLeft);
                    } // End loop on Trips-within-Route
                    tripsLoaded += trips.size();
                    screen << Qt::endl;
                } // End loop on Routes-serving-Stop

                screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms, "
                       << tripsLoaded << " trips loaded" << Qt::endl;
            }

            screen << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * STA Response
         */
        else if (respObj["message_type"] == "STA") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 2;                    // Remove 2 spaces for 3 column's worth of spaces
            qint32 c1 = totalCol * 0.25;      // 25% of open space for first column
            qint32 c2 = c1;                   // 25% of open space for second column
            qint32 c3 = totalCol - (2 * c1);  // Everything else goes to the third column

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Individual Stop Service Information"
                   << qSetFieldWidth(this->disp.getCols() - 35)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            if (respObj["error"].toInt() == 401) {
                screen << "Stop not found." << Qt::endl;
            }
            else {
                screen << "Stop ID/Name . . . " << respObj["stop_id"].toString() << " :: "
                                                << respObj["stop_name"].toString() << Qt::endl;
                screen << "Stop Desc  . . . . " << respObj["stop_desc"].toString() << Qt::endl;
                screen << "Location . . . . . " << respObj["loc_lat"].toString() << ", "
                                                << respObj["loc_lon"].toString() << Qt::endl;
                screen << "Parent Station . . " << respObj["parent_sta"].toString() << Qt::endl << Qt::endl;

                // Show all the Routes associated with the station requested
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << "[ Routes Serving Stop ]" << Qt::endl;
                screen << qSetFieldWidth(c1) << "ROUTE-ID"         << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << "ROUTE-NAME-SHORT" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << "ROUTE-NAME-LONG"  << qSetFieldWidth(1) << Qt::endl;

                // Loop on all the routes
                QJsonArray routes = respObj["routes"].toArray();
                for (const QJsonValue &ro : routes) {
                    screen << qSetFieldWidth(c1) << ro["route_id"].toString().left(c1)
                           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << ro["route_short_name"].toString().left(c2)
                           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << ro["route_long_name"].toString().left(c3)
                           << qSetFieldWidth(0) << Qt::endl;
                }
                screen << Qt::endl;

                // Then display any associated stations with the parent
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << "[ Stops Sharing Parent Station ]" << Qt::endl;
                screen << qSetFieldWidth(c1) << "STOP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << "STOP-NAME" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << "STOP-DESC" << qSetFieldWidth(1) << Qt::endl;

                QJsonArray sharedStops = respObj["stops_sharing_parent"].toArray();
                for (const QJsonValue &ss : sharedStops) {
                    screen << qSetFieldWidth(c1) << ss["stop_id"].toString().left(c1)   << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << ss["stop_name"].toString().left(c2) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << ss["stop_desc"].toString().left(c3)
                           << qSetFieldWidth(0)  << Qt::endl;
                }
                screen << Qt::endl;
                screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << Qt::endl;
            }

            screen << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * SSR Response
         */
        else if (respObj["message_type"] == "SSR") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= (4 + 17 + 6);         // Remove 4 spaces for 5 column's worth of spacers
            qint32 c5 = 8;                    // Fixed width for lat/lon
            qint32 c4 = 6;                    // Fixed width for # of trips serving the stop for the route
            qint32 c1 = totalCol * 0.25;      // 25% of open space for first column
            qint32 c3 = totalCol * 0.40;      // 40% of open space for the third column
            qint32 c2 = totalCol - (c1 + c3); // Everything else to the stop name

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Route Summary and Stop Information"
                   << qSetFieldWidth(this->disp.getCols() - 34)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            if (respObj["error"].toInt() == 501) {
                screen << "Route not found." << Qt::endl;
            }
            else {
                screen << "Route ID . . . . . " << respObj["route_id"].toString() << Qt::endl
                       << "Short Name . . . . " << respObj["route_short_name"].toString() << Qt::endl
                       << "Long Name  . . . . " << respObj["route_long_name"].toString() << Qt::endl
                       << "Description  . . . " << respObj["route_desc"].toString() << Qt::endl
                       << "Type . . . . . . . " << respObj["route_type"].toString() << Qt::endl
                       << "URL  . . . . . . . " << respObj["route_url"].toString() << Qt::endl
                       << "Color  . . . . . . " << respObj["route_color"].toString() << Qt::endl
                       << "Text Color . . . . " << respObj["route_text_color"].toString() << Qt::endl << Qt::endl;

                // Show all the Routes associated with the station requested
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << "[ Stops Served by Route ]" << Qt::endl;
                screen << qSetFieldWidth(c1)     << "STOP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)     << "STOP-NAME" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3)     << "STOP-DESC" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4)     << "#TRIPS"    << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5 * 2) << "LOCATION"  << qSetFieldWidth(1) << Qt::endl;

                // Loop on all the routes
                QJsonArray stops = respObj["stops"].toArray();
                for (const QJsonValue &so : stops) {
                    screen << qSetFieldWidth(c1) << so["stop_id"].toString().left(c1)   << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << so["stop_name"].toString().left(c2) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << so["stop_desc"].toString().left(c3) << qSetFieldWidth(1) << " ";
                    screen.setFieldAlignment(QTextStream::AlignRight);
                    screen << qSetFieldWidth(c4) << so["trip_count"].toInt()            << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5) << so["stop_lat"].toString().left(c5)  << qSetFieldWidth(1) << ","
                           << qSetFieldWidth(c5) << so["stop_lon"].toString().left(c5)
                           << qSetFieldWidth(0)  << Qt::endl;
                    screen.setFieldAlignment(QTextStream::AlignLeft);
                }
                screen << Qt::endl;
            }
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * NEX Response
         */
        else if (respObj["message_type"] == "NEX") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 28;                    // Remove space for fixed-width fields and spacers
            qint32 c1 = totalCol * 0.4;        // 40% of open space for trip ID
            qint32 c2 = totalCol * 0.2;        // 20% of open space for the short name of the trip
            qint32 c3 = totalCol - (c1 + c2);  // Remaining scalable range (~50%) Headsign
            qint32 c4 = 1;                     // 1 space  (x3) for terminate + pickup/drop-off exception flags
            qint32 c5 = 11;                    // 9 spaces for date-of-service field
            qint32 c6 = 4;                     // 9 spaces for date-of-service field
            qint32 c7 = 4;                     // 1 space  (x2) for exemption/supplement

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Upcoming Service at Stop"
                   << qSetFieldWidth(this->disp.getCols() - 24)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            if (respObj["error"].toInt() == 601) {
                screen << "Stop not found." << Qt::endl;
            }
            else {
                // Count how many records were returned
                qint32 tripsLoaded = 0;

                screen << "Stop ID  . . . . . " << respObj["stop_id"].toString()   << Qt::endl
                       << "Stop Name  . . . . " << respObj["stop_name"].toString() << Qt::endl
                       << "Stop Desc  . . . . " << respObj["stop_desc"].toString() << Qt::endl << Qt::endl;

                // ... then all the trips within each route
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)   << "TRIP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)   << "NAME"      << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3)   << "HEADSIGN"  << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4*3) << "TPD"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5)   << "STOP-TIME" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c6)   << "MINS"      << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c7)   << "STAT"      << qSetFieldWidth(0) << Qt::endl;

                // Loop on all the routes
                QJsonArray routes = respObj["routes"].toArray();
                for (const QJsonValue &ro : routes) {
                    screen << "[ Route ID " << ro["route_id"].toString() << " ]"   << Qt::endl;

                    // Loop on all the trips
                    QJsonArray trips = ro["trips"].toArray();
                    for (const QJsonValue &tr : trips) {
                        QString headsign   = tr["headsign"].toString().left(c3);
                        QString tripTerm;
                        if (tr["trip_begins"].toBool()) {
                            tripTerm = "S";
                        } else if (tr["trip_terminates"].toBool()) {
                            tripTerm = "T";
                        } else {
                            tripTerm = " ";
                        }

                        QString dropOff    = dropoffToChar(tr["drop_off_type"].toInt());
                        QString pickUp     = pickupToChar(tr["pickup_type"].toInt());
//                        QString dstIndic   = (tr["dst_on"].toBool()) ? "D" : "S";
                        QString dstIndic   = " ";
                        qint32 waitTimeMin = tr["wait_time_sec"].toInt() / 60;
                        screen << qSetFieldWidth(c1) << tr["trip_id"].toString().left(c1) << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c2) << tr["short_name"].toString().left(c2) << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c3) << headsign.left(c3)                 << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c4) << tripTerm << pickUp << dropOff     << qSetFieldWidth(1) << " ";
                        screen.setFieldAlignment(QTextStream::AlignRight);

                        // Static schedule arrival/departure times
                        QString depTimeSch = tr["dep_time"].toString();
                        QString arrTimeSch = tr["arr_time"].toString();

                        /*
                         * Real-Time Processing -- Check if the "realtime_data" object is present
                         */
                        if (tr["realtime_data"].isObject()) {
                            QJsonObject rt = tr["realtime_data"].toObject();

                            QString rtStatus   = rt["status"].toString();
                            QString depTimeAct = rt["actual_arrival"].toString();
                            QString arrTimeAct = rt["actual_departure"].toString();

                            // Print the actual time of arrival and countdown
                            if (rtStatus == "SKIP") {
                                screen << qSetFieldWidth(c5) << depTimeSch
                                       << qSetFieldWidth(1) << " "
                                       << qSetFieldWidth(c7) << "-" << qSetFieldWidth(1) << " ";
                            } else if (rtStatus == "CNCL") {
                                // Print regular schedule information with no add-ons
                                screen << qSetFieldWidth(c5)  << depTimeSch
                                       << qSetFieldWidth(1) << " " << qSetFieldWidth(c7) << " "
                                       << qSetFieldWidth(1) << " " << qSetFieldWidth(0);
                            } else {
                                QString timeToShow;
                                // Give priority to the actual arrival, then actual departure ... failing those,
                                // we revert to the static arrival, then static departure
                                if (!arrTimeAct.isEmpty()) {
                                    timeToShow = arrTimeAct;
                                }
                                else if (!depTimeAct.isEmpty()) {
                                    timeToShow = depTimeAct;
                                }
                                else if (!arrTimeSch.isEmpty()) {
                                    timeToShow = arrTimeSch;
                                }
                                else {
                                    timeToShow = depTimeSch;
                                }

                                screen << qSetFieldWidth(c5) << timeToShow
                                       << qSetFieldWidth(1)       << " ";

                                if (rtStatus == "ARRV" || rtStatus == "BRDG" || rtStatus == "DPRT") {
                                    screen << qSetFieldWidth(c7) << rtStatus << qSetFieldWidth(1) << " ";
                                } else {
                                    screen << qSetFieldWidth(c7) << waitTimeMin << qSetFieldWidth(1) << " ";
                                }
                            }

                            if (rtStatus == "RNNG" || rtStatus == "ARRV" || rtStatus == "BRDG" || rtStatus == "DPRT") {
                                // If a supplemental trip or a trip without schedul / departure information, note it
                                QString stopStatus = rt["stop_status"].toString();
                                if (stopStatus == "SPLM" || stopStatus == "SCHD" || stopStatus == "PRED") {
                                    screen << qSetFieldWidth(c7) << stopStatus << qSetFieldWidth(0);
                                } else {
                                    // A running trip is either on-time, late, or early, depending on the offset
                                    qint64 offsetSec = rt["offset_seconds"].toInt();
                                    if (offsetSec < -60 || offsetSec > 60) {
                                        screen.setNumberFlags(QTextStream::ForceSign);
                                        screen << qSetFieldWidth(c7) << offsetSec / 60 << qSetFieldWidth(0);
                                        Qt::noforcesign(screen);
                                    } else {
                                        screen << qSetFieldWidth(c7) << "ONTM" << qSetFieldWidth(0);
                                    }
                                }
                            } else {
                                screen << qSetFieldWidth(c7) << rtStatus << qSetFieldWidth(0);
                            }
                        } else {
                            // Just print regular schedule information with no add-ons, preference is always given to
                            // the arrival time, unless it is not available then use the departure
                            if (arrTimeSch.isEmpty()) {
                            screen << qSetFieldWidth(c5) << depTimeSch  << qSetFieldWidth(1) << " "
                                   << qSetFieldWidth(c7) << waitTimeMin << qSetFieldWidth(0);
                            } else {
                                screen << qSetFieldWidth(c5) << arrTimeSch  << qSetFieldWidth(1) << " "
                                       << qSetFieldWidth(c7) << waitTimeMin << qSetFieldWidth(0);
                            }
                        }

                        screen.setFieldAlignment(QTextStream::AlignLeft);
                        screen << Qt::endl;

                    } // End loop on Trips-within-Route

                    tripsLoaded += trips.size();
                    screen << Qt::endl;
                } // End loop on Routes-serving-Stop

                screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms, "
                       << tripsLoaded << " trips loaded" << Qt::endl;
            }

            screen << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * NCF Response
         */
        else if (respObj["message_type"] == "NCF") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 29;                         // Remove space for fixed-width fields and spacers
            qint32 c1 = totalCol * 0.15;            // 15% of open space for route ID
            qint32 c2 = totalCol * 0.25;            // 25% of open space for trip ID
            qint32 c3 = totalCol * 0.2;             // 20% of open space for the short name of the trip
            qint32 c4 = totalCol - (c1 + c2 + c3);  // Remaining scalable range (~50%) Headsign
            qint32 c5 = 1;                          // 1 space  (x3) for terminate + pickup/drop-off exception flags
            qint32 c6 = 11;                         // 9 spaces for date-of-service field
            qint32 c7 = 4;                          // 9 spaces for date-of-service field
            qint32 c8 = 4;                          // 1 space  (x2) for exemption/supplement

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Upcoming Service at Stop"
                   << qSetFieldWidth(this->disp.getCols() - 24)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            if (respObj["error"].toInt() == 601) {
                screen << "Stop not found." << Qt::endl;
            }
            else {
                // Count how many records were returned
                qint32 tripsLoaded = 0;

                screen << "Stop ID  . . . . . " << respObj["stop_id"].toString()   << Qt::endl
                       << "Stop Name  . . . . " << respObj["stop_name"].toString() << Qt::endl
                       << "Stop Desc  . . . . " << respObj["stop_desc"].toString() << Qt::endl << Qt::endl;

                // ... then all the trips within each route
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)   << "ROUTE-ID"  << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)   << "TRIP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3)   << "NAME"      << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4)   << "HEADSIGN"  << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5*3) << "TPD"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c6)   << "STOP-TIME" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c7)   << "MINS"      << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c8)   << "STAT"      << qSetFieldWidth(0) << Qt::endl;

                // Loop on all the trips
                QJsonArray trips = respObj["trips"].toArray();
                for (const QJsonValue &tr : trips) {
                    QString headsign   = tr["headsign"].toString().left(c4);
                    QString tripTerm;
                    if (tr["trip_begins"].toBool()) {
                        tripTerm = "S";
                    } else if (tr["trip_terminates"].toBool()) {
                        tripTerm = "T";
                    } else {
                        tripTerm = " ";
                    }

                    QString dropOff    = dropoffToChar(tr["drop_off_type"].toInt());
                    QString pickUp     = pickupToChar(tr["pickup_type"].toInt());
//                        QString dstIndic   = (tr["dst_on"].toBool()) ? "D" : "S";
                    QString dstIndic   = " ";
                    qint32 waitTimeMin = tr["wait_time_sec"].toInt() / 60;
                    screen << qSetFieldWidth(c1) << tr["route_id"].toString().left(c1) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << tr["trip_id"].toString().left(c2) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << tr["short_name"].toString().left(c3) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c4) << headsign.left(c4)                 << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5) << tripTerm << pickUp << dropOff     << qSetFieldWidth(1) << " ";
                    screen.setFieldAlignment(QTextStream::AlignRight);

                    // Static schedule arrival/departure times
                    QString depTimeSch = tr["dep_time"].toString();
                    QString arrTimeSch = tr["arr_time"].toString();

                    /*
                     * Real-Time Processing -- Check if the "realtime_data" object is present
                     */
                    if (tr["realtime_data"].isObject()) {
                        QJsonObject rt = tr["realtime_data"].toObject();

                        QString rtStatus   = rt["status"].toString();
                        QString depTimeAct = rt["actual_arrival"].toString();
                        QString arrTimeAct = rt["actual_departure"].toString();

                        // Print the actual time of arrival and countdown
                        if (rtStatus == "SKIP") {
                            screen << qSetFieldWidth(c6) << depTimeSch
                                   << qSetFieldWidth(1) << " "
                                   << qSetFieldWidth(c8) << "-" << qSetFieldWidth(1) << " ";
                        } else if (rtStatus == "CNCL") {
                            // Print regular schedule information with no add-ons
                            screen << qSetFieldWidth(c6)  << depTimeSch
                                   << qSetFieldWidth(1) << " " << qSetFieldWidth(c8) << " "
                                   << qSetFieldWidth(1) << " " << qSetFieldWidth(0);
                        } else {
                            QString timeToShow;
                            // Give priority to the actual arrival, then actual departure ... failing those,
                            // we revert to the static arrival, then static departure
                            if (!arrTimeAct.isEmpty()) {
                                timeToShow = arrTimeAct;
                            }
                            else if (!depTimeAct.isEmpty()) {
                                timeToShow = depTimeAct;
                            }
                            else if (!arrTimeSch.isEmpty()) {
                                timeToShow = arrTimeSch;
                            }
                            else {
                                timeToShow = depTimeSch;
                            }

                            screen << qSetFieldWidth(c6) << timeToShow
                                   << qSetFieldWidth(1)       << " ";

                            if (rtStatus == "ARRV" || rtStatus == "BRDG" || rtStatus == "DPRT") {
                                screen << qSetFieldWidth(c8) << rtStatus << qSetFieldWidth(1) << " ";
                            } else {
                                screen << qSetFieldWidth(c8) << waitTimeMin << qSetFieldWidth(1) << " ";
                            }
                        }

                        if (rtStatus == "RNNG" || rtStatus == "ARRV" || rtStatus == "BRDG" || rtStatus == "DPRT") {
                            // If a supplemental trip or a trip without schedul / departure information, note it
                            QString stopStatus = rt["stop_status"].toString();
                            if (stopStatus == "SPLM" || stopStatus == "SCHD" || stopStatus == "PRED") {
                                screen << qSetFieldWidth(c7) << stopStatus << qSetFieldWidth(0);
                            } else {
                                // A running trip is either on-time, late, or early, depending on the offset
                                qint64 offsetSec = rt["offset_seconds"].toInt();
                                if (offsetSec < -60 || offsetSec > 60) {
                                    screen.setNumberFlags(QTextStream::ForceSign);
                                    screen << qSetFieldWidth(c8) << offsetSec / 60 << qSetFieldWidth(0);
                                    Qt::noforcesign(screen);
                                } else {
                                    screen << qSetFieldWidth(c8) << "ONTM" << qSetFieldWidth(0);
                                }
                            }
                        } else {
                            screen << qSetFieldWidth(c8) << rtStatus << qSetFieldWidth(0);
                        }
                    } else {
                        // Just print regular schedule information with no add-ons, preference is always given to
                        // the arrival time, unless it is not available then use the departure
                        if (arrTimeSch.isEmpty()) {
                        screen << qSetFieldWidth(c6) << depTimeSch  << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c8) << waitTimeMin << qSetFieldWidth(0);
                        } else {
                            screen << qSetFieldWidth(c6) << arrTimeSch  << qSetFieldWidth(1) << " "
                                   << qSetFieldWidth(c8) << waitTimeMin << qSetFieldWidth(0);
                        }
                    }

                    screen.setFieldAlignment(QTextStream::AlignLeft);
                    screen << Qt::endl;

                } // End loop on Trips-within-Route

                tripsLoaded += trips.size();
                screen << Qt::endl;

                screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms, "
                       << tripsLoaded << " trips loaded" << Qt::endl;
            }

            screen << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * SNT Response
         */
        else if (respObj["message_type"] == "SNT") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 4 + 16;              // Spacer + Location columns
            qint32 c1 = totalCol * 0.25;     // Stop ID
            qint32 c3 = totalCol * 0.30;     // Stop Description
            qint32 c4 = 16;                  // Location entries
            qint32 c2 = totalCol - c1 - c3;  // Remaining space to the stop-name

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Stops Without Trips"
                   << qSetFieldWidth(this->disp.getCols() - 19)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            screen.setFieldAlignment(QTextStream::AlignLeft);
            screen << qSetFieldWidth(c1) << "STOP-ID"   << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "STOP-NAME" << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c3) << "STOP-DESC" << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c4) << "LOCATION"  << qSetFieldWidth(1) << Qt::endl;

            // Loop on all the routes
            QJsonArray stops = respObj["stops"].toArray();
            for (const QJsonValue &st : stops) {
                screen << qSetFieldWidth(c1)   << st["stop_id"].toString().left(c1)
                       << qSetFieldWidth(1)    << " "
                       << qSetFieldWidth(c2)   << st["stop_name"].toString().left(c2)
                       << qSetFieldWidth(1)    << " "
                       << qSetFieldWidth(c3)   << st["stop_desc"].toString().left(c3)
                       << qSetFieldWidth(1)    << " ";
                screen.setFieldAlignment(QTextStream::AlignRight);
                screen << qSetFieldWidth(c4/2) << st["loc_lat"].toString().left(c4/2)
                       << qSetFieldWidth(1)    << ","
                       << qSetFieldWidth(c4/2) << st["loc_lon"].toString().left(c4/2)
                       << qSetFieldWidth(0)    << Qt::endl;
                screen.setFieldAlignment(QTextStream::AlignLeft);
            }
            screen << Qt::endl << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << Qt::endl << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        /*
         * RDS Response
         */
        else if (respObj["message_type"] == "RDS") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 1;                // Spacer
            qint32 c1 = totalCol * 0.50;  // Stop ID
            qint32 c2 = totalCol - c1;    // Remaining space to the stop-name

            this->disp.clearTerm();

            // Standard Response Header
            screen << "GTFS Realtime Data Status"
                   << qSetFieldWidth(this->disp.getCols() - 25)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << Qt::endl << Qt::endl;

            screen.setFieldAlignment(QTextStream::AlignLeft);

            // Mutual Exclusion
            screen << "[ Mutual Exclusion ]" << Qt::endl
                   << "Active Side  . . . " << respObj["active_side"].toString() << Qt::endl
                   << "Data Age . . . . . " << respObj["active_age_sec"].toInt() << " s" << Qt::endl
                   << "Feed Time  . . . . " << respObj["active_feed_time"].toString() << Qt::endl
                   << "Download Time  . . " << respObj["active_download_ms"].toInt() << " ms" << Qt::endl
                   << "Integ Time . . . . " << respObj["active_integration_ms"].toInt() << " ms" << Qt::endl
                   << "Next Fetch In  . . " << respObj["seconds_to_next_fetch"].toInt() << " s" << Qt::endl
                   << "Latest RT Txn  . . " << respObj["last_realtime_query"].toString() << Qt::endl
                   << Qt::endl << Qt::endl;


            screen << qSetFieldWidth(c1) << "TRIP-ID"   << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "ROUTE-ID"  << qSetFieldWidth(1) << Qt::endl;

            // Loop on all the routes
            //QJsonArray stops = respObj["stops"].toArray();

            screen << Qt::endl << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << Qt::endl << Qt::endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        // Probably some kind of error condition, just respond in kind
        else {
            screen << Qt::endl << "No handler for the request: " << Qt::endl << responseStr << Qt::endl;
        }
    }

    // On exit (loop halted above)
    screen << "*** DISCONNECTING ***" << Qt::endl;
}

QString ClientGtfs::dropoffToChar(qint8 svcDropOff)
{
    QString dropOff;
    switch (svcDropOff) {
    case 1:
        dropOff = "D";     // Drop-off unavailable for this trip (some agencies mark this for the start of trips)
        break;
    case 2:
        dropOff = "A";     // Drop-off must be pre-arranged by calling the transit agency
        break;
    case 3:
        dropOff = "R";     // Make a special request the operator/conductor to let you off here
        break;
    default:
        dropOff = " ";     // Regular stop (may still have to request to stop on a bus)
        break;
    }
    return dropOff;
}

QString ClientGtfs::pickupToChar(qint8 svcPickUp)
{
    QString pickUp;
    switch (svcPickUp) {
    case 1:
        pickUp  = "P";     // Pick-up service not available (some agencies mark this at the terminus, some don't)
        break;
    case 2:
        pickUp  = "C";     // Pick-up must be scheduled ahead of time by calling transit agency
        break;
    case 3:
        pickUp  = "F";     // Pick-up requested specifically flagging the operator ("flag stop")
        break;
    default:
        pickUp  = " ";     // Regular pick-up service (NOTE: For rail this is a dedicate stop most likely,
        break;             // but in the scope of a bus trip, you probably still want to signal the operator
    }
    return pickUp;
}
