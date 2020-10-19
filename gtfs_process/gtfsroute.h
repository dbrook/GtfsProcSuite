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

#ifndef GTFSROUTE_H
#define GTFSROUTE_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QVector>
#include <QPair>

namespace GTFS {

// To save space we only use the required subset of route records
typedef struct {
//  QString  route_id;          // Primary Key, we don't need to define this here
    QString  agency_id;
    QString  route_short_name;
    QString  route_long_name;
    QString  route_desc;
    QString  route_type;
    QString  route_url;
    QString  route_color;
    QString  route_text_color;
    QVector<QPair<QString, qint32>> trips;  // Trips and their first departure time associated to each route
    QHash<QString, qint32> stopService;     // Holds all the stops served by the route across all trips (+frequency)
} RouteRec;

// Map for all routes. String represents the route_id.
typedef QHash<QString, RouteRec> RouteData;

/*
 * GTFS::Routes is a wrapper around a GTFS feed's routes.txt data
 */
class Routes : public QObject
{
    Q_OBJECT
public:
    // Constructor
    explicit Routes(const QString dataRootPath, QObject *parent = nullptr);

    // Returns the number of records stored that pertain to routes.txt
    qint64 getRoutesDBSize() const;

    // Read-only access to the Routes data
    const RouteData &getRoutesDB() const;

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
    RouteData routeDb;
};

}  // Namespace GTFS

#endif // GTFSROUTE_H
