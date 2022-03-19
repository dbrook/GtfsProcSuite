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

#ifndef GTFSREQUESTPROCESSOR_H
#define GTFSREQUESTPROCESSOR_H

#include <QObject>
#include <QRunnable>

/*
 * GtfsRequestProcessor implements a QRunnable to be executed from a threadpool when a request arrives at the
 * GtfsProc_Server. Upon decoding a request sent into run(), it calls the appropriate member function to hook into the
 * needed GTFS data feed(s) via the GTFS::DataGateway or via abstraction layers. The response data is encoded in JSON.
 */
class GtfsRequestProcessor : public QObject , public QRunnable
{
    Q_OBJECT
public:
    GtfsRequestProcessor(QString userRequest);

signals:
    // JSON-style response, terminated with a '\n' (required for clients to detect the end of output over a socket)
    void Result(QString userResponse);

protected:
    void run();

private:
    /*
     * The "NEX" request handler - Next Trips to Serve a Stop
     *
     * If a stop_id is requested that is also a parent_station, then all the parent_station's children will be parsed
     *
     * TODO: A route-filtering mechanism would be a good idea for this, since parent stations can be VERY active
     *
     * TODO: Providing a QList or something of the stop_ids should also be supported since many agencies don't file
     *       with parent_station, rendering several stop_ids which are basically the same physical stop. Basically it
     *       seems like depending on the agency size and technical expertise there is wildly different filing standards
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     *
     * NOTE: This process will look at AT MOST 3 operating days to get the full scope of the possible upcoming stops:
     *   - Yesterday -- specifically for any trips that began / still run after midnight that could show up, i.e. a
     *                  trip which began at 23:55 and runs until 25:10 (using time-since-midnight notation), but we ask
     *                  about a the next trips as of 00:35 where we are technically in the following day
     *   - Today     -- Obviously we want the trips that would pertain to this operating day, regardless of the time
     *   - Tomorrow  -- In case we ask for a future time-range which bleeds into the next operating day
     *
     * As a consequence to this, you will only ever realistically be able to ask for trips until the end of the next
     * operating day, thus the effective logical maximum future time is 1440 minutes or 24 hours, however it can be
     * longer depending on the time of day you ask for future trips and how 'far into' the current operating day it is.
     *
     * If it's preferred (and with stops covered by a myriad of confusing routes it might be), you can instead request
     * to show at most X future stops within the previous + current + next operating day using maxTripsPerRoute != 0.
     */
    void nextTripsAtStop(QString      stopID,
                         qint32       futureMinutes,
                         qint32       maxTripsPerRoute,
                         bool         realTimeOnly,
                         bool         combinedFormat,
                         QJsonObject &resp);

    /*
     * Date decoder / helper
     *
     * Translates "Y" into a QDate of yesterday
     *            "T" into a QDate of today
     *            "ddmmmyyyy" into a QDate of QDate(dd, mm, yyyy)
     *            ... and probably more as it comes up
     *
     * It also returns the userReq less it's date prefix. Day/Date in userReq must be space-delimited from rest.
     */
    QDate determineServiceDay(const QString &userReq, QString &remUserQuery);

    /*
     * Request time decoder / helper
     *
     * For the NEX application, a time is specified before the stop, so extract it and make it an integer
     */
    qint32 determineMinuteRange(const QString &userReq, QString &remUserQuery);

    /*
     * Multiple Stop ID decoder. Several stop IDs can be queried at a time, separated with a "|" symbol for NEX & NCF
     */
    void listifyStopIDs(const QString &remUserQuery, QList<QString> &listStopIDs);

    // Data member
    QString request;
};

#endif // GTFSREQUESTPROCESSOR_H
