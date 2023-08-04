#!/usr/bin/env python3

# This file is part of GtfsProc
# (C) 2021, Daniel Brook
#
# See the LICENSE and README files in the root of the project directory.
#
# This is a very rudimentary non-regression test environment to ensure that changes to GtfsProc
# do not cause any possibly problematic side-effects to already-working parts of the schedule
# processor and real-time integration code.
#
# All directories under tests/ will be scanned for any .test files to find the necessary
# server data load parameters, queries (and test case descriptions) and expected responses.
# Any line in the expected responses that begin with a "*" (asterisk) will NOT be
# checked for equality.
#
# Again ... this is a VERY basic system.
#
# See the example test suites in the included directories under tests/*

import sys
import os
import subprocess
from time import sleep

# All tests appear within this script's directory ./GtfsProcSuite/tests/*
this_dir = os.path.dirname(os.path.realpath(__file__))


class TestCase:
    def __init__(self):
        self.case = ""
        self.query = ""
        self.expect = ""


class RegressionSet:
    def __init__(self):
        self.descrip = ""
        self.startup = []
        self.tests = []


def openRegressionFile(regression_path):
    ''' Opens a regression file and fills in description and GtfsProc startup parameters.
        In addition the list of test cases (name, actual query, and expected result) is attached.

        args:
            regression_path (str): path to an individual file conforming to the .test format

        returns: filled RegressionSet object with description, startup command, and all TestCases
    '''
    reg_set = RegressionSet()

    reg_parse = "None"
    reg_file = open(regression_path, "r")

    # Read the file line by line
    # Allowed fields:
    #   @Description - @End: (Multiline case description)
    #   @StartParams - @End: (GtfsProc startup parameters relevant to the testcases)
    # +1 or more of:
    #   @Case:               (Description of a testcase)
    #   @Query:              (Query to shoot at running GtfsProc instance)
    #   @Expected - @End:    (Multiline expected JSON results from the Query)
    for line in reg_file:
        line_strip = line.rstrip('\n')
        if reg_parse == "None":
            if line_strip == "@Description":
                reg_parse = "Description"
            elif line_strip == "@StartParams":
                reg_parse = "Parameters"
            elif line.startswith("@Expected"):
                reg_parse = "Expected"
            elif line.startswith("@Case"):
                test_case = TestCase()
                test_case.case = line_strip[6:]
            elif line.startswith("@Query"):
                test_case.query = line_strip[7:]
        elif reg_parse == "Description":
            if line_strip == "@End":
                reg_parse = "None"
            else:
                reg_set.descrip = reg_set.descrip + line
        elif reg_parse == "Parameters":
            if line_strip == "@End":
                reg_parse = "None"
            else:
                reg_set.startup.append(line_strip)
        elif reg_parse == "Expected":
            if line_strip == "@End":
                test_case.expect = test_case.expect.rstrip('\n')
                reg_set.tests.append(test_case)
                reg_parse = "None"
            else:
                test_case.expect = test_case.expect + line
        else:
            print(f"Unexptected File Condition: '{reg_parse}'")

    return reg_set


def actualMatchesExpected(received, expected):
    ''' Determines if the expected and received results are of the same number of lines,
        then if each line is equal. If an expected line starts with a "*", then the line's
        contents in the received are ignored (not used for the basis of returning True/False).

        args:
            received (str): The JSON response returned from the actual transaction.
            expected (str): The JSON response read from the test case file for matching.

        returns: True if a match (considering wildcards), False if different
    '''
    rec_split = received.splitlines()
    exp_split = expected.splitlines()

    if len(rec_split) != len(exp_split):
        print("\033[91m         EXPECT/ACTUAL DIFFER IN LENGTH\033[00m")
        return False

    for line in range(len(rec_split)):
        if exp_split[line][0] == '*':
            continue
        elif rec_split[line] != exp_split[line]:
            print(f"\033[91m         DIFFER@ {line}")
            print(f"         EXPECT: {exp_split[line].lstrip()}")
            print(f"         ACTUAL: {rec_split[line].lstrip()}\033[00m")
            return False

    return True


def loadGtfsProcAndRunCases(regression_file):
    ''' Loads a GtfsProc server with the flags dictated by the regression_file, then shoots
        the cases within and compares the expected results. The passed and total cases are
        counted for the sake of statistics / determining overall pass/fail conditions.

        args:
            regression_file (str): path to the regression file to scan for cases

        returns: passed_cases (int), total_cases (int) as a tuple
    '''
    passed_cases = 0
    total_cases = 0

    # Switch to the file's working directory so GtfsProc can be started relative to it
    os.chdir(os.path.dirname(regression_file))

    regression_set = openRegressionFile(regression_file)
    gtfs_start_ops = [gtfsproc_path, *regression_set.startup]
    gtfs_process = subprocess.Popen(gtfs_start_ops,
                                    shell=False,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)

    ok_to_test = False
    while True:
        boot_msg = gtfs_process.stdout.readline()
        if gtfs_process.poll() is not None:
            break
        if boot_msg == b"SERVER STARTED - READY TO ACCEPT INCOMING CONNECTIONS\n":
            ok_to_test = True
            break
        if boot_msg == b"(!) COULD NOT START SERVER - SEE ERROR STRING ABOVE\n":
            print("\nCannot run -- perhaps there is another instance of GtfsProc running?")
            exit()

    if ok_to_test:
        gtfs_cli_opts = [gtfsclnt_path, "localhost", "5000", "P"]

        for test_case in regression_set.tests:
            gtfs_client = subprocess.Popen(gtfs_cli_opts,
                                           shell=False,
                                           stdout=subprocess.PIPE,
                                           stdin=subprocess.PIPE)
            gtfs_client.stdin.write("{}\n".format(test_case.query).encode("utf-8"))
            gtfs_client.stdin.flush()
            test_case_query = f"[{test_case.query}] {test_case.case}"
            server_resp = ""
            while True:
                json_line = gtfs_client.stdout.readline()
                server_resp = server_resp + json_line.decode("utf-8")
                if gtfs_client.poll() is not None:
                    break

            if actualMatchesExpected(server_resp, test_case.expect):
                print(f"\033[92m  [PASS] {test_case_query}\033[00m")
                passed_cases += 1
            else:
                print(f"\033[91m  [FAIL] {test_case_query}\033[00m")
            total_cases += 1

    gtfs_process.kill()
    sleep(2)

    return passed_cases, total_cases
    

def runAgencyDirectoryTests(agency_set_dir):
    ''' Finds all .test files within a directory under tests/(Agency)/*

        args:
            agency_set_dir (str): directory path to scan for .test files

        returns: passed_cases (int), total_cases (int) sums from all cases as a tuple
    '''
    passed_cases = 0
    total_cases = 0

    # Loop over all .test files and send them through the regression process
    with os.scandir(agency_set_dir) as it:
        for file_entry in it:
            if not file_entry.is_dir() and file_entry.name.endswith('.test'):
                print(file_entry.name)
                suite_passed, suite_ran = loadGtfsProcAndRunCases(file_entry.path)
                passed_cases += suite_passed
                total_cases += suite_ran

    return passed_cases, total_cases


def findAllAgencyDirectories(this_dir):
    ''' Scans through all agency test directories under tests/* so groups of .test files may
        be found and processed by the regression system.

        args:
            this_dir (str): the directory that this script is located (should be tests/)

        returns: passed_cases (int), total_cases (int) sums from all cases in all dirs as a tuple
    '''
    passed_cases = 0
    total_cases = 0

    # Loop over all subdirectories in the test suite to find agency test cases and datasets
    with os.scandir(f"{this_dir}") as it:
        for agency_entry in it:
            if agency_entry.is_dir():
                print(f"\n{agency_entry.name}/")
                dir_passed, dir_ran = runAgencyDirectoryTests(agency_entry.path)
                passed_cases += dir_passed
                total_cases += dir_ran

    return passed_cases, total_cases


print("\nWelcome to the GtfsProc Non-Regression Test Environment!")

if __name__ == "__main__":
    if len(sys.argv) not in [3, 4]:
        print("You must provide the path to the gtfsproc server and client_cli binaries,")
        print(f"like so:  $ {sys.argv[0]} /path/to/gtfsproc /path/to/client_cli\n")
        print("You can also choose to process a single test file,")
        print(f"like so:  $ {sys.argv[0]} /path/to/gtfsproc /path/to/client_cli tests/Agency/my_suite.test\n")
        exit()
    current_wrkdir = os.getcwd()
    gtfsproc_path = f"{current_wrkdir}/{sys.argv[1]}"
    gtfsclnt_path = f"{current_wrkdir}/{sys.argv[2]}"
    
    os.chdir(this_dir)
    print(f"Starting from working dir: {os.getcwd()}")

    if len(sys.argv) == 4:
        print(f"Executing a single file: {current_wrkdir}/{sys.argv[3]}")
        total_passed, total_ran = loadGtfsProcAndRunCases(f"{current_wrkdir}/{sys.argv[3]}")
    else:
        total_passed, total_ran = findAllAgencyDirectories(this_dir)

    if total_passed is not total_ran:
        failed_cases = total_ran - total_passed
        print(f"\n\033[91mTEST ERROR:\033[00m {failed_cases} out of {total_ran} test cases failed!\n")
    else:
        print(f"\n\033[92mTESTS ALL PASSED:\033[00m {total_ran} cases checked.\n")
