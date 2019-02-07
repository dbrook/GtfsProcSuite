#ifndef GTFSSTOPTIMES_H
#define GTFSSTOPTIMES_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QVector>

namespace GTFS {

typedef struct {
//    QString trip_id;          Trip ID is the primary key
    qint32  stop_sequence;
    QString stop_id;
    qint32  arrival_time;     // in seconds relative to local noon of the operating day (can exceed 12-hours!)
    qint32  departure_time;   // in seconds relative to local noon of the operating day (can exceed 12-hours!)
    /*
     * There are several more feeds which are not mandatory (nor are interesting here), so we will skip them for now
     */
    // Except for drop-off and pick-up types ... that is actually an interesting concept!
    // These are stored as-is from the data: 0 = normal, 1 = not allowed, 2 = call agency first, 3 = arrange with driver
    // so it is up to the front-end to decide how/if to show this information (as this software is intended to be used
    // as more of a wait / countdown for service, the pickup types might be the most interesting
    qint8   drop_off_type;
    qint8   pickup_type;

    // And another one just for fun: the each individual stop in a trip (i.e. one of these stop-time records) can have
    // a unique / standalone headsign just for it (independent of the overall journey)
    QString stop_headsign;
} StopTimeRec;


class StopTimes : public QObject
{
    Q_OBJECT
public:
    explicit StopTimes(const QString dataRootPath, QObject *parent = nullptr);

    qint64 getStopTimesDBSize() const;

    const QMap<QString, QVector<StopTimeRec>> &getStopTimesDB() const;

    // Sorter
    static bool compareByStopSequence(const StopTimeRec &a, const StopTimeRec &b);

    // Notion of local noon (for offset calculations)
    static const qint32 s_localNoonSec;

private:
    static void stopTimesCSVOrder(const QVector<QString> csvHeader,
                                  qint8 &tripIdPos,
                                  qint8 &stopSeqPos,
                                  qint8 &stopIdPos,
                                  qint8 &arrTimePos,
                                  qint8 &depTimePos,
                                  qint8 &dropOffPos,
                                  qint8 &pickupPos,
                                  qint8 &stopHeadsignPos);

    static qint32 computeSecondsLocalNoonOffset(const QString &hhmmssTime);  // Send time as (h)h:mm:ss

    bool operator <(const StopTimeRec &strec) const;


    // Stop Times Database
    // This one is tricky, we have unique trips, but several entries for them, so we must be able to address
    // them with this hybrid "map of vectors" thing.
    QMap<QString, QVector<StopTimeRec>> stopTimeDb;
};

} // Namespace GTFS

#endif // GTFSSTOPTIMES_H
