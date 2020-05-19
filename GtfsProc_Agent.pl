#!/usr/bin/env perl

###################################################################################
# Perl script to manage if new data must be downloaded from the upstream provider
# for running a long-term GtfsProc server in a more maintenance-free way.
#
# To use this script, you must have perl, curl, and wget installed
#
# Make sure to fill in your local configuration parameters in the variables below
#
# After, just run this script and it will recheck daily (at the specified
# $restartHour) if new data is available from the $agencyDataLoc. If it is, then
# the server will be killed an restarted with the new data automatically.
#
# (c) 2020, Daniel Brook (danb358@gmail.com)
###################################################################################

##############################################
## User / Instance configuration parameters ##
##############################################

# GtfsProc server program location
my $gtfsProcServer = "/opt/gtfsproc/gtfsproc";

# Web location of Transit Operator's Static Dataset
my $agencyDataLoc  = "";
# Example:
#my $agencyDataLoc  = "https://cdn.mbta.com/MBTA_GTFS.zip";

# Local static dataset download directory (should ONLY hold the dataset, this entire
# directory is purged when new data is downloaded!)
my $staticDataLoc  = "";
# Example:
#my $staticDataLoc  = "/opt/gtfsproc/staticdata";

# Local file to hold last-modified dataset time
my $staticDataStat = "";
# Example:
#my $staticDataStat = "/opt/gtfsproc/staticdata_current.dat";

# Local file to stage the check of the [potentially] new data
my $tmpStaticStat  = "";
# Example:
#my $tmpStaticStat  = "/opt/gtfsproc/staticdata_staged.dat";

# GtfsProc Server Port to listen on
my $portNum        = "5000";

# Additional options for GtfsProc, like realtime data location and refresh interval
# See gtfsproc --help to find the other options (besides -p and -d which are mandatory)
my $addedProcOpts  = "-t 1";
# Example:
#my $addedProcOpts  = "-t 1 -r https://cdn.mbta.com/realtime/TripUpdates.pb -u 90";

# The hour (in 24-hour format of the system's local time) to attempt to restart and fetch
# You should pick an hour that fits your transit agency (like if they operate after midnight)
# Example: for 04:00 am each day, use "04", for 11:00 pm each day, use "23"
my $restartHour    = "03";



###########################################################
## GtfsProc System Startup Functions - leave these alone ##
###########################################################

# Build the runtime command
$gtfsProcLine = "$gtfsProcServer -d $staticDataLoc -p $portNum $addedProcOpts";

# Check for new data using curl with header information
sub UpdateAvailable
{
    my $newData = 0;

    # Read in the current configuration (if possible)
    open my $fileHandle, '<', $staticDataStat;
    chomp(my @fileLines = <$fileHandle>);
    close $fileHandle;
    
    my $lastModif;
    my $newModif;
    
    # Find the currently-held static dataset's updated time
    foreach $line (@fileLines) {
        if ($line =~ /^last-modified/i) {
            print "**** Local's Last Modified:  $line\n";
            $lastModif = $line;
            $newModif  = $line;    # Safety precaution in case the remote is dead
            last;
        }
    }
    
    # See how new the server's copy of the static dataset is
    `curl -I $agencyDataLoc > $tmpStaticStat 2> /dev/null`;

    # Check how new the data on the server was
    open my $newFileHandle, '<', $tmpStaticStat;
    chomp(my @newFileLines = <$newFileHandle>);
    close $newFileHandle;
    foreach $line (@newFileLines) {
        if ($line =~ /^last-modified/i) {
            print "**** Agency's Last Modified: $line\n";
            $newModif = $line;
            last;
        }
    }

    # If newer than what we have, download and extract the data
    if ($lastModif ne $newModif) {
        print "**** New data is available from the agency! Purge old local repository.\n";

        opendir(DIR, $staticDataLoc) or die $!;
        while (my $staticDataFile = readdir(DIR)) {
            # Unlink all files so we can redownload
            if ($staticDataFile ne "." && $staticDataFile ne "..") {
                unlink "$staticDataLoc/$staticDataFile";
            }
        }

        print "**** Downloading new data\n";
        `wget -O $staticDataLoc/agencydata.zip $agencyDataLoc`;

        print "**** Extracting data\n";
        `unzip $staticDataLoc/agencydata.zip -d $staticDataLoc`;

        # Overwrite the old version tracking file
        `cp $tmpStaticStat $staticDataStat`;

        $newData = 1;
    } else {
        print "**** No new data available.\n";
    }

    print "\n";
    return ($newData);
}

# Every hour, check if it is the time of day to attempt an update
sub HourlyCheckIfRestartTime
{
    my $serverPID = $_[0];
    print "**** Will restart GtfsProc each day at $restartHour if new data is available\n";
    while (1) {
        sleep(3600);
        my $currentHour = `date +%H`;
        if ($currentHour == $restartHour) {
            print "**** Check for a new static dataset now ...\n";

            # Get new data if it is available
            if (UpdateAvailable() == 1) {
                # Kill the existing server
                print "**** Restarting GtfsProc from PID $serverPID\n";
                kill('TERM', $serverPID);
                waitpid($serverPID, 0);

                # Restart server
                $serverPID = fork();
                die "Unable to fork: $!" unless defined($serverPID);
                if (!$serverPID) {
                    print "**** Starting with command: $gtfsProcLine\n\n";
                    exec($gtfsProcLine);
                    die "Unable to execute $!";
                    exit;
                }
            }
        }
    }
}

# Execute - The first will ensure the data is present
print "**** GtfsProc Static Dataset Updater and Service Recycler Agent ****\n\n";

UpdateAvailable();

my $gtfsPID = fork();
die "Unable to fork: $!" unless defined($gtfsPID);
if (!$gtfsPID) {
    print "**** Starting with command: $gtfsProcLine\n\n";
    exec($gtfsProcLine);
    die "Unable to execute $!";
    exit;
}

# Start infinite wait loop
HourlyCheckIfRestartTime($gtfsPID);
