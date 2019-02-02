#ifndef GTFSTRIP_H
#define GTFSTRIP_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>

namespace GTFS {

typedef struct {
    QString trip_id;        // Primary data key, needed by StopTimes
    QString route_id;       // The route on which this trip operates
    QString service_id;     // Needed to see which day(s) trip is active
    QString trip_headsign;  // Note this is optional
    /*
     * There are several additional fields here but we are ignoring them for the time being
     */
} TripRec;

class Trips : public QObject
{
    Q_OBJECT
public:
    explicit Trips(const QString dataRootPath, QObject *parent = nullptr);

    qint64 getTripsDBSize() const;

    const QMap<QString, TripRec> &getTripsDB() const;

private:
    // Determine the order of the 'interesting' fields within the trip database
    static void tripsCSVOrder(const QVector<QString> csvHeader,
                              qint8 &routeIdPos,
                              qint8 &tripIdPos,
                              qint8 &serviceIdPos,
                              qint8 &headsignPos);

    // Trip Database
    QMap<QString, TripRec> tripDb;
};

} // Namespace GTFS

#endif // GTFSTRIP_H
