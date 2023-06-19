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

#ifndef GTFSTRIP_H
#define GTFSTRIP_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>

namespace GTFS {

typedef struct {
//    QString trip_id;        // Primary data key, needed by StopTimes
    QString route_id;         // The route on which this trip operates
    QString service_id;       // Needed to see which day(s) trip is active
    QString trip_headsign;    // OPTIONAL: Almost always filled, this is what the vehicle destination usually is
    QString trip_short_name;  // OPTIONAL: Sometimes used to indicate an easier-to-grok label like the "Train Number"
    /*
     * There are several additional fields here but we are ignoring them for the time being
     */
} TripRec;

// Map for all stop-times. String represents the trip_id, the vector is all the stops in sequence .
typedef QHash<QString, TripRec> TripData;

/*
 * GTFS::Trips is a wrapper around the GTFS Feed's trips.txt file
 */
class Trips : public QObject
{
    Q_OBJECT
public:
    // Constructor
    explicit Trips(const QString dataRootPath, QObject *parent = nullptr);

    // Returns the number of records loaded that pertain to the trips.txt file
    qint64 getTripsDBSize() const;

    // Database retrieval function
    const TripData &getTripsDB() const;

private:
    // Determine the order of the 'interesting' fields within the trip database
    static void tripsCSVOrder(const QVector<QString> csvHeader,
                              qint8 &routeIdPos,
                              qint8 &tripIdPos,
                              qint8 &serviceIdPos,
                              qint8 &headsignPos,
                              qint8 &tripShortNamePos);

    // Trip Database
    TripData tripDb;
};

} // Namespace GTFS

#endif // GTFSTRIP_H
