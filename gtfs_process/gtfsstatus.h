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

#ifndef GTFSSTATUS_H
#define GTFSSTATUS_H

#include <QObject>
#include <QDateTime>
#include <QTimeZone>
#include <QVector>

namespace GTFS {

// Represents a single record from the agencies.txt GTFS file
typedef struct {
    QString agency_id;
    QString agency_name;
    QString agency_url;
    QString agency_timezone;
    QString agency_lang;
    QString agency_phone;
} AgencyRecord;

/*
 * The GTFS::Status class wraps around the GTFS feed's agency.txt and feed_info.txt (if present) file(s)
 */
class Status : public QObject
{
    Q_OBJECT
public:
    // Constructor
    explicit Status(const QString dataRootPath, const QString &frozenDateTime, QObject *parent = nullptr);

    // Returns the number of records loaded from the agency.txt and feed_into.txt files
    qint64    getRecordsLoaded() const;

    // Returns the date and time of the server's actual start time before any data was integrated
    QDateTime getServerStartTimeUTC() const;

    // In order to see how many records were loaded from the GTFS feed, call this to increment by value
    void      incrementRecordsLoaded(const qint64 &value);

    // Date/Time process for when the server data integration stopped
    QDateTime getLoadFinishTimeUTC() const;
    void      setLoadFinishTimeUTC();

    // Data from feed_info.txt
    QString   getPublisher() const;
    QString   getUrl() const;
    QString   getLanguage() const;
    QDate     getStartDate() const;
    QDate     getEndDate() const;
    QString   getVersion() const;

    // Retrieve agency information
    const QVector<AgencyRecord> &getAgencies() const;

    // What the hell time is it where our feed is based?
    const QTimeZone &getAgencyTZ() const;

    // Return a null QDateTime if there is no override in place. A valid QDateTime indicates the actual current time
    // should not be used for the sake of showing upcoming trips
    const QDateTime &getOverrideDateTime() const;

private:
    // Determine order of CSV table columns from feed_info.txt
    static void feedInfoCSVOrder(const QVector<QString> csvHeader,
                                 qint8 &publisherPos,
                                 qint8 &urlPos,
                                 qint8 &languagePos,
                                 qint8 &versionPos,
                                 qint8 &startDatePos,
                                 qint8 &endDatePos);

    // Determine order of CSV table columns from agency.txt
    static void agencyCSVOrder(const QVector<QString> csvHeader,
                               qint8 &idPos,
                               qint8 &namePos,
                               qint8 &urlPos,
                               qint8 &tzPos,
                               qint8 &langPos,
                               qint8 &phonePos);

    qint64     recordsLoaded;        // Number of lines loaded from the GTFS files
    QDateTime  serverStartTimeUTC;   // Start time of the processor
    QDateTime  loadFinishTimeUTC;    // Time when the whole database finished loading

    // Data straight from "feed_info.txt"
    QString    publisher;
    QString    url;
    QString    language;
    QDate      startDate;
    QDate      endDate;
    QString    version;

    // Agency Information
    QVector<AgencyRecord> Agencies;

    QTimeZone  serverFeedTZ;

    // A date and time to force the local time to for debugging purposes
    QDateTime  frozenAgencyTime;
};

}  // Namespace GTFS

#endif // GTFSSTATUS_H
