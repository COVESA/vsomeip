#!/bin/bash
# Copyright (C) 2015-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Purpose: This script is needed to start the services with
# one command. This is necessary as ctest - which is used to run the
# tests - isn't able to start multiple binaries for one testcase. Therefore
# the testcase simply executes this script. This script then runs the services
# and checks that all exit successfully.

FAIL=0

export VSOMEIP_CONFIGURATION=cpu_load_test_client_master.json
./cpu_load_test_client --protocol UDP --calls 1000 &

sleep 1

cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** cpu_load_test_slave_starter.sh
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** cpu_load_test_client_master.json,
** cpu_load_test_service_master.json,
** cpu_load_test_client_client.json and
** cpu_load_test_service_client.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message

for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || FAIL=$(($FAIL+1))
done

sleep 4
cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Now switching roles and running service on this host
*******************************************************************************
*******************************************************************************
End-of-message

export VSOMEIP_CONFIGURATION=cpu_load_test_service_master.json
./cpu_load_test_service &
sleep 1

for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || FAIL=$(($FAIL+1))
done


# Check if both exited successfully 
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
