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
#include <QTextStream>

#include "servegtfs.h"

int main(int argc, char *argv[])
{
    /*
     * Core Qt Framework Initialization
     */
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("GtfsProc");
    QCoreApplication::setApplicationVersion("0.2");

    QTextStream console(stdout);
    QString appName = QCoreApplication::applicationName();
    QString appVers = QCoreApplication::applicationVersion();

    console << endl << appName.remove("\"") << " version " << appVers.remove("\"")
            << " - Running on Process ID: " << QCoreApplication::applicationPid() << endl << endl;

    /*
     * Command Line Parsing
     */
    QCommandLineParser parser;
    parser.setApplicationDescription("GTFS Processor written with the Qt Framework");
    parser.addVersionOption();
    parser.addHelpOption();
    QCommandLineOption dataRootOption(QStringList() << "d" << "dataRoot",
                                      QCoreApplication::translate("main", "GTFS static feed"),
                                      QCoreApplication::translate("main", "path to feed"));
    QCommandLineOption serverPortOption(QStringList() << "p" << "serverPort",
                                        QCoreApplication::translate("main", "Port on which to receive requests"),
                                        QCoreApplication::translate("main", "port number"));
    QCommandLineOption realTimeOption(QStringList() << "r" << "realTimeData",
                                      QCoreApplication::translate("main", "Real-time (GTFS) data path"),
                                      QCoreApplication::translate("main", "URL or local path"));

    parser.addOption(dataRootOption);
    parser.addOption(serverPortOption);
    parser.addOption(realTimeOption);
    parser.process(a);

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

    /*
     * Primary Application Data Load and Server Connection Setup
     * (Includes static dataset and real-time data)
     */
    ServeGTFS gtfsRequestServer(databaseRootPath, realTimePath);
    gtfsRequestServer.displayDebugging();

    /*
     * BEGIN LISTENING FOR CONNECTIONS
     */
    if (gtfsRequestServer.listen(QHostAddress::Any, portNum)) {
        console << endl << "SERVER STARTED - READY TO ACCEPT INCOMING CONNECTIONS" << endl << endl;
    } else {
        qCritical() << gtfsRequestServer.errorString();
    }

    return a.exec();
}
