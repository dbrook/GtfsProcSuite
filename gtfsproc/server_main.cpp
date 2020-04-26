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
    QCoreApplication::setApplicationVersion("1.3");

    QTextStream console(stdout);
    QString appName = QCoreApplication::applicationName();
    QString appVers = QCoreApplication::applicationVersion();

    console << appName.remove("\"") << " version " << appVers.remove("\"")
            << " - Running on Process ID: " << QCoreApplication::applicationPid() << endl << endl;

    /*
     * Command Line Parsing
     */
    QCommandLineParser parser;
    parser.setApplicationDescription("GTFS Processor written with the Qt Framework");
    parser.addVersionOption();
    parser.addHelpOption();
    QCommandLineOption dataRootOption(QStringList() << "d" << "dataRoot",
                                      QCoreApplication::translate("main", "GTFS static feed directory."),
                                      QCoreApplication::translate("main", "path to feed"));
    QCommandLineOption serverPortOption(QStringList() << "p" << "serverPort",
                                        QCoreApplication::translate("main", "Port to listen for requests."),
                                        QCoreApplication::translate("main", "port number"));
    QCommandLineOption serverThreads(QStringList() << "t" << "threads",
                                     QCoreApplication::translate("main", "Number of processing threads."),
                                     QCoreApplication::translate("main", "nb threads"));
    QCommandLineOption realTimeOption(QStringList() << "r" << "realTimeData",
                                      QCoreApplication::translate("main", "Real-time (GTFS) data path."),
                                      QCoreApplication::translate("main", "URL or file"));
    QCommandLineOption realTimeRefresh(QStringList() << "u" << "realTimeUpdate",
                                       QCoreApplication::translate("main", "Time between real-time updates."),
                                       QCoreApplication::translate("main", "nb. seconds"));
    QCommandLineOption fixedLocalTime(QStringList() << "f" << "fixedDateTime",
                                      QCoreApplication::translate("main", "Force local time for all modules."),
                                      QCoreApplication::translate("main", "y,m,d,h,m,s"));
    QCommandLineOption dumpRTProtobuf(QStringList() << "x" << "examineRTPB",
                                      QCoreApplication::translate("main", "Examine GTFS-Realtime ProtoBuf."));
    QCommandLineOption ampmTimes(QStringList() << "a" << "use12hour",
                                 QCoreApplication::translate("main", "Show times with 12-hour AM/PM."));
    QCommandLineOption noRTDateMatch(QStringList() << "n" << "noRTDate",
                                     QCoreApplication::translate("main", "Skip date match process for realtime."));

    parser.addOption(dataRootOption);
    parser.addOption(serverPortOption);
    parser.addOption(serverThreads);
    parser.addOption(realTimeOption);
    parser.addOption(realTimeRefresh);
    parser.addOption(fixedLocalTime);
    parser.addOption(dumpRTProtobuf);
    parser.addOption(ampmTimes);
    parser.addOption(noRTDateMatch);
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

    bool skipRTDateMatching = false;
    if (parser.isSet(noRTDateMatch)) {
        skipRTDateMatching = true;
    }

    int nbProcThreads = 1;
    if (parser.isSet(serverThreads)) {
        nbProcThreads = parser.value(serverThreads).toInt();
    }
    QThreadPool::globalInstance()->setMaxThreadCount(nbProcThreads);

    ServeGTFS gtfsRequestServer(databaseRootPath,
                                realTimePath,
                                rtDataInterval,
                                unchangingLocalTime,
                                protobufToQDebug,
                                use12HourTimes,
                                skipRTDateMatching);
    gtfsRequestServer.displayDebugging();

    /*
     * BEGIN LISTENING FOR CONNECTIONS
     */
    if (gtfsRequestServer.listen(QHostAddress::Any, portNum)) {
        console << "SERVER STARTED - READY TO ACCEPT INCOMING CONNECTIONS" << endl << endl;
    } else {
        console << gtfsRequestServer.errorString();
        console << endl << "(!) COULD NOT START SERVER - SEE ERROR STRING ABOVE" << endl;
        return 1;
    }

    return a.exec();
}
