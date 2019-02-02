#ifndef GTFSSTATUS_H
#define GTFSSTATUS_H

#include <QObject>
#include <QDateTime>
#include <QTimeZone>
#include <QVector>

namespace GTFS {

// Represents a single record from the agencies.txt GTFS file
typedef struct {
    QString agency_id;
    QString agency_name;
    QString agency_url;
    QString agency_timezone;
    QString agency_lang;
    QString agency_phone;
} AgencyRecord;

class Status : public QObject
{
    Q_OBJECT
public:
    explicit Status(const QString dataRootPath, QObject *parent = nullptr);

    qint64    getRecordsLoaded() const;

    QDateTime getServerStartTimeUTC() const;

    void      incrementRecordsLoaded(const qint64 &value);

    QDateTime getLoadFinishTimeUTC() const;
    void      setLoadFinishTimeUTC();

    // Determine order of CSV table columns from feed_info.txt
    static void feedInfoCSVOrder(const QVector<QString> csvHeader,
                                 qint8 &publisherPos,
                                 qint8 &urlPos,
                                 qint8 &languagePos,
                                 qint8 &versionPos,
                                 qint8 &startDatePos,
                                 qint8 &endDatePos);

    // Data from feed_info.txt
    QString   getPublisher() const;
    QString   getUrl() const;
    QString   getLanguage() const;
    QDate     getStartDate() const;
    QDate     getEndDate() const;
    QString   getVersion() const;

    // Determine order of CSV table columns from agency.txt
    static void agencyCSVOrder(const QVector<QString> csvHeader,
                               qint8 &idPos,
                               qint8 &namePos,
                               qint8 &urlPos,
                               qint8 &tzPos,
                               qint8 &langPos,
                               qint8 &phonePos);

    // Retrieve agency information
    const QVector<AgencyRecord> &getAgencies() const;

    // What the hell time is it where our feed is based?
    const QTimeZone &getAgencyTZ() const;

private:
    qint64     recordsLoaded;        // Number of lines loaded from the GTFS files
    QDateTime  serverStartTimeUTC;   // Start time of the processor
    QDateTime  loadFinishTimeUTC;    // Time when the whole database finished loading

    // Data straight from "feed_info.txt"
    QString    publisher;
    QString    url;
    QString    language;
    QDate      startDate;
    QDate      endDate;
    QString    version;

    // Agency Information
    QVector<AgencyRecord> Agencies;

    QTimeZone  serverFeedTZ;
};

}  // Namespace GTFS

#endif // GTFSSTATUS_H
