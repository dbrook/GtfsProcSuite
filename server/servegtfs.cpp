#include "servegtfs.h"

// GTFS Static Data
#include "datagateway.h"
#include "gtfsconnection.h"

// GTFS RealTime Data
#include "gtfsrealtimegateway.h"

#include <QThread>
#include <QDebug>

ServeGTFS::ServeGTFS(QString dbRootPath, QString realTimePath, QObject *parent) : TcpServer(parent)
{
    // Setup the global data access
    GTFS::DataGateway &data = GTFS::DataGateway::inst();
    data.initDataPath(dbRootPath);

    // Populate each data set
    data.initStatus();                   // "Status" is special, it holds process, feed_info.txt, and agency.txt data
    data.initRoutes();                   // Fill the Routes database from routes.txt
    data.initOperatingDay();             // Fill the calendar.txt and calendar_dates.txt
    data.initTrips();                    // Fill the trips.txt data
    data.initStopTimes();                // Fill the stop_times.txt data
    data.initStops();                    // Fill the stops.txt data

    // Post-Processing of Data Load
    data.linkTripsRoutes();              // Associate all the trips to the routes they serve
    data.linkStopsTripsRoutes();         // Associate all possible TripIDs + RouteIDs to every stop served (and sort)

    // Note when we finished loading (for performance analysis)
    GTFS::DataGateway::inst().setStatusLoadFinishTimeUTC();

    // If Real-Time data is requested, then we also need to load it
    if (realTimePath.isEmpty()) {
        return;
    }

    GTFS::RealTimeGateway &rtData = GTFS::RealTimeGateway::inst();
    rtData.setRealTimeFeedPath(realTimePath);
    rtData.refetchData();

    // The real-time processor must be able to independently download new realtime protobuf files
    QThread *rtThread = new QThread;
    GTFS::RealTimeGateway::inst().moveToThread(rtThread);
    connect(rtThread, SIGNAL(started()), &GTFS::RealTimeGateway::inst(), SLOT(dataRetrievalLoop()));
    rtThread->start();
}

ServeGTFS::~ServeGTFS()
{
    // TODO: Look into destructor needs (probably handled by QObject mechanism though
}

void ServeGTFS::displayDebugging() const
{
    const GTFS::Status *data = GTFS::DataGateway::inst().getStatus();
    qDebug() << "[ GTFS Static Data Information ]";
    qDebug() << "Recs Loaded . . . ." << data->getRecordsLoaded();
    qDebug() << "Server Start Time ." << data->getServerStartTimeUTC();
    qDebug() << "Feed Publisher  . ." << data->getPublisher();
    qDebug() << "Feed URL  . . . . ." << data->getUrl();
    qDebug() << "Feed Language . . ." << data->getLanguage();
    qDebug() << "Feed Start Date . ." << data->getStartDate().toString("dd-MMM-yyyy");
    qDebug() << "Feed End Date . . ." << data->getEndDate().toString("dd-MMM-yyyy");
    qDebug() << "Feed Version  . . ." << data->getVersion();
}

void ServeGTFS::incomingConnection(qintptr descriptor)
{
    qDebug() << "Discovered an incoming GTFS Request: " << descriptor;

    GtfsConnection *connection = new GtfsConnection();

    //
    // Rate Limiting configuration inserted here if necessary
    //

    // Accept a connection into the event loop
    accept(descriptor, connection);
}
