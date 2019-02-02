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

    // Banner is 68 columns wide (1/2 of banner is 34), we must try to center it
    int leftbuf = this->cols / 2 - 34;

    // Stupid evil hack ... TODO: find a more glamorous way to do this...
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
    screen << "\n";
    screen << buffer << "      GGGGGG     TTTTTTTTTT  FFFFFFFFFF   SSSSSSS     General" << endl;
    screen << buffer << "    GG              TT      FF          SS      SS" << endl;
    screen << buffer << "   GG              TT      FF          SS             Transit" << endl;
    screen << buffer << "  GG    GGGG      TT      FFFFFFFF      SSSSSS" << endl;
    screen << buffer << " GG      GG      TT      FF                  SS       Feed" << endl;
    screen << buffer << "GG      GG      TT      FF          SS      SS" << endl;
    screen << buffer << " GGGGGGG       TT      FF            SSSSSSS          Specification" << endl << endl;
    screen << buffer << "   Data Browser  /  Debugging Console  v" << QCoreApplication::applicationVersion() << endl;
    screen << endl;
    screen << buffer << "Available Applications:  SDS / System and Data Status" << endl;
    screen << buffer << "                         RTE / List all routes in database" << endl;
    screen << buffer << "                         TRI / Individual trip schedule" << endl;
    screen << buffer << "                         NEX / Next trips serving a stations" << endl;
    screen << buffer << "                         PAR / Parent-Station-Children lookup" << endl << endl;
    screen << buffer << "                         HOM / Display this welcome screen" << endl;
    screen << buffer << "                         HEL / Help on individual commands" << endl;
    screen << buffer << "                         BYE / Disconnect" << endl << endl;
}

void Display::showServer(QString hostname, int port)
{
    QTextStream screen(stdout);
    screen << buffer << "Connected to: " << hostname << " : " << port;
    screen << endl << endl;
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
