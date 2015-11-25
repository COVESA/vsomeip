#!/bin/bash
# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Purpose: This script is needed to start the services with
# one command. This is necessary as ctest - which is used to run the
# tests - isn't able to start two binaries for one testcase. Therefore
# the testcase simply executes this script. This script then runs the services
# and checks that both exit successfully.

if [ $# -lt 1 ]
then
    echo "Please pass a json file to this script."
    echo "For example: $0 client_id_test_diff_client_ids_diff_ports_master.json"
    exit 1
fi

MASTER_JSON_FILE=$1
CLIENT_JSON_FILE=${MASTER_JSON_FILE/master/slave}

FAIL=0

# Start the services
export VSOMEIP_APPLICATION_NAME=client_id_test_service_one
export VSOMEIP_CONFIGURATION=$1
./client_id_test_service 1 &

export VSOMEIP_APPLICATION_NAME=client_id_test_service_two
export VSOMEIP_CONFIGURATION=$1
./client_id_test_service 2 &



cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** client_id_test_slave_starter.sh $CLIENT_JSON_FILE
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** client_id_test_diff_client_ids_diff_ports_master.json and
** client_id_test_diff_client_ids_diff_ports_slave.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message

# Wait until client and service are finished
for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || ((FAIL+=1))
done

# Check if both exited successfully 
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
