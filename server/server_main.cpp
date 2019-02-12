/*
 * This is the main server application
 *
 *  - Starts the enhanced TCP Server from VoidRealms
 *  - Processes incoming requests and responds with the desired data (hopefully)
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

#include "servegtfs.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("GtfsProc");
    QCoreApplication::setApplicationVersion("0.1");

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

    qint32 portNum = 5000;  // Open on port 5000 by default
    if (parser.isSet(serverPortOption)) {
        portNum = parser.value(serverPortOption).toInt();
    }

    QString realTimePath;
    if (parser.isSet(realTimeOption)) {
        realTimePath = parser.value(realTimeOption);
    }

    /*
     * Primary Application Data Load and Server Connection Setup
     */
    ServeGTFS gtfsRequestServer(databaseRootPath, realTimePath);
    gtfsRequestServer.displayDebugging();

    /*
     * Real-Time Data Acquisition (if requested)
     */
    //TODO: Use constructor...

    /*
     * BEGIN LISTENING FOR CONNECTIONS
     */
    if (gtfsRequestServer.listen(QHostAddress::Any, portNum)) {
        qDebug() << "SERVER HAS BEEN STARTED";
    } else {
        qCritical() << gtfsRequestServer.errorString();
    }

    return a.exec();
}
