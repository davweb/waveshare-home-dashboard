#!/bin/sh

debug() {
  if [ "${LOG_LEVEL}" == "DEBUG" ]
  then
    echo `date +"[%Y-%m-%dT%H:%M:%S%z]"` DEBUG $*
  fi
}

error() {
  echo `date +"[%Y-%m-%dT%H:%M:%S%z]"` ERROR $*
}

for VAR in BUS_STOP_IDS PIRATE_API_KEY LAT_LONG
do
  VALUE=`eval "echo \\$$VAR"`

  if [ -z "${VALUE}" ]
  then
    error Environment variable ${VAR} is not set.
    MISSING_VARIABLE=TRUE
  fi
done

if [ -n "${MISSING_VARIABLE}" ]
then
  error Missing configuration, exiting.
  exit 3
fi

# Show dependency versions
debug `python --version`

# Run now
cd /app
source venv/bin/activate
python -m dashboard
