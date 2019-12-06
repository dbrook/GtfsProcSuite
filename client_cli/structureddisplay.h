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

#ifndef DISPLAY_H
#define DISPLAY_H

#include <QObject>

/*
 * Display provides an inelegant wrapper around low-level NCURSES routines to determine the display terminal's width
 */
class Display
{
public:
    Display();

    // (Re)initialize the terminal width / height information
    void reInitTerm();

    // Clear the terminal
    void clearTerm();

    // Show a welcoming screen all nicely centered
    void displayWelcome();

    // Display server details
    void showServer(QString hostname, int port);

    // Print an error condition
    void showError(QString errorText);

    int getCols() const;

    int getRows() const;

private:
    QString buffer;

    // Size of the terminal
    int cols;
    int rows;
};

#endif // DISPLAY_H
