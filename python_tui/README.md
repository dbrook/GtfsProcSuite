# Python + Urwid (Curses) Front-End to GtfsProc

Sure, the GUI projects are nice and all, and the included C++ client_cli more-or-less
"works", but it's not very scrollable or nice to use if you are copying/writing out
route/trip/stop IDs everywhere.

This command-line / curses interface allows users to use the server data in a more
interactive fashion.

# First-Time Setup

## You'll need some python
`sudo apt install python3-pip python3-venv`

## Create and activate the virtual environment
`python3 -m venv venv`
`. venv/bin/activate`

## Install dependencies
`pip install -r requirements.txt`

## Run it!

Note: your mileage may vary based on your GtfsProc server, but it'll likely be:

`./bin/gtfsproc_tui.sh -n localhost -p 5000`

To quit, use the `F8` key on your keyboard.
