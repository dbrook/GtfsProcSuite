/*
 * GtfsProc_Server
 * Copyright (C) 2018-2024, Daniel Brook
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

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QThreadPool>
#include <QTextStream>

#include "servegtfs.h"

int main(int argc, char *argv[])
{
    /*
     * Core Qt Framework Initialization
     */
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("GtfsProc");
    QCoreApplication::setApplicationVersion("2.3.3");

    QTextStream console(stdout);
    QString appName = QCoreApplication::applicationName();
    QString appVers = QCoreApplication::applicationVersion();

    console << appName.remove("\"") << " version " << appVers.remove("\"")
            << " - Running on Process ID: " << QCoreApplication::applicationPid() << Qt::endl << Qt::endl;

    /*
     * Command Line Parsing
     */
    QCommandLineParser parser;
    parser.setApplicationDescription("GTFS Processor written with the Qt Framework");
    parser.addVersionOption();
    parser.addHelpOption();
    QCommandLineOption dataRootOption(QStringList() << "d",
                    QCoreApplication::translate("main", "GTFS static feed directory (local directory, unzipped)."),
                    QCoreApplication::translate("main", "path"));
    QCommandLineOption serverPortOption(QStringList() << "p",
                    QCoreApplication::translate("main", "Port to listen for requests."),
                    QCoreApplication::translate("main", "portNum"));
    QCommandLineOption serverThreads(QStringList() << "t",
                    QCoreApplication::translate("main", "Number of processing threads for simultaneous transactions."),
                    QCoreApplication::translate("main", "nbThreads"));
    QCommandLineOption realTimeOption(QStringList() << "r",
                    QCoreApplication::translate("main", "GTFS-Realtime data location (remote or local)."),
                    QCoreApplication::translate("main", "URL or file"));
    QCommandLineOption realTimeRefresh(QStringList() << "u",
                    QCoreApplication::translate("main", "Real-time fetch interval, check agency policies before use."),
                    QCoreApplication::translate("main", "nbSeconds"));
    QCommandLineOption ampmTimes(QStringList() << "a",
                    QCoreApplication::translate("main", "Render all times with 12-hour AM/PM format, not 24-hour."));

    // Trip Processing Behavioral Options
    QCommandLineOption hideTermTrips(QStringList() << "m",
                    QCoreApplication::translate("main", "Do not show terminating trips in NEX/NCF modules."));
    QCommandLineOption tripsPerNEX(QStringList() << "s",
                    QCoreApplication::translate("main", "Number of trips to show per route in NEX responses."),
                    QCoreApplication::translate("main", "nbRtTrips"));
    QCommandLineOption noRTDateMatch(QStringList() << "l",
                    QCoreApplication::translate("main", "Real-time date matching level (read the documentation!)."),
                    QCoreApplication::translate("main", "0|1|2"));
    QCommandLineOption noRTStopSeqEnf(QStringList() << "q",
                    QCoreApplication::translate("main", "Do not check stop sequences in real-time feed to static."));

    // Non-Standard Override Options - things that (sort of (?)) violate the GTFS specificaiton but are necessary to
    // display the data normally or as expected - we'll break these into a list
    QCommandLineOption zOptions(QStringList() << "z",
                    QCoreApplication::translate("main", "Processing Overrides List."),
                    QCoreApplication::translate("main", "Comma-separated Options"));

    // Debugging Options - Fix system date and time for debugging, dump realtime protobuf when processing
    QCommandLineOption showFrontendRequests(QStringList() << "i",
                    QCoreApplication::translate("main", "Show every transaction and real-time update to the screen."));
    QCommandLineOption fixedLocalTime(QStringList() << "f",
                    QCoreApplication::translate("main", "Freeze GtfsProc on this local time for all requests."),
                    QCoreApplication::translate("main", "y,m,d,h,m,s"));
    QCommandLineOption dumpRTProtobuf(QStringList() << "x",
                    QCoreApplication::translate("main", "Display GTFS-Realtime ProtoBuf to stderr upon receiving."));

    parser.addOption(dataRootOption);
    parser.addOption(serverPortOption);
    parser.addOption(serverThreads);
    parser.addOption(realTimeOption);
    parser.addOption(realTimeRefresh);
    parser.addOption(ampmTimes);
    parser.addOption(hideTermTrips);
    parser.addOption(tripsPerNEX);
    parser.addOption(noRTDateMatch);
    parser.addOption(noRTStopSeqEnf);
    parser.addOption(zOptions);
    parser.addOption(showFrontendRequests);
    parser.addOption(fixedLocalTime);
    parser.addOption(dumpRTProtobuf);
    parser.process(a);

    QString unchangingLocalTime;
    if (parser.isSet(fixedLocalTime)) {
        unchangingLocalTime = parser.value(fixedLocalTime);
    }

    QString databaseRootPath;
    if (parser.isSet(dataRootOption)) {
        databaseRootPath = parser.value(dataRootOption);
    } else {
        qDebug() << "Must have a data root path";
        return 1;
    }

    quint16 portNum = 5000;  // Open on port 5000 by default, but you can use "-p $PORT" to use whichever you want
    if (parser.isSet(serverPortOption)) {
        portNum = parser.value(serverPortOption).toUShort();
    }

    QString realTimePath;
    if (parser.isSet(realTimeOption)) {
        realTimePath = parser.value(realTimeOption);
    }

    bool protobufToQDebug = false;
    if (parser.isSet(dumpRTProtobuf)) {
        protobufToQDebug = true;
    }

    /*
     * Primary Application Data Load and Server Connection Setup
     * (Includes static dataset and real-time data, which by default will refresh every 2 minutes, and no more often
     * then 10 seconds to prevent DDOSing the server)
     */
    qint32 rtDataInterval = 120;
    if (parser.isSet(realTimeRefresh)) {
        rtDataInterval = parser.value(realTimeRefresh).toInt();
        if (rtDataInterval < 10) {
            rtDataInterval = 10;
        }
    }

    bool use12HourTimes = false;
    if (parser.isSet(ampmTimes)) {
        use12HourTimes = true;
    }

    quint32 realTimeDateMatchLevel = 0;
    if (parser.isSet(noRTDateMatch)) {
        realTimeDateMatchLevel = parser.value(noRTDateMatch).toUInt();
        if (realTimeDateMatchLevel > 2) {
            qDebug() << "WARNING: An invalid date matching level (-l option) was found, defaulting to STRICTEST (0)";
            realTimeDateMatchLevel = 0;
        }
    }

    int nbProcThreads = 1;
    if (parser.isSet(serverThreads)) {
        nbProcThreads = parser.value(serverThreads).toInt();
    }
    QThreadPool::globalInstance()->setMaxThreadCount(nbProcThreads);

    quint32 nbTripsPerNEXRoute = 4;
    if (parser.isSet(tripsPerNEX)) {
        nbTripsPerNEXRoute = parser.value(tripsPerNEX).toUInt();
    }

    bool hideTerminatingTripsNEXNCF = false;
    if (parser.isSet(hideTermTrips)) {
        hideTerminatingTripsNEXNCF = true;
    }

    bool showTransactions = false;
    if (parser.isSet(showFrontendRequests)) {
        showTransactions = true;
    }

    bool loosenRTStopSeqStopIDEnforce = false;
    if (parser.isSet(noRTStopSeqEnf)) {
        loosenRTStopSeqStopIDEnforce = true;
    }

    ServeGTFS gtfsRequestServer(databaseRootPath,
                                realTimePath,
                                rtDataInterval,
                                unchangingLocalTime,
                                protobufToQDebug,
                                use12HourTimes,
                                realTimeDateMatchLevel,
                                showTransactions,
                                nbTripsPerNEXRoute,
                                hideTerminatingTripsNEXNCF,
                                loosenRTStopSeqStopIDEnforce,
                                parser.value(zOptions));
    gtfsRequestServer.displayDebugging();

    /*
     * BEGIN LISTENING FOR CONNECTIONS
     */
    if (gtfsRequestServer.listen(QHostAddress::Any, portNum)) {
        console << "SERVER STARTED - READY TO ACCEPT INCOMING CONNECTIONS" << Qt::endl << Qt::endl;
    } else {
        console << gtfsRequestServer.errorString();
        console << Qt::endl << "(!) COULD NOT START SERVER - SEE ERROR STRING ABOVE" << Qt::endl;
        return 1;
    }

    return a.exec();
}
