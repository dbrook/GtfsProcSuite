/*
 * GtfsProc_Server
 * Copyright (C) 2018-2023, Daniel Brook
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

#ifndef DATAGATEWAY_H
#define DATAGATEWAY_H

#include "gtfsstatus.h"
#include "gtfsroute.h"
#include "operatingday.h"
#include "gtfstrip.h"
#include "gtfsstoptimes.h"
#include "gtfsstops.h"

#include <QObject>
#include <QMutex>

namespace GTFS {

/*
 * GTFS::DataGateway is a singleton class used to retrieve GTFS data for cross-referencing
 */
class DataGateway : public QObject
{
    Q_OBJECT
public:
    // Primary member function to do work with the DataGateway
    static DataGateway & inst();

    // Set path to database
    void initDataPath(QString databaseFilePath);

    // Add feed information and initialize items. Use setStatusLoadFinishTimeUTC to save when load finished
    void initStatus(const QString frozenLocalDateTime, bool use12hourTimes, quint32 maxTripsNEX, bool hideTermTrips);
    void initRoutes();
    void initOperatingDay();
    void initTrips();
    void initStopTimes();
    void initStops();

    //
    // Data load post-processing functions - used to make data access more efficient than scanning entire DB
    //

    // Associate all Trip Stops (StopTime) and Routes to all the StopIDs in the database
    void linkStopsTripsRoutes();

    // Associate all the Trips to their respective routes in the database
    void linkTripsRoutes();

    //
    // Initialization complete - set timestamp
    //
    void setStatusLoadFinishTimeUTC();

    //
    // increment the number of transactions
    //
    qint64 incrementHandledRequests();
    qint64 getHandledRequests();

    //
    // Direct External Data Access
    //
    const Status         *getStatus();
    const RouteData      *getRoutesDB();
    const TripData       *getTripsDB();
    const StopTimeData   *getStopTimesDB();
    const StopData       *getStopsDB();
    const ParentStopData *getParentsDB();
    const OperatingDay   *getServiceDB();

private:
    // Required per the Singleton Pattern
    explicit DataGateway(QObject *parent = nullptr);
    explicit DataGateway(const DataGateway &);
    DataGateway &operator =(DataGateway const &other);
    virtual ~DataGateway();

    // Dataset members
    QString             _dbRootPath;
    GTFS::Status       *_status;
    GTFS::Routes       *_routes;
    GTFS::OperatingDay *_opDay;
    GTFS::Trips        *_trips;
    GTFS::StopTimes    *_stopTimes;
    GTFS::Stops        *_stops;

    QMutex lock_handledRequests;
    qint64 handledRequests;
    qint32 stopsNoSortTimes;
};

} // namespace GTFS
#endif // DATAGATEWAY_H
