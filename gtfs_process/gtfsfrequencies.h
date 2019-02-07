#ifndef GTFSFREQUENCIES_H
#define GTFSFREQUENCIES_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QPair>

namespace GTFS {

// We do not really need all the records so let's save some space
typedef struct {
    QString trip_id;
    qint32  start_time;  // in seconds relative to local noon of the operating day (can exceed 12-hours!)
    qint32  end_time;    // in seconds relative to local noon of the operating day (can exceed 12-hours!)
    qint32  headway_sec; // Repetition interval (in seconds) since the start_time for which to create new stop_times
} FreqRec;

class Frequencies : public QObject
{
    Q_OBJECT
public:
    explicit Frequencies(const QString dataRootPath, QObject *parent = nullptr);

    qint64 getFrequencyDBSize() const;

    const QVector<FreqRec> &getFrequenciesDB() const;

private:
    // Determine order of CSV table columns from agency.txt
    static void frequenciesCSVOrder(const QVector<QString> csvHeader,
                                    qint8 &tripIdPos,
                                    qint8 &startPos,
                                    qint8 &endPos,
                                    qint8 &headwayPos);

    // Route Database
    QVector<FreqRec> frequencyDb;
};

}  // Namespace GTFS

#endif // GTFSROUTE_H
