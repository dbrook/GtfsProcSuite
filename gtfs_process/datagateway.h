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

class DataGateway : public QObject
{
    Q_OBJECT
public:
    static DataGateway & inst();

    // Set path to database
    void initDataPath(QString databaseFilePath);

    // Add feed information and initialize items. Use setStatusLoadFinishTimeUTC to save when load finished
    void initStatus();
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
    // Data Access
    //
    const Status *getStatus();
    const QMap<QString, RouteRec> *getRoutesDB();
    const QMap<QString, GTFS::TripRec> *getTripsDB();
    const QMap<QString, QVector<GTFS::StopTimeRec>> *getStopTimesDB();
    const QMap<QString, GTFS::StopRec> *getStopsDB();
    const QMap<QString, QVector<QString>> *getParentStationDB();
    const OperatingDay *getServiceDB();

private:
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
};

} // namespace GTFS
#endif // DATAGATEWAY_H
