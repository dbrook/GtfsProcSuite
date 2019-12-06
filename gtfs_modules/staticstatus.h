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

    /*
     * Fills a JSON (resp) with server status information, formatted in the schema below
     * {
     *   message_type     :string: Standard Content : "SDS" indicates this is a status message
     *   error            :int   : Standard Content : 0 indicates success (no other value possible)
     *   message_time     :string: Standard Content : Message creation time (format: dd-MMM-yyyy hh:mm:ss t)
     *   proc_time_ms     :int   : Standard Content : Milliseconds it took to populate the response after instantiation
     *
     *   application      :string: Process version information
     *   records          :int   : Number of records loaded in the GTFS static feed
     *   appuptime_ms     :int   : Milliseconds that the backend service has been running
     *   dataloadtime_ms  :int   : Milliseconds elapsed (wall-clock-time) during GTFS static data load
     *   threadpool_count :int   : Number of threads present in the thread-pool to handle incoming requests
     *   processed_reqs   :int   : Number of requests processed during backend service uptime
     *
     *   feed_publisher   :string: GTFS static feed publisher
     *   feed_url         :string: GTFS static feed publisher's URL
     *   feed_lang        :string: Language in which the static feed is (principally) encoded
     *   feed_valid_start :string: GTFS static feed validity range (beginning)
     *   feed_valid_end   :string: GTFS static feed validity range (ending)
     *   feed_version     :string: Free text version string provided by the GTFS static feed publisher
     *   agencies:        :ARRAY : one for each agency appearing in the feed (see contents)
     *   [
     *     id             :string: Numeric ID of the agency
     *     name           :string: Name of the agency
     *     url            :string: URL for agency website
     *     tz             :string: Timezone in which the agency (principally) operates
     *     lang           :string: Language in which the agency provides feed information
     *     phone          :string: Phone number to reach the agency
     *   ]
     * }
     */
    virtual void fillResponseData(QJsonObject &resp);

private:
    const GTFS::Status *_stat;
    QDateTime           _currUTC;
    QDateTime           _currAgency;
};

}  // Namespace GTFS

#endif // STATICSTATUS_H
