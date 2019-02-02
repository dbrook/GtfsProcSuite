#ifndef DISPLAY_H
#define DISPLAY_H

#include <QObject>

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
