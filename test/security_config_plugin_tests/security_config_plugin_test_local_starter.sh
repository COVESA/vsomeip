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

FAIL=0

cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Running first test
*******************************************************************************
*******************************************************************************
End-of-message

# Array for client pids
CLIENT_PIDS=()
export VSOMEIP_CONFIGURATION=security_config_plugin_test_local.json

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f ../plugins/mgu)
# start daemon
../daemon/./vsomeipd &
PID_VSOMEIPD=$!

sleep 1

# Start the service
export VSOMEIP_APPLICATION_NAME=service-sample
./security_config_plugin_test_service &
PID_SERVICE=$!

sleep 1

# Start the client
export VSOMEIP_APPLICATION_NAME=client-sample
./security_config_plugin_test_client &
CLIENT_PIDS+=($!)

# Wait until all clients are finished
for job in ${CLIENT_PIDS[*]}
do
    # Fail gets incremented if a client exits with a non-zero exit code
    wait $job || FAIL=$(($FAIL+1))
done

kill $PID_VSOMEIPD
sleep 1

kill $PID_SERVICE
sleep 1

# Check if everything went well
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
