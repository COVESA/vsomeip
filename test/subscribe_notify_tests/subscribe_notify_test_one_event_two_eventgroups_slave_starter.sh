#!/bin/bash
# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Purpose: This script is needed to start the services with
# one command. This is necessary as ctest - which is used to run the
# tests - isn't able to start multiple binaries for one testcase. Therefore
# the testcase simply executes this script. This script then runs the services
# and checks that all exit successfully.

if [ $# -lt 1 ]; then
    echo "Please pass a json file to this script."
    echo "For example: $0 subscribe_notify_test_one_event_two_eventgroups_slave.json"
    exit 1
fi

FAIL=0

export VSOMEIP_CONFIGURATION=$1
# start daemon
../daemon/./vsomeipd &
PID_VSOMEIPD=$!

# Start the services
./subscribe_notify_test_one_event_two_eventgroups_service &
PID_SERVICE=$!

# wait until service exits successfully
wait $PID_SERVICE || FAIL=$(($FAIL+1))


# kill daemon
kill $PID_VSOMEIPD
wait $PID_VSOMEIPD || FAIL=$(($FAIL+1))

echo ""

# Check if both exited successfully 
if [ $FAIL -eq 0 ]; then
    exit 0
else
    exit 1
fi
