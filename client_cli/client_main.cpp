#include <QCoreApplication>
#include <QCommandLineParser>

#include "clientgtfs.h"

#include <QString>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCoreApplication::setApplicationName("GtfsProc_Client");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("GtfsProc Debugging Console and Data Access Client");
    parser.addVersionOption();
    parser.addPositionalArgument("serverHost", QCoreApplication::translate("main", "Server to connect with"));
    parser.process(a);

    // Server to connect with and port
    const QStringList args = parser.positionalArguments();
    QString serverHost = args.at(0);
    QString serverPort = args.at(1);

    // Print fancy ASCII art welcome message
    // send terminal characteristics so we can do some alignments
    ClientGtfs client;

    if (!client.startConnection(serverHost, serverPort.toInt(), 5000)) {
        return 1;
    }

    // Begin processing user commands
    client.repl();

    //
    return 0;
}
