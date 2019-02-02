#include "datagateway.h"

#include "qdebug.h"

namespace GTFS {

DataGateway &DataGateway::inst()
{
    static DataGateway *_instance = 0;
    if (_instance == 0) {
        _instance = new DataGateway();
        _instance->handledRequests = 0;
    }
    return *_instance;
}

void DataGateway::initDataPath(QString databaseFilePath)
{
    this->_dbRootPath = databaseFilePath;
}

void DataGateway::initStatus()
{
    this->_status = new GTFS::Status(this->_dbRootPath, this);
}

void DataGateway::initRoutes()
{
    this->_routes = new GTFS::Routes(this->_dbRootPath, this);
    this->_status->incrementRecordsLoaded(this->_routes->getRoutesDBSize());
}

void DataGateway::initOperatingDay()
{
    this->_opDay = new GTFS::OperatingDay(this->_dbRootPath, this);
    this->_status->incrementRecordsLoaded(this->_opDay->getCalendarAndDatesDBSize());
}

void DataGateway::initTrips()
{
    this->_trips = new GTFS::Trips(this->_dbRootPath, this);
    this->_status->incrementRecordsLoaded(this->_trips->getTripsDBSize());
}

void DataGateway::initStopTimes()
{
    this->_stopTimes = new GTFS::StopTimes(this->_dbRootPath, this);
    this->_status->incrementRecordsLoaded(this->_stopTimes->getStopTimesDBSize());
}

void DataGateway::initStops()
{
    this->_stops = new GTFS::Stops(this->_dbRootPath, this);
    this->_status->incrementRecordsLoaded(this->_stops->getStopsDBSize());
}

void DataGateway::linkStopsTripsRoutes()
{
    QMap<QString, QVector<StopTimeRec>> sTimDB = this->_stopTimes->getStopTimesDB();
    QMap<QString, TripRec> tripDB              = this->_trips->getTripsDB();

    // For every StopTime in the database, bind its trip_id and route_id to its stop_id
    for (const QString &stopTimeTripID : sTimDB.keys()) {
        for (qint32 sTimeIdx = 0; sTimeIdx < sTimDB[stopTimeTripID].length(); ++sTimeIdx) {
            const StopTimeRec &rec = sTimDB[stopTimeTripID].at(sTimeIdx);

            // Sort the stop times for every stop after they're all connected. If a stop time is available for a stop,
            // then use it for the sort. This is not always the case as some stops are un-timed depending on the
            // publishing agency, so in those particular cases, WE SORT BASED ON THE NEXT AVAILABLE TIME IN THE TRIP SEQ
            qint32 sortTime = -1;
            if (rec.departure_time != -1) {
                sortTime = rec.departure_time;
            } else if (rec.arrival_time != -1) {
                sortTime = rec.arrival_time;
            } else {
                // Neither time is available for this stop, so find the next one in the chronology ( loop in a loop :( )
                for (qint32 sTimeIdxAhead = sTimeIdx;
                     sTimeIdxAhead < sTimDB[stopTimeTripID].length();
                     ++sTimeIdxAhead) {
                    if (sTimDB[stopTimeTripID].at(sTimeIdxAhead).departure_time != -1) {
                        sortTime = sTimDB[stopTimeTripID].at(sTimeIdxAhead).departure_time;
                        break;
                    } else if (sTimDB[stopTimeTripID].at(sTimeIdxAhead).arrival_time != -1) {
                        sortTime = sTimDB[stopTimeTripID].at(sTimeIdxAhead).arrival_time;
                        break;
                    }
                }
            }

            if (sortTime == -1) {
                qDebug() << "WARNING: a sortTime was not findable for Route: " << tripDB[stopTimeTripID].route_id
                         << ", Trip: " << stopTimeTripID << ", Stop: " << rec.stop_id;
            }

            this->_stops->connectTripRoute(rec.stop_id,
                                           stopTimeTripID,
                                           tripDB[stopTimeTripID].route_id,
                                           sTimeIdx,
                                           sortTime);
        }
    }

    this->_stops->sortStopTripTimes();

    // For every route, there are stops served but those stops are coded in every single trip, which is annoying if you
    // want to see the full scope of available service per route. What we've done here is to associate stops to routes
    // as well as the # of trips that serve them so the fuller-scale of service is clearer.
    for (const QString &stopTimeTripID : sTimDB.keys()) {
        for (const StopTimeRec &rec : sTimDB[stopTimeTripID]) {
            this->_routes->connectStop(tripDB[stopTimeTripID].route_id, rec.stop_id);
        }
    }
}

void DataGateway::linkTripsRoutes()
{
    QMap<QString, TripRec>  tripDB  = this->_trips->getTripsDB();
    QMap<QString, QVector<StopTimeRec>> stopTimeDB = this->_stopTimes->getStopTimesDB();

    // For every TripID in the database, bind it to its associated RouteID
    // (This will only insert them in the order from the data provider, which is difficult to grok) ...
    for (QString tripID : tripDB.keys()) {
        this->_routes->connectTrip(tripDB[tripID].route_id, tripID,
                                   stopTimeDB[tripID].at(0).departure_time, stopTimeDB[tripID].at(0).arrival_time);
    }

    // ... so we will sort them after they're all connected
    this->_routes->sortRouteTrips();
}

const Status *DataGateway::getStatus()
{
    return this->_status;
}

const QMap<QString, GTFS::RouteRec> *DataGateway::getRoutesDB()
{
    return &this->_routes->getRoutesDB();
}

const QMap<QString, TripRec> *DataGateway::getTripsDB()
{
    return &this->_trips->getTripsDB();
}

const QMap<QString, QVector<StopTimeRec> > *DataGateway::getStopTimesDB()
{
    return &this->_stopTimes->getStopTimesDB();
}

const QMap<QString, StopRec> *DataGateway::getStopsDB()
{
    return &this->_stops->getStopDB();
}

const QMap<QString, QVector<QString> > *DataGateway::getParentStationDB()
{
    return &this->_stops->getParentStationDB();
}

const OperatingDay *DataGateway::getServiceDB()
{
    return this->_opDay;
}

void DataGateway::setStatusLoadFinishTimeUTC()
{
    this->_status->setLoadFinishTimeUTC();
}

qint64 DataGateway::incrementHandledRequests()
{
    // Must be thread safe ;)
    lock_handledRequests.lock();
    qint64 HRs = ++handledRequests;
    lock_handledRequests.unlock();
    return HRs;
}

qint64 DataGateway::getHandledRequests()
{
    // Must be thread safe ;)
    lock_handledRequests.lock();
    qint64 HRs = handledRequests;
    lock_handledRequests.unlock();
    return HRs;
}

DataGateway::DataGateway(QObject *parent) :
    QObject(parent)
{    
}

DataGateway::~DataGateway()
{
    delete this->_status;
    delete this->_routes;
    delete this->_opDay;
    delete this->_trips;
    delete this->_stopTimes;
}

} // namespace GTFS
