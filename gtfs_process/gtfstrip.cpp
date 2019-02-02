#include "gtfstrip.h"
#include "csvprocessor.h"

#include <QDebug>

namespace GTFS {

Trips::Trips(const QString dataRootPath, QObject *parent) : QObject(parent)
{
    QVector<QVector<QString>> dataStore;

    // Read the feed information
    qDebug() << "Starting Trip Process";
    CsvProcess((dataRootPath + "/trips.txt").toUtf8(), &dataStore);
    qint8 routeIdPos, tripIdPos, serviceIdPos, headsignPos;
    tripsCSVOrder(dataStore.at(0), routeIdPos, tripIdPos, serviceIdPos, headsignPos);

    for (int l = 1; l < dataStore.size(); ++l) {
        TripRec trip;
        trip.trip_id       = dataStore.at(l).at(tripIdPos);
        trip.route_id      = dataStore.at(l).at(routeIdPos);
        trip.service_id    = dataStore.at(l).at(serviceIdPos);
        trip.trip_headsign = (headsignPos != -1) ? dataStore.at(l).at(headsignPos) : "";

        this->tripDb[dataStore.at(l).at(tripIdPos)] = trip;
    }
}

qint64 Trips::getTripsDBSize() const
{
    return this->tripDb.size();
}

const QMap<QString, TripRec> &Trips::getTripsDB() const
{
    return this->tripDb;
}

void Trips::tripsCSVOrder(const QVector<QString> csvHeader,
                          qint8 &routeIdPos, qint8 &tripIdPos, qint8 &serviceIdPos, qint8 &headsignPos)
{
    routeIdPos = tripIdPos = serviceIdPos = headsignPos = -1;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "route_id") {
            routeIdPos = position;
        } else if (item == "trip_id") {
            tripIdPos = position;
        } else if (item == "service_id") {
            serviceIdPos = position;
        } else if (item == "trip_headsign") {
            headsignPos = position;
        }
        ++position;
    }
}

} // Namespace GTFS
