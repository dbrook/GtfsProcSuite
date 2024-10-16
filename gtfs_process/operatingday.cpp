/*
 * GtfsProc_Server
 * Copyright (C) 2018-2024, Daniel Brook
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

#include "operatingday.h"
#include "csvprocessor.h"

#include <QDebug>
#include <QFileInfo>

namespace GTFS {

const qint32 k12hInSec = 43200;

OperatingDay::OperatingDay(const QString dataRootPath, QObject *parent) : QObject(parent)
{
    QVector<QVector<QString>> dataStore;

    // Ingest the calendar information if it exists: NOTE: Either calendar_dates.txt and/or calendar.txt must exist
    if (QFileInfo::exists(dataRootPath + "/calendar.txt")) {
        qDebug() << "Starting Calendar Information Process ...";
        CsvProcess((dataRootPath + "/calendar.txt").toUtf8(), &dataStore);
        qint8 servicePos, monPos, tuePos, wedPos, thuPos, friPos, satPos, sunPos, sDatePos, eDatePos;
        calendarCSVOrder(dataStore.at(0),
                         servicePos, monPos, tuePos, wedPos, thuPos, friPos, satPos, sunPos, sDatePos, eDatePos);

        for (int l = 1; l < dataStore.size(); ++l) {
            CalendarRec cal;
            cal.service_id   = dataStore.at(l).at(servicePos);
            cal.monday       = (dataStore.at(l).at(monPos).toInt() == 1);
            cal.tuesday      = (dataStore.at(l).at(tuePos).toInt() == 1);
            cal.wednesday    = (dataStore.at(l).at(wedPos).toInt() == 1);
            cal.thursday     = (dataStore.at(l).at(thuPos).toInt() == 1);
            cal.friday       = (dataStore.at(l).at(friPos).toInt() == 1);
            cal.saturday     = (dataStore.at(l).at(satPos).toInt() == 1);
            cal.sunday       = (dataStore.at(l).at(sunPos).toInt() == 1);
            QStringView dateStrStart = dataStore.at(l).at(sDatePos);
            QStringView dateStrEnd = dataStore.at(l).at(eDatePos);
            cal.start_date   = QDate(dateStrStart.left (4)   .toInt(),
                                     dateStrStart.mid  (4, 2).toInt(),
                                     dateStrStart.right(2)   .toInt());
            cal.end_date     = QDate(dateStrEnd.left (4)   .toInt(),
                                     dateStrEnd.mid  (4, 2).toInt(),
                                     dateStrEnd.right(2)   .toInt());
            this->calendarDb[dataStore.at(l).at(servicePos)] = cal;
        }
    }

    dataStore.clear();

    // Ingest the calednar_dates (override) information (again, see above, it is possible for at least 1 to exist
    if (QFileInfo::exists(dataRootPath + "/calendar_dates.txt")) {
        CsvProcess((dataRootPath + "/calendar_dates.txt").toUtf8(), &dataStore);
        qint8 idPos, datePos, exceptionPos;
        calendarDatesCSVOrder(dataStore.at(0), idPos, datePos, exceptionPos);

        for (int l = 1; l < dataStore.size(); ++l) {
            CalDateRec cd;
            cd.service_id       = dataStore.at(l).at(idPos);
            cd.exception_type   = dataStore.at(l).at(exceptionPos).toShort();
            QStringView dateStr = dataStore.at(l).at(datePos);
            cd.date             = QDate(dateStr.left (4)   .toInt(),
                                        dateStr.mid  (4, 2).toInt(),
                                        dateStr.right(2)   .toInt());
            this->calendarDateDb[dataStore.at(l).at(idPos)].push_back(cd);
        }
    }
}

qint64 OperatingDay::getCalendarAndDatesDBSize() const
{
    qint64 sumOfRec = 0;

    for (const QVector<CalDateRec> &cdrec : this->calendarDateDb) {
        sumOfRec += cdrec.size();
    }

    return sumOfRec + this->calendarDb.size();
}

bool OperatingDay::serviceRunning(QDate serviceDate, QString serviceName) const
{
    // Try to find the current day in the overrides
    QHash<QString, QVector<CalDateRec>>::const_iterator cdi = this->calendarDateDb.find(serviceName);

    if (cdi != this->calendarDateDb.end()) {
        // We found the service, see if today is a holiday / overridden day
        for (const CalDateRec &cdr : *cdi) {
            if (cdr.date == serviceDate) {
                if (cdr.exception_type == 1) {
                    return true;
                } else {           // (== 2)
                    return false;
                }
            }
        }
    }


    // No clear resolution from the Calendar-Date Database, just go off the day of the week...
    QHash<QString, CalendarRec>::const_iterator cri = this->calendarDb.find(serviceName);

    if (cri == this->calendarDb.end()) {
        // This is actually not an error, the service requested may potentially exist only as a calendar_date
        // (basically see it as more of an exception to the rule, the rule being that the route is predominantly
        // not serviced except for the days defined and probably matched above.
        return false;
    }

    // ... should we actually be in the range of the service start and end dates
    // NOTE: The start_date and end_date values are INCLUSIVE (meaning an end_date of 10mar2019 will have service)
    if (serviceDate < cri->start_date || serviceDate > cri->end_date) {
        return false;
    }

    // See if today's day of the week is an operating day for the service requested
    // The QDate framework considers a First-day-of-Week as MONDAY == 1 up to SUNDAY == 7
    int dayOfWeek = serviceDate.dayOfWeek();

    if (dayOfWeek == 0) {
        // Should not happen, also an error case (systemDate does not fall on a real day of the week)
        // TODO: define and raise an exception
        // For now, return false
        qDebug() << "Service Date is 0 for day of week!";
        return false;
    } else if (dayOfWeek == 1) {
        return cri->monday;
    } else if (dayOfWeek == 2) {
        return cri->tuesday;
    } else if (dayOfWeek == 3) {
        return cri->wednesday;
    } else if (dayOfWeek == 4) {
        return cri->thursday;
    } else if (dayOfWeek == 5) {
        return cri->friday;
    } else if (dayOfWeek == 6) {
        return cri->saturday;
    } else if (dayOfWeek == 7) {
        return cri->sunday;
    }

    // We should not be able to get down here
    // TODO: define and raise an exception
    // For now, return false
    qDebug() << "Got to where we shouldn't";
    return false;
}

QString OperatingDay::serializeOpDays(const QString &serviceName) const
{
    CalendarRec serviceCal = this->calendarDb[serviceName];
    QString days;

    // Let's pull no punches and go full European with the date serialization (MON-SUN weeks)
    if (serviceCal.monday)
        days += "MON ";
    if (serviceCal.tuesday)
        days += "TUE ";
    if (serviceCal.wednesday)
        days += "WED ";
    if (serviceCal.thursday)
        days += "THU ";
    if (serviceCal.friday)
        days += "FRI ";
    if (serviceCal.saturday)
        days += "SAT ";
    if (serviceCal.sunday)
        days += "SUN ";

    return days;
}

QString OperatingDay::shortSerializeOpDays(const QString &serviceName) const
{
    CalendarRec serviceCal = this->calendarDb[serviceName];
    QString days;

    // Let's pull no punches and go full European with the date serialization (MON-SUN weeks)
    if (serviceCal.monday)
        days += "Mo";
    else
        days += "  ";
    if (serviceCal.tuesday)
        days += "Tu";
    else
        days += "  ";
    if (serviceCal.wednesday)
        days += "We";
    else
        days += "  ";
    if (serviceCal.thursday)
        days += "Th";
    else
        days += "  ";
    if (serviceCal.friday)
        days += "Fr";
    else
        days += "  ";
    if (serviceCal.saturday)
        days += "Sa";
    else
        days += "  ";
    if (serviceCal.sunday)
        days += "Su";
    else
        days += "  ";

    return days;
}

void OperatingDay::booleanOpDays(const QString &serviceName,
                                 bool &mon, bool &tue, bool &wed, bool &thu, bool &fri, bool &sat, bool &sun) const
{
    CalendarRec serviceCal = this->calendarDb[serviceName];

    // Fill each operating day in individually
    if (serviceCal.monday)
        mon = true;
    else
        mon = false;
    if (serviceCal.tuesday)
        tue = true;
    else
        tue = false;
    if (serviceCal.wednesday)
        wed = true;
    else
        wed = false;
    if (serviceCal.thursday)
        thu = true;
    else
        thu = false;
    if (serviceCal.friday)
        fri = true;
    else
        fri = false;
    if (serviceCal.saturday)
        sat = true;
    else
        sat = false;
    if (serviceCal.sunday)
        sun = true;
    else
        sun = false;
}

QString OperatingDay::serializeAddedServiceDates(const QString &serviceName) const
{
    QString calDates;

    if (this->calendarDateDb.contains(serviceName)) {
        const QVector<CalDateRec> &rec = this->calendarDateDb[serviceName];
        for (const CalDateRec &cdr : rec) {
            if (cdr.exception_type == 1)
                calDates += cdr.date.toString("ddMMMyyyy") + " ";
        }
    } else {
        calDates = "";
    }

    return calDates;
}

bool OperatingDay::serviceAddedOnOtherDates(const QString &serviceName) const
{
    qint32 nbAddedDates = 0;

    if (this->calendarDateDb.contains(serviceName)) {
        const QVector<CalDateRec> &rec = this->calendarDateDb[serviceName];
        for (const CalDateRec &cdr : rec)
            if (cdr.exception_type == 1)
                ++nbAddedDates;
    }

    return nbAddedDates != 0;
}

QString OperatingDay::serializeNoServiceDates(const QString &serviceName) const
{
    QString calDates;

    if (this->calendarDateDb.contains(serviceName)) {
        const QVector<CalDateRec> &rec = this->calendarDateDb[serviceName];
        for (const CalDateRec &cdr : rec) {
            if (cdr.exception_type == 2)
                calDates += cdr.date.toString("ddMMMyyyy") + " ";
        }
    } else {
        calDates = "";
    }

    return calDates;
}

bool OperatingDay::serviceRemovedOnDates(const QString &serviceName) const
{
    qint32 nbExemptedDates = 0;

    if (this->calendarDateDb.contains(serviceName)) {
        const QVector<CalDateRec> &rec = this->calendarDateDb[serviceName];
        for (const CalDateRec &cdr : rec)
            if (cdr.exception_type == 2)
                ++nbExemptedDates;
    }

    return nbExemptedDates != 0;
}

QDate OperatingDay::getServiceStartDate(const QString &serviceName) const
{
    if (this->calendarDb.contains(serviceName)) {
        return this->calendarDb[serviceName].start_date;
    }
    return QDate();
}

QDate OperatingDay::getServiceEndDate(const QString &serviceName) const
{
    if (this->calendarDb.contains(serviceName)) {
        return this->calendarDb[serviceName].end_date;
    }
    return QDate();
}

const QHash<QString, CalendarRec> &OperatingDay::getServiceDB() const
{
    return this->calendarDb;
}

bool OperatingDay::isNextActualDay(qint32 noonOffsetSeconds)
{
    return noonOffsetSeconds >= k12hInSec;
}

void OperatingDay::calendarCSVOrder(const QVector<QString> csvHeader,
                                    qint8 &servicePos,
                                    qint8 &monPos,
                                    qint8 &tuePos,
                                    qint8 &wedPos,
                                    qint8 &thuPos,
                                    qint8 &friPos,
                                    qint8 &satPos,
                                    qint8 &sunPos,
                                    qint8 &sDatePos,
                                    qint8 &eDatePos)
{
    // service_id,service_name,monday,tuesday,wednesday,thursday,friday,saturday,sunday,start_date,end_date
    servicePos = monPos = tuePos = wedPos = thuPos = friPos = satPos = sunPos = sDatePos = eDatePos = -1;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "service_id") {
            servicePos = position;
        } else if (item == "monday") {
            monPos = position;
        } else if (item == "tuesday") {
            tuePos = position;
        } else if (item == "wednesday") {
            wedPos = position;
        } else if (item == "thursday") {
            thuPos = position;
        } else if (item == "friday") {
            friPos = position;
        } else if (item == "saturday") {
            satPos = position;
        } else if (item == "sunday") {
            sunPos = position;
        } else if (item == "start_date") {
            sDatePos = position;
        } else if (item == "end_date") {
            eDatePos = position;
        }
        ++position;
    }
}

void OperatingDay::calendarDatesCSVOrder(const QVector<QString> csvHeader,
                                         qint8 &idPos, qint8 &datePos, qint8 &exceptionPos)
{
    // service_id,date,holiday_name,exception_type
    idPos = datePos = exceptionPos = -1;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "service_id") {
            idPos = position;
        } else if (item == "date") {
            datePos = position;
        } else if (item == "exception_type") {
            exceptionPos = position;
        }
        ++position;
    }
}

}  // Namespace GTFS
