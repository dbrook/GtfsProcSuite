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

#ifndef STATICSTATUS_H
#define STATICSTATUS_H

#include "gtfsstatus.h"

#include <QObject>
#include <QJsonObject>

namespace GTFS {

/*
 * GTFS::StaticStatus connects to GTFS information via the data gateway interface and populates a JSON structure with
 * information about the server backend process.
 */
class StaticStatus : public QObject
{
    Q_OBJECT
public:
    explicit StaticStatus();

    /****************************************** Module-Independent Functions ******************************************/
    /* Retrieve UTC and local TZ date/time */
    const QDateTime     getAgencyTime();
    const QDateTime     getUTCTime();

    /* Retrieve the status object instance */
    const GTFS::Status *getStatus();

    /* Fill GtfsProc's protocol-required JSON items */
    void fillProtocolFields(const QString moduleID, qint64 errorID, QJsonObject &resp);

    /******************************************* Module-Specific Functions ********************************************/

    /* See GtfsProc_Documentation.html for JSON response format */
    virtual void fillResponseData(QJsonObject &resp);

private:
    const GTFS::Status *_stat;
    QDateTime           _currUTC;
    QDateTime           _currAgency;
};

}  // Namespace GTFS

#endif // STATICSTATUS_H
