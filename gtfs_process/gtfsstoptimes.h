/*
 * GtfsProc_Server
 * Copyright (C) 2018-2021, Daniel Brook
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

#ifndef GTFSSTOPTIMES_H
#define GTFSSTOPTIMES_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QVector>

namespace GTFS {

typedef struct {
    qint32  stop_sequence;

    // Arrival and Departure Times for the stop ID in the trip. If no time is present, the value is StopTimes::kNoTime
    qint32  arrival_time;     // in seconds relative to local noon of the operating day (can exceed 12-hours!)
    qint32  departure_time;   // in seconds relative to local noon of the operating day (can exceed 12-hours!)

    /*
     * There are several more feeds which are not mandatory (nor are interesting here), so we will skip them for now.
     * Except for drop-off and pick-up types ... that is actually an interesting concept as we should indicate them.
     * These are stored as-is from the data: 0 = normal, 1 = not allowed, 2 = call agency first, 3 = arrange with driver
     * so it is up to the front-end to decide how/if to show this information (as this software is intended to be used
     * as more of a wait / countdown for service, the pickup types might be the most interesting
     */
    qint16  drop_off_type;
    qint16  pickup_type;

    QString stop_id;

    // And another one just for fun: the each individual stop in a trip (i.e. one of these stop-time records) can have
    // a unique / standalone headsign just for it (independent of the overall journey)
    QString stop_headsign;
} StopTimeRec;

// Map for all stop-times. String represents the trip_id, the vector is all the stops in sequence .
typedef QHash<QString, QVector<StopTimeRec>> StopTimeData;

/*
 * GTFS::StopTimes is a wrapper around the GTFS Feed's stop_times.txt file
 */
class StopTimes : public QObject
{
    Q_OBJECT
public:
    // When a time is not present the value of arrival_time / departure_time will be GTFS::StopTimes::kNoTime
    const static qint32 kNoTime;

    // Constructor
    explicit StopTimes(const QString dataRootPath, QObject *parent = nullptr);

    // Returns the size of the data associated to stop_times
    qint64 getStopTimesDBSize() const;

    // Database retrieval
    const StopTimeData &getStopTimesDB() const;

    // Sorter
    static bool compareByStopSequence(const StopTimeRec &a, const StopTimeRec &b);

    // Notion of local noon (for offset calculations)
    static qint32 computeSecondsLocalNoonOffset(const QString &hhmmssTime);  // Send time as (h)h:mm:ss
    static const qint32 s_localNoonSec;

private:
    static void stopTimesCSVOrder(const QVector<QString> csvHeader,
                                  qint8 &tripIdPos,
                                  qint8 &stopSeqPos,
                                  qint8 &stopIdPos,
                                  qint8 &arrTimePos,
                                  qint8 &depTimePos,
                                  qint8 &dropOffPos,
                                  qint8 &pickupPos,
                                  qint8 &stopHeadsignPos);

    bool operator <(const StopTimeRec &strec) const;

    // Stop Times Database
    StopTimeData stopTimeDb;
};

} // Namespace GTFS

#endif // GTFSSTOPTIMES_H
