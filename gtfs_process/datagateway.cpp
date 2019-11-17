/*
 * GtfsProc_Server
 * Copyright (C) 2018-2019, Daniel Brook
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

#include "datagateway.h"
#include "qdebug.h"

namespace GTFS {

DataGateway &DataGateway::inst()
{
    static DataGateway *_instance = nullptr;
    if (_instance == nullptr) {
        _instance = new DataGateway();
        _instance->handledRequests  = 0;
        _instance->stopsNoSortTimes = 0;
    }
    return *_instance;
}

void DataGateway::initDataPath(QString databaseFilePath)
{
    _dbRootPath = databaseFilePath;
}

void DataGateway::initStatus()
{
    _status = new GTFS::Status(_dbRootPath, this);
}

void DataGateway::initRoutes()
{
    _routes = new GTFS::Routes(_dbRootPath, this);
    _status->incrementRecordsLoaded(_routes->getRoutesDBSize());
}

void DataGateway::initOperatingDay()
{
    _opDay = new GTFS::OperatingDay(_dbRootPath, this);
    _status->incrementRecordsLoaded(_opDay->getCalendarAndDatesDBSize());
}

void DataGateway::initTrips()
{
    _trips = new GTFS::Trips(_dbRootPath, this);
    _status->incrementRecordsLoaded(_trips->getTripsDBSize());
}

void DataGateway::initStopTimes()
{
    _stopTimes = new GTFS::StopTimes(_dbRootPath, this);
    _status->incrementRecordsLoaded(_stopTimes->getStopTimesDBSize());
}

void DataGateway::initStops()
{
    _stops = new GTFS::Stops(_dbRootPath, this);
    _status->incrementRecordsLoaded(_stops->getStopsDBSize());
}

void DataGateway::linkStopsTripsRoutes()
{
    QMap<QString, QVector<StopTimeRec>> sTimDB = _stopTimes->getStopTimesDB();
    QMap<QString, TripRec> tripDB              = _trips->getTripsDB();

    // For every StopTime in the database, bind its trip_id and route_id to its stop_id
    for (const QString &stopTimeTripID : sTimDB.keys()) {
        for (qint32 sTimeIdx = 0; sTimeIdx < sTimDB[stopTimeTripID].length(); ++sTimeIdx) {
            const StopTimeRec &rec = sTimDB[stopTimeTripID].at(sTimeIdx);

            // Sort the stop times for every stop after they're all connected. If a stop time is available for a stop,
            // then use it for the sort. This is not always the case as some stops are un-timed depending on the
            // publishing agency, so in those particular cases, WE SORT BASED ON THE NEXT AVAILABLE TIME IN THE TRIP SEQ
            qint32 sortTime = -1;
            if (rec.arrival_time != -1) {
                sortTime = rec.arrival_time;
            } else if (rec.departure_time != -1) {
                sortTime = rec.departure_time;
            } else {
                // Neither time is available for this stop, so find the next one in the chronology ( loop in a loop :( )
                for (qint32 sTimeIdxAhead = sTimeIdx;
                     sTimeIdxAhead < sTimDB[stopTimeTripID].length();
                     ++sTimeIdxAhead) {
                    if (sTimDB[stopTimeTripID].at(sTimeIdxAhead).arrival_time != -1) {
                        sortTime = sTimDB[stopTimeTripID].at(sTimeIdxAhead).arrival_time;
                        break;
                    } else if (sTimDB[stopTimeTripID].at(sTimeIdxAhead).departure_time != -1) {
                        sortTime = sTimDB[stopTimeTripID].at(sTimeIdxAhead).departure_time;
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

    _stops->sortStopTripTimes();

    // For every route, there are stops served but those stops are coded in every single trip, which is annoying if you
    // want to see the full scope of available service per route. What we've done here is to associate stops to routes
    // as well as the # of trips that serve them so the fuller-scale of service is clearer.
    for (const QString &stopTimeTripID : sTimDB.keys()) {
        for (const StopTimeRec &rec : sTimDB[stopTimeTripID]) {
            _routes->connectStop(tripDB[stopTimeTripID].route_id, rec.stop_id);
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
        _routes->connectTrip(tripDB[tripID].route_id, tripID,
                             stopTimeDB[tripID].at(0).departure_time, stopTimeDB[tripID].at(0).arrival_time);
    }

    // ... so we will sort them after they're all connected
    _routes->sortRouteTrips();
}

const Status         *DataGateway::getStatus()      {return _status;}
const RouteData      *DataGateway::getRoutesDB()    {return &_routes->getRoutesDB();}
const TripData       *DataGateway::getTripsDB()     {return &_trips->getTripsDB();}
const StopTimeData   *DataGateway::getStopTimesDB() {return &_stopTimes->getStopTimesDB();}
const StopData       *DataGateway::getStopsDB()     {return &_stops->getStopDB();}
const ParentStopData *DataGateway::getParentsDB()   {return &_stops->getParentStationDB();}
const OperatingDay   *DataGateway::getServiceDB()   {return _opDay;}
void  DataGateway::setStatusLoadFinishTimeUTC()     {_status->setLoadFinishTimeUTC();}

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
    delete _status;
    delete _routes;
    delete _opDay;
    delete _trips;
    delete _stopTimes;
}

} // namespace GTFS
