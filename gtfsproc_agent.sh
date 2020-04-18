#!/usr/bin/env bash

#
# Shell script to manage day-to-day maintenance of GtfsProc
# (c) 2020, Daniel Brook
#

#
# User / Instance Configurable Parameters
#

# GtfsProc server's binary location
gtfsProcServer="/opt/gtfsproc/gtfsproc"

# Transit Operator's Static Dataset Location
agencyData=""

# Download Location for Static Dataset
staticDataLoc="/home/dan/workspace/MBTA_GTFS_Data"

# Server Port Number (this must be unique for each backend started!)
portNum="5000"

# Additional Options (realtime refresh interval, realtime data location, AM/PM, etc.)
#addedGtfsProcOpts="-r https://cdn.mbta.com/realtime/TripUpdates.pb -u 90"
addedGtfsProcOpts="-t 6"

# Hour of the day to reboot GtfsProc and check for new data (use a 2-digit hour)
#restartHour="04"
restartHour="21"



#
# GtfsProc System Startup (leave these alone)
#

# Base Command to start GtfsProc (see documentation in GtfsProc_Documentation.html to see command options
coreGtfsProcCmd="$gtfsProcServer -d $staticDataLoc -p $portNum"

fullRunCommand="$coreGtfsProcCmd $addedGtfsProcOpts"

# Check which hour it is every hour and see if it is the restart-and-check-for-updates time
updateInterval() {
    while [ 1 ]
    do
        sleep 3600
        currentHour=`date +%H`
        echo "Do we need to restart?"
        if [ $currentHour = $restartHour ]
        then
            echo "Yes, killing GtfsProc on PID $gtfsprocPID"
            kill $gtfsprocPID
            $fullRunCommand 2> /dev/null &
            gtfsprocPID=$!
            echo $gtfsprocPID
        fi
    done
}

# First run, the subsequent restarting and checks are in updateInterval()
$fullRunCommand 2> /dev/null &
gtfsprocPID=$!;
echo "First run PID is $gtfsprocPID"

# Run the infinite loop
updateInterval
