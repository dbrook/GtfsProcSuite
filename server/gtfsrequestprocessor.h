#ifndef GTFSREQUESTPROCESSOR_H
#define GTFSREQUESTPROCESSOR_H

#include <QObject>
#include <QRunnable>

class GtfsRequestProcessor : public QObject , public QRunnable
{
    Q_OBJECT
public:
    GtfsRequestProcessor(QString userRequest);

signals:
    // TODO: This will probably be a JSON something or other
    void Result(QString userResponse);

protected:
    void run();

private:
    /*
     * The "SDS" request handler - System and Data Status
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     */
    void applicationStatus(QJsonObject &resp);

    /*
     * The "RTE" request handler - RouTEs belonging to agency/feed
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     */
    void availableRoutes(QJsonObject &resp);

    /*
     * The "TRI" request handler - TRIp information
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     */
    void tripStopsDisplay(QString tripID, QJsonObject &resp);

    /*
     * The "TSR" request handler - Trips Serving Route
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     */
    void tripsServingRoute(QString routeID, QDate onlyDate, QJsonObject &resp);

    /*
     * The "TSS" request handler - Trips Serving Route
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     */
    void tripsServingStop(QString stopID, QDate onlyDate, QJsonObject &resp);

    /*
     * The "STA" requestion handler - Station Details Display
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     */
    void stationDetailsDisplay(QString stopID, QJsonObject &resp);

    /*
     * The "SSR" request handler - Stops Served by Route
     *
     * Response format: JSON
     *
     * (TOOD: Intert details of the message contents)
     */
    void stopsServedByRoute(QString routeID, QJsonObject &resp);

    /*
     * The "NEX" request handler - Next Trips to Serve a Stop
     *
     * Response format: JSON
     *
     * (TODO: Insert details of message contents)
     */
    void nextTripsAtStop(QString stopID, qint32 futureMinutes, QJsonObject &resp);


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


    // Data member
    QString request;
};

#endif // GTFSREQUESTPROCESSOR_H
