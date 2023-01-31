#!/bin/env /bin/zsh

PID_FILE=/run/pierre/pierre.pid

if [[ -f ${PID_FILE} ]]; then
  PID=$(cat /run/pierre/pierre.pid)
  echo "sending SIGINT to ${PID}..."
  kill -INT ${PID}

  PID_STATUS=$(kill -s 0 ${PID})

  if [[ ${PID_STATUS} != 0 ]]; then
    echo "${PID} still alive"
  fi
else
  echo "${PID_FILE}: no such file (pierre is not running)"
fi