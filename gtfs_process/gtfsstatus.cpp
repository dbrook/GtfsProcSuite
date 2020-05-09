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

#include "gtfsstatus.h"

#include "csvprocessor.h"

#include <QDebug>
#include <QString>
#include <QVector>
#include <QFileInfo>

namespace GTFS {

Status::Status(const QString dataRootPath,
               const QString &frozenDateTime,
               bool           use12hClock,
               quint32        nbTripsPerRouteNEX,
               bool           hideTermTrips,
               QObject       *parent)
    : QObject(parent),
      numberTripsPerRouteNEX(nbTripsPerRouteNEX),
      hideEndingTrips(hideTermTrips)
{
    // We should pass the start time from the main server start?
    this->serverStartTimeUTC = QDateTime::currentDateTimeUtc();
    this->recordsLoaded = 0;
    this->staticDataRevision = QDateTime();

    QVector<QVector<QString>> dataStore;

    // the feed_info.txt is [unfortunately] not required, so don't assume you have it
    if (QFileInfo(dataRootPath + "/feed_info.txt").exists()) {
        qDebug() << "Starting Feed Information Gathering ...";
        // Read in the feed information
        CsvProcess((dataRootPath + "/feed_info.txt").toUtf8(), &dataStore);
        qint8 pubPos, urlPos, lanPos, verPos, sDatePos, eDatePos;
        feedInfoCSVOrder(dataStore.at(0), pubPos, urlPos, lanPos, verPos, sDatePos, eDatePos);

        // Store the Record Information we care about
        this->publisher = (pubPos != -1) ? dataStore.at(1).at(pubPos) : "";
        this->url       = (urlPos != -1) ? dataStore.at(1).at(urlPos) : "";
        this->language  = (lanPos != -1) ? dataStore.at(1).at(lanPos).toUpper() : "";
        this->version   = (verPos != -1) ? dataStore.at(1).at(verPos) : "";

        // Save the start and end dates? Stored as text: YYYYMMDD
        this->startDate = (sDatePos != -1) ? QDate(dataStore.at(1).at(sDatePos).left  (4)   .toInt(),
                                                   dataStore.at(1).at(sDatePos).midRef(4, 2).toInt(),
                                                   dataStore.at(1).at(sDatePos).right (2)   .toInt())
                                           : QDate();
        this->endDate   = (eDatePos != -1) ? QDate(dataStore.at(1).at(eDatePos).left  (4)   .toInt(),
                                                   dataStore.at(1).at(eDatePos).midRef(4, 2).toInt(),
                                                   dataStore.at(1).at(eDatePos).right (2)   .toInt())
                                           : QDate();

        // Say we processed a record
        this->incrementRecordsLoaded(1);
    } else {
        // Fill the feed info with some horrible defaults that will undoubtedly lead to later confusion
        qDebug() << "Feed Information is not present, fill with defaults and NULL dates";
        this->publisher = "Not Provided";
        this->url       = "Not Provided";
        this->language  = "Not Provided";
        this->version   = "Not Provided";

        // Save the start and end dates? Stored as text: YYYYMMDD
        this->startDate = QDate();
        this->endDate   = QDate();
    }

    dataStore.clear();

    // Agency is always required.
    {
        qDebug() << "Starting Agency Gathering ...";
        // Now let's load the agencies
        CsvProcess((dataRootPath + "/agency.txt").toUtf8(), &dataStore);
        qint8 idPos, namePos, urlPos, tzPos, langPos, phonePos;           // They can put e-mail instead of phone!
        agencyCSVOrder(dataStore.at(0), idPos, namePos, urlPos, tzPos, langPos, phonePos);
        for (int l = 1; l < dataStore.size(); ++l) {
            AgencyRecord agency;
            agency.agency_id       = (idPos != -1)    ? dataStore.at(l).at(idPos)    : "";
            agency.agency_name     = (namePos != -1)  ? dataStore.at(l).at(namePos)  : "";
            agency.agency_url      = (urlPos != -1)   ? dataStore.at(l).at(urlPos)   : "";
            agency.agency_timezone = (tzPos != -1)    ? dataStore.at(l).at(tzPos)    : "";
            agency.agency_lang     = (langPos != -1)  ? dataStore.at(l).at(langPos)  : "";
            agency.agency_phone    = (phonePos != -1) ? dataStore.at(l).at(phonePos) : "";
            this->Agencies.push_back(agency);
            this->incrementRecordsLoaded(1);
        }

        // TODO: I actually have no idea, let's just take the first timezone we find and reference it for proper stamps
        this->serverFeedTZ = QTimeZone(this->Agencies[0].agency_timezone.toUtf8());

        // Store the modified date/time of this file. It will be considered as the modified date/time for the entire
        // static dataset (which will be sent to NEX/NCF outputs so front-end clients can compare to their cached
        // date/time to see if they can re-poll for new route information).
        this->staticDataRevision = QFileInfo(dataRootPath + "/agency.txt").lastModified();
    }

    // Decode any date string from input in case the time of every transaction should always be the same. This might
    // be useful for debugging static data or a realtime feed that is giving any particular issue
    frozenAgencyTime = QDateTime();
    if (!frozenDateTime.isNull()) {
        QStringList brokenDateParams = frozenDateTime.split(',');
        if (brokenDateParams.size() != 6) {
            qDebug() << "The requested date has improperly-formatted information: " << frozenDateTime;
            return;
        }
        QDate frozenDate(brokenDateParams.at(0).toInt(),
                         brokenDateParams.at(1).toInt(),
                         brokenDateParams.at(2).toInt());
        QTime frozenTime(brokenDateParams.at(3).toInt(),
                         brokenDateParams.at(4).toInt(),
                         brokenDateParams.at(5).toInt());
        frozenAgencyTime = QDateTime(frozenDate, frozenTime, this->serverFeedTZ);
        qDebug() << endl << "TESTING/DEBUGGING/ANALYSIS MODE: All transaction will be processed as if it is "
                 << frozenAgencyTime << endl;
    }

    // Show all times with AM/PM indicator instead of standard 24-hour clock
    use12h = use12hClock;
}

qint64 Status::getRecordsLoaded() const
{
    return recordsLoaded;
}

QDateTime Status::getServerStartTimeUTC() const
{
    return serverStartTimeUTC;
}

QString Status::getPublisher() const
{
    return publisher;
}

QString Status::getUrl() const
{
    return url;
}

QString Status::getLanguage() const
{
    return language;
}

QDate Status::getStartDate() const
{
    return startDate;
}

QDate Status::getEndDate() const
{
    return endDate;
}

QString Status::getVersion() const
{
    return version;
}

const QVector<AgencyRecord> &Status::getAgencies() const
{
    return this->Agencies;
}

const QTimeZone &Status::getAgencyTZ() const
{
    return this->serverFeedTZ;
}

const QDateTime &Status::getOverrideDateTime() const
{
    return this->frozenAgencyTime;
}

bool Status::format12h() const
{
    return this->use12h;
}

quint32 Status::getNbTripsPerRoute() const
{
    return numberTripsPerRouteNEX;
}

bool Status::hideTerminatingTripsForNEXNCF() const
{
    return hideEndingTrips;
}

QDateTime Status::getStaticDatasetModifiedTime() const
{
    return staticDataRevision;
}

void Status::incrementRecordsLoaded(const qint64 &value)
{
    recordsLoaded += value;
}

QDateTime Status::getLoadFinishTimeUTC() const
{
    return loadFinishTimeUTC;
}

void Status::setLoadFinishTimeUTC()
{
    loadFinishTimeUTC = QDateTime::currentDateTimeUtc();
}

void Status::feedInfoCSVOrder(const QVector<QString> csvHeader,
                              qint8 &publisherPos,
                              qint8 &urlPos,
                              qint8 &languagePos,
                              qint8 &versionPos,
                              qint8 &startDatePos,
                              qint8 &endDatePos)
{
    // Start everything as -1 for error tracking purposes
    publisherPos = urlPos = languagePos = versionPos = startDatePos = endDatePos = -1;
    qint8 position = 0;

    // Possible Layou: feed_publisher_name,feed_publisher_url,feed_lang,feed_start_date,feed_end_date,feed_version
    for (const QString &item : csvHeader) {
        if (item == "feed_publisher_name") {
            publisherPos = position;
        } else if (item == "feed_publisher_url") {
            urlPos = position;
        } else if (item == "feed_lang") {
            languagePos = position;
        } else if (item == "feed_version") {
            versionPos = position;
        } else if (item == "feed_start_date") {
            startDatePos = position;
        } else if (item == "feed_end_date") {
            endDatePos = position;
        }
        ++position;
    }
}

void Status::agencyCSVOrder(const QVector<QString> csvHeader,
                            qint8 &idPos, qint8 &namePos, qint8 &urlPos, qint8 &tzPos, qint8 &langPos, qint8 &phonePos)
{
    // Start everything as -1 for error tracking purposes
    idPos = namePos = urlPos = tzPos = langPos = phonePos = -1;
    qint8 position = 0;

    // Possible Layou: agency_id,agency_name,agency_url,agency_timezone,agency_lang,agency_phone
    for (const QString &item : csvHeader) {
        if (item == "agency_id") {
            idPos = position;
        } else if (item == "agency_name") {
            namePos = position;
        } else if (item == "agency_url") {
            urlPos = position;
        } else if (item == "agency_timezone") {
            tzPos = position;
        } else if (item == "agency_lang") {
            langPos = position;
        } else if (item == "agency_phone" || item == "agency_email") {
            phonePos = position;
        }
        ++position;
    }
}

}
