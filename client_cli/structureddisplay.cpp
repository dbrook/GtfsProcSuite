/*
 * GtfsProc_Client
 * Copyright (C) 2018-2019, Daniel Brook
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
    setupterm(NULL, STDOUT_FILENO, &result);
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
                     << QCoreApplication::applicationVersion() << endl
           << endl
           << "[ System Information ]" << endl
           << "SDS: Backend system and data load status" << endl
           << "RDS: GTFS Real-Time data retrieval status" << endl
           << "RTE: Routes available from the data set" << endl
           << "SSR: List of all stops served by a single route" << endl
           << "SNT: List all stops that have no trips (diagnostic)" << endl
           << endl
           << "[ Full Schedule Lookup ]" << endl
           << "STA: Stop information lookup by stop_id" << endl
           << "TSR: List of trips serving a route_id" << endl
           << "TSS: List of trips serving a stop_id" << endl
           << "TRI: List all the stops served by a trip_id" << endl
           << "RTR: List the real-time data of an active trip_id" << endl
           << endl
           << "[ Data Lookup for Specific Date ]" << endl
           << "TRD: List of trips serving a route_id on a date" << endl
           << "TSD: List of trips serving a stop_id on a date" << endl
           << "NEX: List upcoming trips serving a stop_id within a number of minutes" << endl
           << endl
           << "Reinitialize the display with 'HOM', quit using 'BYE'"
           << endl << endl;
}

void Display::showServer(QString hostname, int port)
{
    QTextStream screen(stdout);
    screen << "Connected to: " << hostname << " : " << port << endl;
    screen.flush();
}

void Display::showError(QString errorText)
{
    QTextStream screen(stdout);
    screen << "Error: " << errorText << endl;
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
