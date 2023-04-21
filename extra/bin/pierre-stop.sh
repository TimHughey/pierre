#!/bin/env /bin/zsh

PID_FILE=/run/pierre/pierre.pid

if [[ -f ${PID_FILE} ]]; then
  PID=$(cat /run/pierre/pierre.pid)
  echo -n "sending SIGINT to ${PID} "
  kill -INT ${PID} 1>/dev/null 2

  PID_STATUS=$(kill -s 0 ${PID})

  if [[ ${PID_STATUS} != 0 ]]; then

  while kill -0 ${PID} 2> /dev/null; do 
    sleep 0.25
    echo -n "."
  done
  echo 
  fi
else
  echo "${PID_FILE}: no such file (pierre is not running)"
fi