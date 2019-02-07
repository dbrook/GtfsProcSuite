#include "gtfsfrequencies.h"
#include "gtfsstoptimes.h"

#include "csvprocessor.h"

#include <QFileInfo>
#include <QDebug>

namespace GTFS {

Frequencies::Frequencies(const QString dataRootPath, QObject *parent)
{
    QVector<QVector<QString>> dataStore;

    if (QFileInfo(dataRootPath + "/feed_info.txt").exists()) {
        // Read the file information
        qDebug() << "Starting Frequencies Processing";
        CsvProcess((dataRootPath + "/frequencies.txt").toUtf8(), &dataStore);
        qint8 tripIdPos, startTimePos, endTimePos, headwayPos;
        frequenciesCSVOrder(dataStore.at(0), tripIdPos, startTimePos, endTimePos, headwayPos);
        qDebug() << "tripId = " << tripIdPos << "  start = " << startTimePos << "  end = " << endTimePos
                 << "  headway = " << headwayPos;
        for (int l = 1; l < dataStore.size(); ++l) {
            FreqRec frequency;
            // Let's just hope that things are correctly defined ... any mistakes are not presently caught!
            frequency.trip_id     = dataStore.at(l).at(tripIdPos);
            frequency.start_time  = GTFS::StopTimes::computeSecondsLocalNoonOffset(dataStore.at(l).at(startTimePos));
            frequency.end_time    = GTFS::StopTimes::computeSecondsLocalNoonOffset(dataStore.at(l).at(endTimePos));
            frequency.headway_sec = dataStore.at(l).at(headwayPos).toInt();
            this->frequencyDb.push_back(frequency);
        }
    }
}

qint64 Frequencies::getFrequencyDBSize() const
{
    return frequencyDb.size();
}

const QVector<FreqRec> &Frequencies::getFrequenciesDB() const
{
    return frequencyDb;
}

void Frequencies::frequenciesCSVOrder(const QVector<QString>  csvHeader,
                                      qint8                  &tripIdPos,
                                      qint8                  &startPos,
                                      qint8                  &endPos,
                                      qint8                  &headwayPos)
{
    // route_id,agency_id,route_short_name,route_long_name,route_desc,route_type,route_url,route_color,route_text_color
    tripIdPos = startPos = endPos = headwayPos = -1;
    qint8 position = 0;

    for (const QString &item : csvHeader) {
        if (item == "trip_id") {
            tripIdPos = position;
        } else if (item == "start_time") {
            startPos = position;
        } else if (item == "end_time") {
            endPos = position;
        } else if (item == "headway_secs") {
            headwayPos = position;
        }
        ++position;
    }
}


}  // Namespace GTFS
