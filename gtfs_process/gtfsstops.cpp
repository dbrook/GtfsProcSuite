#include "gtfsstops.h"
#include "csvprocessor.h"

#include <algorithm>

#include <QDebug>

namespace GTFS {

Stops::Stops(const QString dataRootPath, QObject *parent)
{
    QVector<QVector<QString>> dataStore;

    // Read feed information
    qDebug() << "Starting Stops Information Process";
    CsvProcess((dataRootPath + "/stops.txt").toUtf8(), &dataStore);
    qint8 stopIdPos, stopNamePos, stopDescPos, stopLatPos, stopLonPos, parentStationPos;
    stopsCSVOrder(dataStore.at(0), stopIdPos, stopDescPos, stopNamePos, stopLatPos, stopLonPos, parentStationPos);

    // Ingest the data and store it by stop_id
    for (int l = 1; l < dataStore.size(); ++l) {
        StopRec stop;
        stop.stop_name      = dataStore.at(l).at(stopNamePos);
        stop.stop_desc      = dataStore.at(l).at(stopDescPos);
        stop.stop_lat       = dataStore.at(l).at(stopLatPos).toFloat();
        stop.stop_lon       = dataStore.at(l).at(stopLonPos).toFloat();
        stop.parent_station = dataStore.at(l).at(parentStationPos);

        this->stopsDb[dataStore.at(l).at(stopIdPos)] = stop;

        if (stop.parent_station != "") {
            this->parentStopDb[stop.parent_station].append(dataStore.at(l).at(stopIdPos));
        }
    }
}

qint64 Stops::getStopsDBSize() const
{
    // Stop database size
    qint64 itemCount = this->stopsDb.size();

    // ... and the parent-station mapping supplementary database
    for (const QVector<QString> entry : this->parentStopDb) {
        itemCount += entry.size();
    }

    return itemCount;
}

const QMap<QString, StopRec> &Stops::getStopDB() const
{
    return this->stopsDb;
}

const QMap<QString, QVector<QString>> &Stops::getParentStationDB() const
{
    return this->parentStopDb;
}

void Stops::connectTripRoute(const QString &StopID,
                             const QString &TripID,
                             const QString &RouteID,
                             qint32         TripSequence,
                             qint32         sortTime)
{
    tripStopSeqInfo tssi;
    tssi.sortTime    = sortTime;
    tssi.tripID      = TripID;
    tssi.tripStopIndex = TripSequence;
    this->stopsDb[StopID].stopTripsRoutes[RouteID].push_back(tssi);
}

void Stops::sortStopTripTimes()
{
    for (const QString &stopID : this->stopsDb.keys()) {
        for (const QString &routeID : this->stopsDb[stopID].stopTripsRoutes.keys()) {
            std::sort(this->stopsDb[stopID].stopTripsRoutes[routeID].begin(),
                      this->stopsDb[stopID].stopTripsRoutes[routeID].end(),
                      Stops::compareStopTrips);
        }
    }
}

bool Stops::compareStopTrips(tripStopSeqInfo &tripStop1, tripStopSeqInfo &tripStop2)
{
    return tripStop1.sortTime < tripStop2.sortTime;
}

void Stops::stopsCSVOrder(const QVector<QString> csvHeader,
                          qint8 &stopIdPos,
                          qint8 &stopDescPos,
                          qint8 &stopNamePos,
                          qint8 &stopLatPos,
                          qint8 &stopLonPos,
                          qint8 &parentStationPos)
{
    stopIdPos = stopNamePos = stopDescPos = stopLatPos = stopLonPos = parentStationPos = 0;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "stop_id") {
            stopIdPos = position;
        } else if (item == "stop_name") {
            stopNamePos = position;
        } else if (item == "stop_desc") {
            stopDescPos = position;
        } else if (item == "stop_lat") {
            stopLatPos = position;
        } else if (item == "stop_lon") {
            stopLonPos = position;
        } else if (item == "parent_station") {
            parentStationPos = position;
        }

        ++position;
    }
}

} // Namespace GTFS
