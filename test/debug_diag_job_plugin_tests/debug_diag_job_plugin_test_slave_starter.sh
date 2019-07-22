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

export VSOMEIP_CONFIGURATION=debug_diag_job_plugin_test_slave.json

../daemon/./vsomeipd &
PID_VSOMEIPD=$!

# Start the services
export VSOMEIP_APPLICATION_NAME=debug_diag_job_plugin_test_client
./debug_diag_job_plugin_test_client &
PID_CLIENT=$!

wait $PID_CLIENT || FAIL=$(($FAIL+1))

kill $PID_VSOMEIPD

sleep 1
echo ""

# Check if both exited successfully 
exit $FAIL
