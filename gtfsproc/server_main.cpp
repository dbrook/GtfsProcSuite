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
#include <QSettings>

#include "servegtfs.h"

int main(int argc, char *argv[])
{
    /*
     * Core Qt Framework Initialization
     */
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("GtfsProc");
    QCoreApplication::setApplicationVersion("2.4.0");

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

    // Debugging Options - Fix system date and time for debugging, dump realtime protobuf when processing
    QCommandLineOption confFilePath(QStringList() << "c",
                    QCoreApplication::translate("main", "Configuration file"),
                    QCoreApplication::translate("main", "path"));
    QCommandLineOption showFrontendRequests(QStringList() << "i",
                    QCoreApplication::translate("main", "Show every transaction and real-time update to the screen."));
    QCommandLineOption fixedLocalTime(QStringList() << "f",
                    QCoreApplication::translate("main", "Freeze GtfsProc on this local time for all requests."),
                    QCoreApplication::translate("main", "y,m,d,h,m,s"));

    parser.addOption(showFrontendRequests);
    parser.addOption(fixedLocalTime);
    parser.addOption(confFilePath);
    parser.process(a);

    QString configurationFilePath;
    if (parser.isSet(confFilePath)) {
        configurationFilePath = parser.value(confFilePath);
    } else {
        qDebug() << "NOTE: As of GtfsProc 2.4.0, server startup configuration is done via an INI file (-c).";
        return 1;
    }

    QString unchangingLocalTime;
    if (parser.isSet(fixedLocalTime)) {
        unchangingLocalTime = parser.value(fixedLocalTime);
    }

    bool showTransactions = false;
    if (parser.isSet(showFrontendRequests)) {
        showTransactions = true;
    }

    QSettings gtfsProcSettings(configurationFilePath, QSettings::IniFormat);

    QString databaseRootPath             = gtfsProcSettings.value("static/dataPath").toString();
    quint16 portNum                      = gtfsProcSettings.value("static/serverPort").toUInt();
    bool    use12HourTimes               = gtfsProcSettings.value("static/clock12hFormat").toBool();
    int     nbProcThreads                = gtfsProcSettings.value("static/numberThreads").toInt();
    quint32 nbTripsPerNEXRoute           = gtfsProcSettings.value("static/nexTripsPerRoute").toUInt();
    bool    hideTerminatingTripsNEXNCF   = gtfsProcSettings.value("static/hideTerminating").toBool();
    QString zOptions                     = gtfsProcSettings.value("static/zOptions").toString();

    QString realTimePath                 = gtfsProcSettings.value("realtime/feedLocation").toString();
    bool    loosenRTStopSeqStopIDEnforce = gtfsProcSettings.value("realtime/skipStopSeqMatch").toBool();
    quint32 realTimeDateMatchLevel       = gtfsProcSettings.value("realtime/serviceDateMatch").toUInt();
    qint32  rtDataInterval               = gtfsProcSettings.value("realtime/updateInterval").toInt();

    QThreadPool::globalInstance()->setMaxThreadCount(nbProcThreads);
    ServeGTFS gtfsRequestServer(databaseRootPath,
                                realTimePath,
                                rtDataInterval,
                                unchangingLocalTime,
                                use12HourTimes,
                                realTimeDateMatchLevel,
                                showTransactions,
                                nbTripsPerNEXRoute,
                                hideTerminatingTripsNEXNCF,
                                loosenRTStopSeqStopIDEnforce,
                                zOptions);
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
