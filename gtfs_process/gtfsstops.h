#ifndef GTFSSTOPS_H
#define GTFSSTOPS_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QVector>
#include <QPair>


namespace GTFS {

typedef struct {
    QString tripID;
    qint32  tripStopSeq;
    qint32  sortTime;
} tripStopSeqInfo;

typedef struct {
    // Data from File
    QString stop_name;
    QString stop_desc;
    float   stop_lat;
    float   stop_lon;
    QString parent_station;
    /*
     * There are several more feeds which are not mandatory (nor are interesting here), so we will skip them for now
     */

    // Computed Data (from other fields/links)
    // String(routeID)->(Vector(Pair(tripID,tripIDStopSequence)))
    QMap<QString, QVector<tripStopSeqInfo>> stopTripsRoutes;
} StopRec;

class Stops : public QObject
{
    Q_OBJECT
public:
    explicit Stops(const QString dataRootPath, QObject *parent = nullptr);

    qint64 getStopsDBSize() const;

    // Database retrieval function
    const QMap<QString, StopRec> &getStopDB() const;
    const QMap<QString, QVector<QString>> &getParentStationDB() const;

    // Association Builder (to link all the trips that service each stop for quick lookups)
    // Note the notion of "sortTime" = stop's departure time > stop's arrival time > trip's first departure time
    // THE sortTime IS ONLY FOR SORTING PURPOSES AND SHOULD NOT BE DISPLAYED IN OUTPUT
    void connectTripRoute(const QString &StopID,
                          const QString &TripID,
                          const QString &RouteID,
                          qint32 TripSequence,
                          qint32 sortTime);

    // Sorter for the stop-trips' times for easier reading
    void sortStopTripTimes();
    static bool compareStopTrips(tripStopSeqInfo &tripStop1, tripStopSeqInfo &tripStop2);

private:
    static void stopsCSVOrder(const QVector<QString> csvHeader,
                              qint8 &stopIdPos,
                              qint8 &stopDescPos,
                              qint8 &stopNamePos,
                              qint8 &stopLatPos,
                              qint8 &stopLonPos,
                              qint8 &parentStationPos);

    // Stop database
    // The addressing element of the stop is the stop_id (primary key)
    QMap<QString, StopRec>          stopsDb;
    QMap<QString, QVector<QString>> parentStopDb;
};

} // Namespace GTFS

#endif // GTFSSTOPS_H
