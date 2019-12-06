/*
 * GtfsProc_Server
 * Copyright (C) 2018-2020, Daniel Brook
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

#ifndef CLIENTGTFS_H
#define CLIENTGTFS_H

#include "structureddisplay.h"

#include <QObject>
#include <QTcpSocket>

/*
 * ClientGTFS initializes a socket connection with the host/port specified, assuming that a GtfsProc_Server is present
 */
class ClientGtfs : public QObject
{
    Q_OBJECT
public:
    // Initializes termio / clears screen / prints welcome
    explicit ClientGtfs(QObject *parent = nullptr);

    // Connects to the server at 'hostname' on port 'port'
    bool startConnection(QString hostname, quint16 port, int userTimeout);

    // Prompts for user input one (also therefore will take injection) for scripting / front-end interaction.
    // Forwards the server the user query and returns the response in JSON to STDOUT
    void once();

    // Awaits user input and sends each query to the server - interactive mode for playing browsing in command line
    void repl();

private:
    QString dropoffToChar(qint8 svcDropOff);
    QString pickupToChar(qint8 svcPickUp);

    Display disp;
    QTcpSocket commSocket;
};

#endif // CLIENTGTFS_H
