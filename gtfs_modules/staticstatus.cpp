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

#include "staticstatus.h"

#include "datagateway.h"

#include <QJsonArray>
#include <QThreadPool>
#include <QCoreApplication>

#include <QDebug>

namespace GTFS {

StaticStatus::StaticStatus()
{
    _stat       = GTFS::DataGateway::inst().getStatus();
    _currUTC    = QDateTime::currentDateTimeUtc();
    _currAgency = _currUTC.toTimeZone(_stat->getAgencyTZ());

    GTFS::DataGateway::inst().incrementHandledRequests();
}

const QDateTime StaticStatus::getAgencyTime()
{
    return _currAgency;
}

const QDateTime StaticStatus::getUTCTime()
{
    return _currUTC;
}

const Status *StaticStatus::getStatus()
{
    return _stat;
}

void StaticStatus::fillProtocolFields(const QString moduleID, qint64 errorID, QJsonObject &resp)
{
    resp["message_type"] = moduleID;
    resp["error"]        = errorID;
    resp["message_time"] = _currAgency.toString("dd-MMM-yyyy hh:mm:ss t");
    resp["proc_time_ms"] = _currUTC.msecsTo(QDateTime::currentDateTimeUtc());
}

void StaticStatus::fillResponseData(QJsonObject &resp)
{
    // Message-Specific Fields
    QString appName = QCoreApplication::applicationName();
    QString appVers = QCoreApplication::applicationVersion();

    resp["application"]      = appName.remove("\"") + " version " + appVers.remove("\"");
    resp["records"]          = _stat->getRecordsLoaded();
    resp["appuptime_ms"]     = _stat->getServerStartTimeUTC().msecsTo(_currUTC);
    resp["dataloadtime_ms"]  = _stat->getServerStartTimeUTC().msecsTo(_stat->getLoadFinishTimeUTC());
    resp["threadpool_count"] = QThreadPool::globalInstance()->maxThreadCount();
    resp["processed_reqs"]   = GTFS::DataGateway::inst().getHandledRequests();
    resp["feed_publisher"]   = _stat->getPublisher();
    resp["feed_url"]         = _stat->getUrl();
    resp["feed_lang"]        = _stat->getLanguage();
    resp["feed_valid_start"] = (!_stat->getStartDate().isNull()) ? _stat->getStartDate().toString("dd-MMM-yyyy")
                                                                 : "__-___-____";
    resp["feed_valid_end"]   = (!_stat->getEndDate().isNull())   ? _stat->getEndDate().toString("dd-MMM-yyyy")
                                                                 : "__-___-____";
    resp["feed_version"]     = _stat->getVersion();

    QJsonArray agencyArray;
    const QVector<GTFS::AgencyRecord> agencyVec = _stat->getAgencies();
    for (const GTFS::AgencyRecord &agency : agencyVec) {
        QJsonObject singleAgencyJSON;
        singleAgencyJSON["id"]    = agency.agency_id;
        singleAgencyJSON["name"]  = agency.agency_name;
        singleAgencyJSON["url"]   = agency.agency_url;
        singleAgencyJSON["tz"]    = agency.agency_timezone;
        singleAgencyJSON["lang"]  = agency.agency_lang;
        singleAgencyJSON["phone"] = agency.agency_phone;
        agencyArray.push_back(singleAgencyJSON);
    }
    resp["agencies"] = agencyArray;

    // Required GtfsProc protocol fields
    fillProtocolFields("SDS", 0, resp);
}

}  // namespace GTFS
