/*
 * GtfsProc_Server
 * Copyright (C) 2018-2023, Daniel Brook
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

#include "structureddisplay.h"

#include <QTextStream>
#include <QCoreApplication>

#include <unistd.h>
#include <term.h>

Display::Display()
{
    this->reInitTerm();
}

void Display::reInitTerm()
{
    // Blow away whatever is present
    // From http://www.cplusplus.com/articles/4z18T05o
    int result;
    setupterm(nullptr, STDOUT_FILENO, &result);
    if (result <= 0) {
        return;
    }

    // Determine a buffer for large terminals using NCURSES voodoo
    this->buffer = "";

    this->cols = columns;    // From NCURSES
    this->rows = lines;      // From NCURSES, useful for determining if text will go off the end of the terminal

    // Banner is 55 columns wide (1/2 of banner is 27), we can try to center it
    int leftbuf = this->cols / 2 - 22;

    // Stupid evil hack ... TODO: find a more glamorous way to do this... (like a QStringStream or something?)
    for (int spacer = 0; spacer < leftbuf; ++spacer) {
        this->buffer += " ";
    }
}

void Display::clearTerm()
{
    putp(tigetstr("clear"));
}

void Display::displayWelcome()
{
    this->clearTerm();

    //
    // Actual display logic
    //
    QTextStream screen(stdout);
    screen << buffer << "GTFS Interactive Data Console -- Version: "
                     << QCoreApplication::applicationVersion() << Qt::endl
           << Qt::endl
           << "[ System Information ]" << Qt::endl
           << "SDS: Backend system and data load status" << Qt::endl
           << "RDS: GTFS Real-Time data retrieval status" << Qt::endl
           << "RTE: Routes available from the data set" << Qt::endl
           << "SSR: List of all stops served by a single route" << Qt::endl
           << "SNT: List all stops that have no trips (diagnostic)" << Qt::endl
           << Qt::endl
           << "[ Full Schedule Lookup ]" << Qt::endl
           << "STA: Stop information lookup by stop_id" << Qt::endl
           << "TSR: List of trips serving a route_id" << Qt::endl
           << "TSS: List of trips serving a stop_id" << Qt::endl
           << "TRI: List all the stops served by a trip_id" << Qt::endl
           << "RTS/RTF/RTT: List the real-time data of an active trip_id or update" << Qt::endl
           << Qt::endl
           << "[ Data Lookup for Specific Date ]" << Qt::endl
           << "TRD: List of trips serving a route_id on a date" << Qt::endl
           << "TSD: List of trips serving a stop_id on a date" << Qt::endl
           << "NEX/NCF: List upcoming trips serving stop_id within a number of minutes" << Qt::endl
           << Qt::endl
           << "[ Service Connecting Stops ]" << Qt::endl
           << "SBS: Service between 2 stops, scheduled" << Qt::endl
           << "EES: End-to-end connecting services with times" << Qt::endl
           << "EER: End-to-end connecting services (real-time data only)" << Qt::endl
           << Qt::endl
           << "Reinitialize the display with 'HOM', quit using 'BYE'"
           << Qt::endl << Qt::endl;
}

void Display::showServer(QString hostname, int port)
{
    QTextStream screen(stdout);
    screen << "Connected to: " << hostname << " : " << port << Qt::endl;
    screen.flush();
}

void Display::showError(QString errorText)
{
    QTextStream screen(stdout);
    screen << "Error: " << errorText << Qt::endl;
    screen.flush();
}

int Display::getCols() const
{
    return cols;
}

int Display::getRows() const
{
    return rows;
}
