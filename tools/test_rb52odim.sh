#!/bin/sh
############################################################
# Description: Script that performs the actual unit tests
#
# Author(s):   Anders Henja and Daniel Michelson
#
# Copyright:   Swedish Meteorological and Hydrological Institute, 2009   
#              The Crown (i.e. Her Majesty the Queen in Right of Canada), 2016
#
# History:     2009-06-15 Created by Anders Henja
#              2016-08-17 Modified by Daniel Michelson
############################################################
SCRFILE=`python -c "import os;print os.path.abspath(\"$0\")"`
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

RES=255

if [ $# -gt 0 -a "$1" = "alltest" ]; then
  "$SCRIPTPATH/run_python_script.sh" "${SCRIPTPATH}/../test/pytest/RB52ODIMFullTestSuite.py" "${SCRIPTPATH}/../test/pytest"
  RES=$?
else
  "$SCRIPTPATH/run_python_script.sh" "${SCRIPTPATH}/../test/pytest/RB52ODIMTestSuite.py" "${SCRIPTPATH}/../test/pytest"
  RES=$?
fi

exit $RES
