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
    parser.addPositionalArgument("dbRoot", QCoreApplication::translate("main", "path to all files from feed"));
    parser.process(a);

    const QStringList args = parser.positionalArguments();
    QString databaseRootPath = args.at(0);

    /*
     * Primary Application Data Load and Server Connection Setup
     *
     *
     */

    ServeGTFS gtfsRequestServer(databaseRootPath);
    gtfsRequestServer.displayDebugging();

    /*
     * BEGIN LISTENING FOR CONNECTIONS
     *
     *
     * Port 5000 from any IP address ... why not?
     */
    if (gtfsRequestServer.listen(QHostAddress::Any, 5000)) {
        qDebug() << "SERVER HAS BEEN STARTED";
    } else {
        qCritical() << gtfsRequestServer.errorString();
    }

    return a.exec();
}
