#!/usr/bin/env bash

# Find where this script is...
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
cd $SCRIPTPATH/..

# Source the virtual environment one directory up from here (./bin)
if [[ -z "${VIRTUAL_ENV}" ]]; then
  echo "NOTE: Python3 Virtual Environment is being set first."
  . venv/bin/activate
fi

# Normalize the import path(s) for all scripts in ./scr
export PYTHONPATH="$SCRIPTPATH/../src:$PYTHONPATH"

# Run the script as a fully-fledged application, passing arguments directly to it
python3 $SCRIPTPATH/../src/main.py "$@"

# Example arguments:
# TODO
