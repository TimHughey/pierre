#!/bin/env /bin/zsh

LOG_FILE=/var/log/pierre/pierre.log

exec tail -F ${LOG_FILE}