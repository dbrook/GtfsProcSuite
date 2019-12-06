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

#ifndef OPERATINGDAY_H
#define OPERATINGDAY_H

#include <QObject>
#include <QString>
#include <QDate>
#include <QVector>
#include <QMap>

namespace GTFS {

// Calendar CSV record contents
typedef struct {
    QString  service_id;
    bool     monday;
    bool     tuesday;
    bool     wednesday;
    bool     thursday;
    bool     friday;
    bool     saturday;
    bool     sunday;
    QDate    start_date;
    QDate    end_date;
} CalendarRec;

// Calendar Day CSV record contents. These are used as overrides to the schedule (CalendarRecs)
typedef struct {
    QString  service_id;
    QDate    date;
    qint16   exception_type;    // 1 == service added for CalDateRec::date, 2 == service removed for CalDateRec::date
} CalDateRec;

/*
 * GTFS::OperatingDay is a wrapper around both calendar.txt and calendar_dates.txt from the GTFS Feed
 */
class OperatingDay : public QObject
{
    Q_OBJECT
public:
    // Constructor
    explicit OperatingDay(const QString dataRootPath, QObject *parent = nullptr);

    // Returns the number of records loaded relating to the date processing
    qint64 getCalendarAndDatesDBSize() const;

    /*
     * Function will return true if the service specified is running on the date specified.
     *
     * It first scans the date exceptions (holidays) as defined in calendar_dates.txt. If the date is not found on
     * this initial scan, then we will return based on the day-of-week determined by the current date.
     */
    bool serviceRunning(QDate serviceDate, QString serviceName)/*
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

 const;

    /*
     * Get a list of days (of the week) for which the serviceName is active
     */
    QString serializeOpDays     (const QString &serviceName) const;
    QString shortSerializeOpDays(const QString &serviceName) const;
    void    booleanOpDays       (const QString &serviceName,
                                 bool &mon, bool &tue, bool &wed, bool &thu, bool &fri, bool &sat, bool &sun) const;

    /*
     * Get a list of dates for which extra service was added for a service_id
     */
    QString serializeAddedServiceDates(const QString &serviceName) const;
    bool    serviceAddedOnOtherDates  (const QString &serviceName) const;

    /*
     * Get a list of dates for which service was removed for a service_id (like a holiday)
     */
    QString serializeNoServiceDates   (const QString &serviceName) const;
    bool    serviceRemovedOnDates     (const QString &serviceName) const;

    /*
     * Returns start/end of validity for service requested
     */
    QDate getServiceStartDate(const QString &serviceName) const;
    QDate getServiceEndDate(const QString &serviceName) const;

    /*
     * For individual trip lookup information (bypass the "is service running" stuff)
     */
    const QMap<QString, CalendarRec> &getServiceDB() const;

private:
    // Determine the order of the CSV table columns from calendar.txt
    static void calendarCSVOrder(const QVector<QString> csvHeader,
                                 qint8 &servicePos,
                                 qint8 &monPos,
                                 qint8 &tuePos,
                                 qint8 &wedPos,
                                 qint8 &thuPos,
                                 qint8 &friPos,
                                 qint8 &satPos,
                                 qint8 &sunPos,
                                 qint8 &sDatePos,
                                 qint8 &eDatePos);

    // Determine the order of the CSV table columns from calendar_dates.txt
    static void calendarDatesCSVOrder(const QVector<QString> csvHeader,
                                      qint8 &idPos,
                                      qint8 &datePos,
                                      qint8 &exceptionPos);

    // Calendar Database
    QMap<QString, CalendarRec>         calendarDb;
    QMap<QString, QVector<CalDateRec>> calendarDateDb;
};

} // Namespace GTFS
#endif // OPERATINGDAY_H
