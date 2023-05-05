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

if [ $# -lt 1 ]
then
    echo "Please pass a reliability type to this script."
    echo "For example: $0 UDP"
    echo "             $0 TCP"
    exit 1
fi

FAIL=0
# Find multiple instances that are already running

export VSOMEIP_CONFIGURATION=any_instance_test_slave.json
# start client
./any_instance_test_client $1 &
PID_CLIENT=$!

# Fail gets incremented if a client exits with a non-zero exit code
wait $PID_CLIENT || FAIL=$(($FAIL+1))

# Check if everything went well
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
