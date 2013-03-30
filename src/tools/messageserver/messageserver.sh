#!/bin/sh

# Launches messageserver if it's not up already : pidof would return 1
# if the process was not running, 0 otherwise.
pidof messageserver
if [ $? -eq 1 ]; then
    echo "START_MESSAGESERVER" > /var/run/upstart.fifo
fi
