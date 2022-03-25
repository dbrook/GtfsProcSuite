/*
 * GtfsProc_Server
 * Copyright (C) 2018-2022, Daniel Brook
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

#ifndef GTFSSTOPS_H
#define GTFSSTOPS_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QVector>
#include <QPair>

namespace GTFS {

typedef struct {
    QString tripID;
    qint32  tripStopIndex;
    qint32  sortTime;
} tripStopSeqInfo;

typedef struct {
    // Data from File
    QString stop_name;
    QString stop_desc;
    QString stop_lat;
    QString stop_lon;
    QString parent_station;

    // All the trips serving an individual stop_id (for quicker trip-stop processing at runtime)
    QHash<QString, QVector<tripStopSeqInfo>> stopTripsRoutes;
} StopRec;

// Map for all stops. String represents the stop_id.
typedef QHash<QString, StopRec> StopData;

// Map for all parent stops. String represents the parent_station, and the vector within is the list of child stop_ids
typedef QHash<QString, QVector<QString>> ParentStopData;

/*
 * GTFS::Stops is a wrapper around the GTFS feed's stops.txt file
 */
class Stops : public QObject
{
    Q_OBJECT
public:
    // Constructor
    explicit Stops(const QString dataRootPath, QObject *parent = nullptr);

    // Returns the number of records loaded pertaining to the stops.txt file
    qint64 getStopsDBSize() const;

    // Database retrieval function
    const StopData &getStopDB() const;
    const ParentStopData &getParentStationDB() const;

    // Association Builder (to link all the trips that service each stop for quick lookups)
    // Note the notion of "sortTime" = stop's departure time > stop's arrival time > trip's first departure time
    // THE sortTime IS ONLY FOR SORTING PURPOSES AND SHOULD NOT BE DISPLAYED IN OUTPUT
    void connectTripRoute(const QString &StopID,
                          const QString &TripID,
                          const QString &RouteID,
                          qint32 TripSequence,
                          qint32 sortTime);

    // Sorter for the stop-trips' times for easier reading
    void sortStopTripTimes();
    static bool compareStopTrips(tripStopSeqInfo &tripStop1, tripStopSeqInfo &tripStop2);

private:
    static void stopsCSVOrder(const QVector<QString> csvHeader,
                              qint8 &stopIdPos,
                              qint8 &stopDescPos,
                              qint8 &stopNamePos,
                              qint8 &stopLatPos,
                              qint8 &stopLonPos,
                              qint8 &parentStationPos);

    // Stop database
    StopData       stopsDb;
    ParentStopData parentStopDb;
};

} // Namespace GTFS

#endif // GTFSSTOPS_H
