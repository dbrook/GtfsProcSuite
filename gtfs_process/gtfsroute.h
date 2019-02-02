#ifndef GTFSROUTE_H
#define GTFSROUTE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>
#include <QPair>

namespace GTFS {

// We do not really need all the records so let's save some space
typedef struct {
//  QString  route_id;          // Primary Key, we don't need to define this here
    qint32   agency_id;
    QString  route_short_name;
    QString  route_long_name;
    QString  route_desc;
    QString  route_type;
    QString  route_url;
    QString  route_color;
    QString  route_text_color;
    QVector<QPair<QString, qint32>> trips;  // Trips and their first departure time associated to each route
    QMap<QString, qint32> stopService;      // Holds all the stops served by the route across all trips (+frequency)
} RouteRec;

class Routes : public QObject
{
    Q_OBJECT
public:
    explicit Routes(const QString dataRootPath, QObject *parent = nullptr);

    qint64 getRoutesDBSize() const;

    const QMap<QString, RouteRec> &getRoutesDB() const;

    // Association Builder (to make lookups quicker at the expense of horrendous memory usage!)
    void connectTrip(const QString &routeID, const QString &tripID, const qint32 fstDepTime, const qint32 fstArrTime);

    // Associate a stop ID to a route (or increment a stop ID that was already added)
    void connectStop(const QString &routeID, const QString &stopID);

    // Sort all the trips by the order in which they depart (do not call until all trips are connected with connectTrip)
    void        sortRouteTrips();
    static bool compareByTripStartTime(const QPair<QString, qint32> &tripA, const QPair<QString, qint32> &tripB);

private:
    // Determine order of CSV table columns from agency.txt
    static void routesCSVOrder(const QVector<QString> csvHeader,
                               qint8 &idPos,
                               qint8 &agencyIdPos,
                               qint8 &shortNamePos,
                               qint8 &longNamePos,
                               qint8 &descPos,
                               qint8 &typePos,
                               qint8 &urlPos,
                               qint8 &colorPos,
                               qint8 &textColorPos);

    // Route Database
    QMap<QString, RouteRec> routeDb;
};

}  // Namespace GTFS

#endif // GTFSROUTE_H
