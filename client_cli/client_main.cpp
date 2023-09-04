/*
 * GtfsProc_Server
 * Copyright (C) 2018-2021, Daniel Brook
 *
 * This file is part of GtfsProc.
 *
 * GtfsProc is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * GtfsProc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with GtfsProc.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 * See included LICENSE.txt file for full license.
 */

#include <QCoreApplication>
#include <QCommandLineParser>

#include "clientgtfs.h"

#include <QString>
#include <QDebug>

/*
 * GtfsProc_Client's main() starts a text-mode display session to interact with a running GtfsProc_Server.
 */
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCoreApplication::setApplicationName("GtfsProc_Client");
    QCoreApplication::setApplicationVersion("0.2");

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

    if (!client.startConnection(serverHost, serverPort.toUShort(), 5000)) {
        return 1;
    }

    if (args.size() == 2) {
        // Begin processing user commands if interactive mode specified
        client.repl();
    } else if (args.size() == 3) {
        // One-off transaction (not interactive), use for regression testing and whatever else doesn't require a REPL
        QString printMode = args.at(2);
        bool prettyPrint = false;
        if (printMode == "P") {
            prettyPrint = true;
        }
        client.once(prettyPrint);
    }

    //
    return 0;
}
