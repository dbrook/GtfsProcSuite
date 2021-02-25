GtfsProcSuite
=============
Roll your own public transit API backend with GTFS and GTFS-Realtime data without the need to make agency-specific
clients! GtfsProc integrates GTFS static and realtime data feeds into a single unified server interface.

GtfsProc is a GTFS schedule processor which loads transit agency data into memory and associates routes, trips, and
stops, responding to lookup requests made from clients using Qt TCP Sockets with JSON-encoded results. The main use case
is for computing predicted and/or scheduled wait times at a stop or combination of stops, but route, station, trip,
and all manner of different data in an agency's GTFS feed are also retrievable from the server with an array of
supported commands.

See the GtfsProc_Documentation.html file for details on starting the server and how clients can send requests and
the schema for all the responses to different request types.

The TCP server code comes from Bryan Cairns of VoidRealms. See http://www.voidrealms.com/ for the original source and license.
The GtfsProc code is written by Daniel Brook, (c) 2018-2020, GPLv3.

GtfsProcSuite consists of 3 applications:
- Server that loads the schedule data (and real-time data if available) and processes client requests (gtfsproc/)
- Command-line client program that was written for quick rendering (and be warned, the code for it is quite messy (client_cli/)
- For compatible agencies, the GtfsProc_Agent will control starting and stopping backends and retrieving new data automatically

There is no "official" GtfsProc front-end application, but a PHP-based sample is being tested on Massachusetts Bay
Transportation Authority data at http://www.danbrook.org/gtfsproc

Certain agencies also provide modification times on their zipped feeds, so using the GtfsProc_Agent.pl script can
automatically check for updates in the background and recycle the server when new data is found. The following
agencies were tested and work with this agent:
- Massachusetts Bay Transportation Authority (MBTA)
- Rhode Island Public Transportation Authority (RIPTA)
- CT Transit - ConnDOT-operated buses and CTfastrak
- Metropolitain Transportation Authority - MetroNorth and Long Island Railroads
- TriMet (Portland, Oregon)
- RTC Southern Nevada

Agencies with realtime data have some differences in feed details, and the GtfsProc server can be started with several
options to work around these differences (see the GtfsProc_Documentation.html file for details). Testing on the following
agencies with realtime data revealed the following overrides were necessary to correctly calculate trip predictions:
- MBTA: default options are fine (MBTA data was used at the original reference)
- RIPTA: use '-l 1' (less-strict realtime trip start-time enforcement)
- CT Transit: use '-r 90 -l 1 -q' (refresh at 90 seconds, disable stop sequence matching)
- SEPTA: use '-l 2' (no date enforcement)
- RTC Southern Nevada: use '-l 2'

Before finalizing any changes, be sure to run the non-regression tests under tests/*. You need Python (3) installed and
simply need to:
- Compile the gtfsproc and client_cli targets
- Run: ./tests/nonregression_run.py /path/to/gtfsproc /path/to/client_cli
- Examine the results, hopefully you get "TESTS ALL PASSED", if not, be sure to investigate the cause of any differences.
- Any changes made are a good candidate for adding new test cases.
