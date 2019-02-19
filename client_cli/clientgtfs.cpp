#include "clientgtfs.h"

#include <QTextStream>
#include <QByteArray>

// To process responses
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

// Temporary Debugging / Time Serialization
#include <QTime>

ClientGtfs::ClientGtfs(QObject *parent) : QObject(parent), disp()
{
    disp.displayWelcome();
}

bool ClientGtfs::startConnection(QString hostname, int port, int userTimeout)
{
    // Start talking to the server to hold open a connection
    // Should be automatically cleaned by destructor
    this->commSocket.connectToHost(hostname, port);

    // Have a problem?
    if (!this->commSocket.waitForConnected(userTimeout)) {
        this->disp.showError(this->commSocket.errorString());
        return false;
    }


    // Now that we are connected, show the server connected message (ehh ... there's no good point to do this)
//    disp.showServer(hostname, port);

    return true;
}

void ClientGtfs::repl()
{
    QTextStream screen(stdout);

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
            continue;
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
//                qDebug() << "! Found Terminal: " << tempRespStr.indexOf('\n');
                break;
            }
        }

        //
        // Display whole response in case decoding below doesn't happen
        //
        screen << "> " << responseStr << endl << endl; screen.flush();

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

        // Just cheat for now and show status
        // TODO: Must be broken into separate classes
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
                   << qSetFieldWidth(0) << endl << endl;

            // Easier to understand uptime
            qint64 uptimeMs = respObj["appuptime_ms"].toInt();
            qint32 days     = uptimeMs / 86400000;
            qint8  hours    = (uptimeMs - days * 86400000) / 3600000;
            qint8  mins     = (uptimeMs - days * 86400000 - hours * 3600000) / 60000;
            qint8  secs     = (uptimeMs - days * 86400000 - hours * 3600000 - mins * 60000) / 1000;

            screen << "[ Backend ]" << endl;
            screen << "Processed Reqs . . " << respObj["processed_reqs"].toInt() << endl;
            screen << "Uptime . . . . . . " << days  << "d " << qSetFieldWidth(2)
                                            << hours << "h "
                                            << mins  << "m "
                                            << secs  << "s " << qSetFieldWidth(0) << endl;
            screen << "Data Load Time . . " << respObj["dataloadtime_ms"].toInt() << "ms" << endl;
            screen << "Thread Pool  . . . " << respObj["threadpool_count"].toInt() << endl << endl;

            screen << "[ Static Feed Information ]" << endl;
            screen << "Publisher  . . . . " << respObj["feed_publisher"].toString() << endl;
            screen << "URL  . . . . . . . " << respObj["feed_url"].toString() << endl;
            screen << "Language . . . . . " << respObj["feed_lang"].toString() << endl;
            screen << "Valid Time . . . . " << "Start: " << respObj["feed_valid_start"].toString() << ", End: "
                                            << respObj["feed_valid_end"].toString() << endl;
            screen << "Version Text . . . " << respObj["feed_version"].toString() << endl;
            screen << "Recs Loaded  . . . " << respObj["records"].toInt() << endl;
            screen << endl;

            screen << "[ Agency Load ]" << endl;

            screen.setFieldAlignment(QTextStream::AlignLeft);
            screen << qSetFieldWidth(c1) << "ID"        << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "NAME"      << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c3) << "TIMEZONE"  << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c4) << "PHONE"     << qSetFieldWidth(0) << endl;

            QJsonArray agencies = respObj["agencies"].toArray();
            for (const QJsonValue &ag : agencies) {
                screen << qSetFieldWidth(c1) << ag["id"].toString().left(c1)   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << ag["name"].toString().left(c2) << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << ag["tz"].toString().left(c3)   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4) << ag["phone"].toString().left(c4)
                       << qSetFieldWidth( 0) << endl;
            }
            screen.setFieldAlignment(QTextStream::AlignRight);
            screen << endl;
        }
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
                   << qSetFieldWidth(0) << endl << endl;

            // Useful data?
            screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << endl << endl;

            // Start spitting out the routes
            screen.setFieldAlignment(QTextStream::AlignLeft);
            screen << qSetFieldWidth(c1) << "ID"         << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "SHORT NAME" << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c3) << "LONG NAME"  << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c4) << "TRIPS"      << qSetFieldWidth(0) << endl;

            QJsonArray routes = respObj["routes"].toArray();
            for (const QJsonValue &ro : routes) {
                screen << qSetFieldWidth(c1) << ro["id"].toString().left(c1)         << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << ro["short_name"].toString().left(c2) << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << ro["long_name"].toString().left(c3)  << qSetFieldWidth(1) << " ";
                screen.setFieldAlignment(QTextStream::AlignRight);
                screen << qSetFieldWidth(c4) << ro["nb_trips"].toInt()               << qSetFieldWidth(0) << endl;
                screen.setFieldAlignment(QTextStream::AlignLeft);
            }

            screen << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }
        else if (respObj["message_type"] == "TRI") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 20;                          // Remove 3 for SEQ + 10 for times + 2 for P/D + 5 for spacers
            qint32 c1 = 3;
            qint32 c2 = totalCol * 0.30;             // 30% of remaining space for STOP-ID
            qint32 c3 = totalCol - c2;               // All remaining space for STOP-NAME
            qint32 c4 = 1;
            qint32 c5 = 5;
            qint32 c6 = 5;

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Trip Schedule"
                   << qSetFieldWidth(this->disp.getCols() - 13)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << endl << endl;

            if (respObj["error"].toInt() == 101) {
                screen << "Trip not found in static database." << endl;
            } else if (respObj["error"].toInt() == 102) {
                screen << "Trip not found in real-time data feed." << endl;
            } else {
                bool realTimeData = respObj["real_time"].toBool();

                screen << "Trip ID  . . . . . " << respObj["trip_id"].toString() << endl;
                if (!realTimeData) {
                    screen << "Service ID . . . . " << respObj["service_id"].toString() << endl;
                    screen << "Validity . . . . . " << respObj["svc_start_date"].toString() << " - "
                                                    << respObj["svc_end_date"].toString() << endl;
                    screen << "Operating Days . . " << respObj["operate_days"].toString() << endl;
                    screen << "Exceptions . . . . " << respObj["exception_dates"].toString() << endl;
                    screen << "Additions  . . . . " << respObj["added_dates"].toString() << endl;
                }
                screen << "Route ID . . . . . " << respObj["route_id"].toString() << endl;
                screen << "Route Name . . . . " << respObj["route_short_name"].toString() << ", \""
                                                << respObj["route_long_name"].toString()  << "\"" << endl;
                if (!realTimeData) {
                    screen << "Headsign . . . . . " << respObj["headsign"].toString() << endl << endl;
                } else {
                    screen << "Real Time Data . . " << respObj["real_time_data_time"].toString() << endl << endl;
                }

                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)   << "SEQ"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)   << "STOP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3)   << "STOP-NAME" << qSetFieldWidth(1) << " ";

                if (!realTimeData) {
                    screen << qSetFieldWidth(c4*2) << "PD"        << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5)   << "SCH-A"     << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c6)   << "SCH-D"     << qSetFieldWidth(0) << endl;
                } else {
                    screen << qSetFieldWidth(c4*2) << "  "        << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5)   << "PRE-A"     << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c6)   << "PRE-D"     << qSetFieldWidth(0) << endl;
                }

                QJsonArray stops = respObj["stops"].toArray();
                for (const QJsonValue &st : stops) {
                    QString dropOff = dropoffToChar(st["drop_off_type"].toInt());
                    QString pickUp  = pickupToChar(st["pickup_type"].toInt());

                    screen << qSetFieldWidth(c1) << st["sequence"].toInt()              << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << st["stop_id"].toString().left(c2)   << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << st["stop_name"].toString().left(c3) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c4) << pickUp << dropOff                   << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5) << st["arr_time"].toString()           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c6) << st["dep_time"].toString()           << qSetFieldWidth(0) << endl;
                }
            }
            screen << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }
        else if (respObj["message_type"] == "TSR") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 45;               // Remove 45 columns for ValidTime, Op-Days, ES, SCH-D + 2 space
            qint32 c1 = totalCol * 0.5;   // 60% of open space for trip ID
            qint32 c2 = totalCol - c1;    // Remaining scalable range (~40%) for Headsign
            qint32 c3 = 9;                //  9 spaces (x2) for valid date range
            qint32 c4 = 14;               // 14 spaces for operating date range
            qint32 c5 = 1;                //  1 space  (x2) for exemption/supplement
            qint32 c6 = 5;                //  5 spaces for first-scheduled-departure

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Trips Serving Route"
                   << qSetFieldWidth(this->disp.getCols() - 19)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << endl << endl;

            if (respObj["error"].toInt() == 201) {
                screen << "Route not found." << endl;
            }
            else {
                screen << "Route ID . . . . . " << respObj["route_id"].toString() << endl;
                screen << "Route Name . . . . " << respObj["route_short_name"].toString() << ", \""
                                                << respObj["route_long_name"].toString()  << "\"" << endl;
                screen << "Service Date . . . " << respObj["service_date"].toString() << endl << endl;

                // Loop on all the trips
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)     << "TRIP-ID"        << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)     << "HEADSIGN"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3*2+1) << "VALID-DURATION" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4)     << "OPERATING-DAYS" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5*2)   << "ES"             << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c6)     << "SCH-D"          << qSetFieldWidth(0) << endl;

                QJsonArray trips = respObj["trips"].toArray();
                for (const QJsonValue &tr : trips) {
                    QString svcExempt = (tr["exceptions_present"].toBool())     ? "E": " ";
                    QString svcSplmnt = (tr["supplements_other_days"].toBool()) ? "S": " ";
                    QString dstOn     = " ";
//                    if (tr["dst_on"].isBool()) {
//                        if (tr["dst_on"].toBool()) {
//                            dstOn = "D";
//                        } else {
//                            dstOn = "S";
//                        }
//                    }
                    screen << qSetFieldWidth(c1) << tr["trip_id"].toString().left(c1)       << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << tr["headsign"].toString().left(c2)      << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << tr["svc_start_date"].toString()         << qSetFieldWidth(1) << "-"
                           << qSetFieldWidth(c3) << tr["svc_end_date"].toString()           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c4) << tr["operate_days_condensed"].toString().left(c4)
                           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5) << svcExempt << svcSplmnt
                           << qSetFieldWidth(1) << dstOn
                           << qSetFieldWidth(c6) << tr["first_stop_departure"].toString()  << qSetFieldWidth(0) << endl;
                }

                screen << endl << "Query took " << respObj["proc_time_ms"].toInt() << " ms, "
                       << trips.size() << " records loaded" << endl;
            }

            screen << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        else if (respObj["message_type"] == "TSS") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 49;               // Remove 48 columns for ValidTime, Op-Days, ES, TPD, SCH-D + 2 space
            qint32 c1 = totalCol * 0.5;   // 50% of open space for trip ID
            qint32 c2 = totalCol - c1;    // Remaining scalable range (~40%) for Headsign
            qint32 c3 = 9;                //  9 spaces (x2) for valid date range
            qint32 c4 = 14;               // 14 spaces for operating date range
            qint32 c5 = 1;                //  1 space  (x2) for exemption/supplement
            qint32 c6 = 5;                //  5 spaces for first-scheduled-departure

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Trips Serving Stop"
                   << qSetFieldWidth(this->disp.getCols() - 18)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << endl << endl;

            if (respObj["error"].toInt() == 301) {
                screen << "Stop not found." << endl;
            }
            else {
                // Count how many records were returned
                qint32 tripsLoaded = 0;

                screen << "Stop ID  . . . . . " << respObj["stop_id"].toString() << endl
                       << "Stop Name  . . . . " << respObj["stop_name"].toString() << endl
                       << "Stop Desc  . . . . " << respObj["stop_desc"].toString() << endl
                       << "Parent Station . . " << respObj["parent_sta"].toString() << endl
                       << "Service Date . . . " << respObj["service_date"].toString() << endl << endl;

                // ... then all the trips within each route
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)     << "TRIP-ID"        << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)     << "HEADSIGN"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3*2+1) << "VALID-DURATION" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4)     << "OPERATING-DAYS" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5*5)   << "ES TPD"         << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c6)     << "SCH-D"          << qSetFieldWidth(0) << endl << endl;

                // Loop on all the routes
                QJsonArray routes = respObj["routes"].toArray();
                for (const QJsonValue &ro : routes) {
                    screen << "[ Route ID " << ro["route_id"].toString() << " :: "
                                            << ro["route_short_name"].toString() << " :: "
                                            << ro["route_long_name"].toString()  << " ]"   << endl;

                    // Loop on all the trips
                    QJsonArray trips = ro["trips"].toArray();
                    for (const QJsonValue &tr : trips) {
                        QString headsign   = tr["headsign"].toString().left(c2);
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
                               << qSetFieldWidth(c2) << headsign                            << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c3) << tr["svc_start_date"].toString()     << qSetFieldWidth(1) << "-"
                               << qSetFieldWidth(c3) << tr["svc_end_date"].toString()       << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c4) << tr["operate_days_condensed"].toString().left(c4)
                               << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c5) << svcExempt << svcSplmnt << " " << tripTerm << pickUp << dropOff
                               << qSetFieldWidth(1) << dstIndic
                               << qSetFieldWidth(c6) << tr["dep_time"].toString() << qSetFieldWidth(0) << endl;
                    } // End loop on Trips-within-Route
                    tripsLoaded += trips.size();
                    screen << endl;
                } // End loop on Routes-serving-Stop

                screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms, "
                       << tripsLoaded << " trips loaded" << endl;
            }

            screen << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

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
                   << qSetFieldWidth(0) << endl << endl;

            if (respObj["error"].toInt() == 401) {
                screen << "Stop not found." << endl;
            }
            else {
                screen << "Stop ID/Name . . . " << respObj["stop_id"].toString() << " :: "
                                                << respObj["stop_name"].toString() << endl;
                screen << "Stop Desc  . . . . " << respObj["stop_desc"].toString() << endl;
                screen << "Location . . . . . " << respObj["loc_lat"].toDouble() << ", "
                                                << respObj["loc_lon"].toDouble() << endl;
                screen << "Parent Station . . " << respObj["parent_sta"].toString() << endl << endl;

                // Show all the Routes associated with the station requested
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << "[ Routes Serving Stop ]" << endl;
                screen << qSetFieldWidth(c1) << "ROUTE-ID"         << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << "ROUTE-NAME-SHORT" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << "ROUTE-NAME-LONG"  << qSetFieldWidth(1) << endl;

                // Loop on all the routes
                QJsonArray routes = respObj["routes"].toArray();
                for (const QJsonValue &ro : routes) {
                    screen << qSetFieldWidth(c1) << ro["route_id"].toString().left(c1)
                           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << ro["route_short_name"].toString().left(c2)
                           << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << ro["route_long_name"].toString().left(c3)
                           << qSetFieldWidth(0) << endl;
                }
                screen << endl;

                // Then display any associated stations with the parent
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << "[ Stops Sharing Parent Station ]" << endl;
                screen << qSetFieldWidth(c1) << "STOP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2) << "STOP-NAME" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3) << "STOP-DESC" << qSetFieldWidth(1) << endl;

                QJsonArray sharedStops = respObj["stops_sharing_parent"].toArray();
                for (const QJsonValue &ss : sharedStops) {
                    screen << qSetFieldWidth(c1) << ss["stop_id"].toString().left(c1)   << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << ss["stop_name"].toString().left(c2) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << ss["stop_desc"].toString().left(c3) << qSetFieldWidth(0) << endl;
                }
                screen << endl;
                screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << endl;
            }

            screen << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

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
                   << qSetFieldWidth(0) << endl << endl;

            if (respObj["error"].toInt() == 501) {
                screen << "Route not found." << endl;
            }
            else {
                screen << "Route ID . . . . . " << respObj["route_id"].toString() << endl
                       << "Short Name . . . . " << respObj["route_short_name"].toString() << endl
                       << "Long Name  . . . . " << respObj["route_long_name"].toString() << endl
                       << "Description  . . . " << respObj["route_desc"].toString() << endl
                       << "Type . . . . . . . " << respObj["route_type"].toString() << endl
                       << "URL  . . . . . . . " << respObj["route_url"].toString() << endl
                       << "Color  . . . . . . " << respObj["route_color"].toString() << endl
                       << "Text Color . . . . " << respObj["route_text_color"].toString() << endl << endl;

                // Show all the Routes associated with the station requested
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << "[ Stops Served by Route ]" << endl;
                screen << qSetFieldWidth(c1)     << "STOP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)     << "STOP-NAME" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3)     << "STOP-DESC" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4)     << "#TRIPS"    << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5 * 2) << "LOCATION"  << qSetFieldWidth(1) << endl;

                // Loop on all the routes
                QJsonArray stops = respObj["stops"].toArray();
                for (const QJsonValue &so : stops) {
                    screen << qSetFieldWidth(c1) << so["stop_id"].toString().left(c1)   << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c2) << so["stop_name"].toString().left(c2) << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c3) << so["stop_desc"].toString().left(c3) << qSetFieldWidth(1) << " ";
                    screen.setFieldAlignment(QTextStream::AlignRight);
                    screen << qSetFieldWidth(c4) << so["trip_count"].toInt()            << qSetFieldWidth(1) << " "
                           << qSetFieldWidth(c5) << so["stop_lat"].toDouble()           << qSetFieldWidth(1) << ","
                           << qSetFieldWidth(c5) << so["stop_lon"].toDouble()           << qSetFieldWidth(0) << endl;
                    screen.setFieldAlignment(QTextStream::AlignLeft);
                }
                screen << endl;
            }
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        else if (respObj["message_type"] == "NEX") {
            // Scalable column width determination
            qint32 totalCol = this->disp.getCols();
            totalCol -= 25;               // Remove space for fixed-width fields and spacers
            qint32 c1 = totalCol * 0.5;   // 50% of open space for trip ID
            qint32 c2 = totalCol - c1;    // Remaining scalable range (~50%) Headsign
            qint32 c3 = 1;                //  1 space  (x3) for terminate + pickup/drop-off exception flags
            qint32 c4 = 3;                //  9 spaces for date-of-service field
            qint32 c5 = 5;                //  9 spaces for date-of-service field
            qint32 c6 = 4;                //  1 space  (x2) for exemption/supplement

            this->disp.clearTerm();

            // Standard Response Header
            screen << "Upcoming Service at Stop"
                   << qSetFieldWidth(this->disp.getCols() - 24)
                   << respObj["message_time"].toString()
                   << qSetFieldWidth(0) << endl << endl;

            if (respObj["error"].toInt() == 601) {
                screen << "Stop not found." << endl;
            }
            else {
                // Count how many records were returned
                qint32 tripsLoaded = 0;

                screen << "Stop ID  . . . . . " << respObj["stop_id"].toString()   << endl
                       << "Stop Name  . . . . " << respObj["stop_name"].toString() << endl
                       << "Stop Desc  . . . . " << respObj["stop_desc"].toString() << endl << endl;

                // ... then all the trips within each route
                screen.setFieldAlignment(QTextStream::AlignLeft);
                screen << qSetFieldWidth(c1)      << "TRIP-ID"   << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c2)      << "HEADSIGN"  << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c3*3)    << "TPD"       << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c4+c5+1) << "STOP-TIME" << qSetFieldWidth(1) << " "
                       << qSetFieldWidth(c5)      << "MINS"
                       << qSetFieldWidth(c5)      << "STAT"      << qSetFieldWidth(0) << endl;

                // Loop on all the routes
                QJsonArray routes = respObj["routes"].toArray();
                for (const QJsonValue &ro : routes) {
                    screen << "[ Route ID " << ro["route_id"].toString() << " :: "
                                            << ro["route_short_name"].toString() << " :: "
                                            << ro["route_long_name"].toString()  << " ]"   << endl;

                    // Loop on all the trips
                    QJsonArray trips = ro["trips"].toArray();
                    for (const QJsonValue &tr : trips) {
                        QString headsign   = tr["headsign"].toString().left(c2);
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
                               << qSetFieldWidth(c2) << headsign.left(c2)                 << qSetFieldWidth(1) << " "
                               << qSetFieldWidth(c3) << tripTerm << pickUp << dropOff     << qSetFieldWidth(1) << " ";
                        screen.setFieldAlignment(QTextStream::AlignRight);

                        /*
                         * Real-Time Processing -- Check if the "realtime_data" object is present
                         */
                        if (tr["realtime_data"].isObject()) {
                            QJsonObject rt = tr["realtime_data"].toObject();

                            QString rtStatus = rt["status"].toString();

                            // Print the actual time of arrival and countdown
                            if (rtStatus == "SKIP") {
                                screen << qSetFieldWidth(c4+c5+1) << tr["dep_time"].toString()
                                       << qSetFieldWidth(1) << " "
                                       << qSetFieldWidth(c6) << "-" << qSetFieldWidth(1) << " ";
                            } else if (rtStatus == "CNCL") {
                                // Print regular schedule information with no add-ons
                                screen << qSetFieldWidth(c4+c5+1)  << tr["dep_time"].toString()
                                       << qSetFieldWidth(1) << " " << qSetFieldWidth(c6) << " "
                                       << qSetFieldWidth(1) << " " << qSetFieldWidth(0);
                            } else {
                                screen << qSetFieldWidth(c4+c5+1) << rt["actual_time"].toString()
                                       << qSetFieldWidth(1)       << " "
                                       << qSetFieldWidth(c6)      << waitTimeMin
                                       << qSetFieldWidth(1)       << " ";
                            }

                            if (rtStatus == "ERLY" || rtStatus == "LATE") {
                                screen.setNumberFlags(QTextStream::ForceSign);
                                screen << qSetFieldWidth(c6) << rt["offset_seconds"].toInt() / 60 << qSetFieldWidth(0);
                                noforcesign(screen);
                            } else {
                                screen << qSetFieldWidth(c6) << rtStatus << qSetFieldWidth(0);
                            }
                        } else {
                            // Just print regular schedule information with no add-ons
                            screen << qSetFieldWidth(c4+c5+1) << tr["dep_time"].toString() << qSetFieldWidth(1) << " "
                                   << qSetFieldWidth(c6)      << waitTimeMin               << qSetFieldWidth(0);
                        }

                        screen.setFieldAlignment(QTextStream::AlignLeft);
                        screen << endl;

                    } // End loop on Trips-within-Route

                    tripsLoaded += trips.size();
                    screen << endl;
                } // End loop on Routes-serving-Stop

                screen << "Query took " << respObj["proc_time_ms"].toInt() << " ms, "
                       << tripsLoaded << " trips loaded" << endl;
            }

            screen << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

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
                   << qSetFieldWidth(0) << endl << endl;

            screen.setFieldAlignment(QTextStream::AlignLeft);
            screen << qSetFieldWidth(c1) << "STOP-ID"   << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "STOP-NAME" << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c3) << "STOP-DESC" << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c4) << "LOCATION"  << qSetFieldWidth(1) << endl;

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
                screen << qSetFieldWidth(c4/2) << st["loc_lat"].toDouble()
                       << qSetFieldWidth(1)    << ","
                       << qSetFieldWidth(c4/2) << st["loc_lon"].toDouble()
                       << qSetFieldWidth(0)    << endl;
                screen.setFieldAlignment(QTextStream::AlignLeft);
            }
            screen << endl << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << endl << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

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
                   << qSetFieldWidth(0) << endl << endl;

            screen.setFieldAlignment(QTextStream::AlignLeft);

            // Mutual Exclusion
            screen << "[ Mutual Exclusion ]" << endl
                   << "Active Side  . . . " << respObj["active_side"].toString() << endl
                   << "Data Age . . . . . " << respObj["active_age_sec"].toInt() << " s" << endl
                   << "Feed Time  . . . . " << respObj["active_feed_time"].toString() << endl
                   << "Download Time  . . " << respObj["active_download_ms"].toInt() << " ms" << endl
                   << "Integ Time . . . . " << respObj["active_integration_ms"].toInt() << " ms" << endl
                   << "Next Fetch In  . . " << respObj["seconds_to_next_fetch"].toInt() << " s" << endl
                   << endl << endl;


            screen << qSetFieldWidth(c1) << "TRIP-ID"   << qSetFieldWidth(1) << " "
                   << qSetFieldWidth(c2) << "ROUTE-ID"  << qSetFieldWidth(1) << endl;

            // Loop on all the routes
            //QJsonArray stops = respObj["stops"].toArray();

            screen << endl << "Query took " << respObj["proc_time_ms"].toInt() << " ms" << endl << endl;
            screen.setFieldAlignment(QTextStream::AlignRight);
        }

        // Probably some kind of error condition, just respond in kind
        else {
            screen << endl << "No handler for the request: " << endl << responseStr << endl;
        }
    }

    // On exit (loop halted above)
    screen << "*** GOOD BYE ***" << endl;
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
