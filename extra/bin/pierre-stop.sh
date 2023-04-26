#!/bin/env /bin/zsh

# set -o errexit
set -o nounset

PID_FILE=/run/pierre/pierre.pid

if [[ ! -f ${PID_FILE} ]]; then
  exit 0;
fi

PID=$(cat /run/pierre/pierre.pid)

# confim PID exists and we can signal it

kill -0 ${PID} > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "could not signal pid=${PID}"
  exit 1
fi

# attempt a graceful shutdown
kill -INT ${PID} > /dev/null 2>&1
if [ $? -ne 0 ]; then
  echo "could not send SIGINT to pid=${PID}"
  exit 1
fi

echo -n "waiting for pid ${PID} to exit..."

while kill -0 ${PID} > /dev/null 2>&1 ; do 
  sleep 0.25
done
echo " done"
