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

if [ $# -lt 2 ]
then
    echo "Please pass a subscription method to this script."
    echo "For example: $0 UDP initial_event_test_diff_client_ids_diff_ports_master.json"
    echo "Valid subscription types include:"
    echo "            [TCP_AND_UDP, PREFER_UDP, PREFER_TCP, UDP, TCP]"
    echo "Please pass a json file to this script."
    echo "For example: $0 UDP initial_event_test_diff_client_ids_diff_ports_master.json"
    echo "To use the same service id but different instances on the node pass SAME_SERVICE_ID as third parameter"
    exit 1
fi

PASSED_SUBSCRIPTION_TYPE=$1
PASSED_JSON_FILE=$2
PASSED_SAME_SERVICE_ID_FLAG=$3

# Make sure only valid subscription types are passed to the script
SUBSCRIPTION_TYPES="TCP_AND_UDP PREFER_UDP PREFER_TCP UDP TCP"
VALID=0
for valid_subscription_type in $SUBSCRIPTION_TYPES
do
    if [ $valid_subscription_type == $PASSED_SUBSCRIPTION_TYPE ]
    then
        VALID=1
    fi
done

if [ $VALID -eq 0 ]
then
    echo "Invalid subscription type passed, valid types are:"
    echo "            [TCP_AND_UDP, PREFER_UDP, PREFER_TCP, UDP, TCP]"
    echo "Exiting"
    exit 1
fi

print_starter_message () {
cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** initial_event_test_slave_starter.sh $PASSED_SUBSCRIPTION_TYPE $CLIENT_JSON_FILE $PASSED_SAME_SERVICE_ID_FLAG
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** initial_event_test_diff_client_ids_diff_ports_master.json and
** initial_event_test_diff_client_ids_diff_ports_slave.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message
}

# replace master with slave to be able display the correct json file to be used
# with the slave script
MASTER_JSON_FILE=$PASSED_JSON_FILE
CLIENT_JSON_FILE=${MASTER_JSON_FILE/master/slave}

FAIL=0

# Start the services
export VSOMEIP_CONFIGURATION=$PASSED_JSON_FILE

export VSOMEIP_APPLICATION_NAME=initial_event_test_service_one
./initial_event_test_service 1 $PASSED_SAME_SERVICE_ID_FLAG &
PID_SERVICE_ONE=$!

export VSOMEIP_APPLICATION_NAME=initial_event_test_service_two
./initial_event_test_service 2 $PASSED_SAME_SERVICE_ID_FLAG &
PID_SERVICE_TWO=$!

export VSOMEIP_APPLICATION_NAME=initial_event_test_service_three
./initial_event_test_service 3 $PASSED_SAME_SERVICE_ID_FLAG &
PID_SERVICE_THREE=$!

unset VSOMEIP_APPLICATION_NAME

# Array for client pids
CLIENT_PIDS=()

# Start some clients
if [[ $PASSED_SUBSCRIPTION_TYPE == "TCP_AND_UDP" ]]
then
    ./initial_event_test_client 9000 $PASSED_SUBSCRIPTION_TYPE $PASSED_SAME_SERVICE_ID_FLAG &
    FIRST_PID=$!
    sleep 1
    print_starter_message
    wait $FIRST_PID || FAIL=$(($FAIL+1))
else
    for client_number in $(seq 9000 9009)
    do
        ./initial_event_test_client $client_number $PASSED_SUBSCRIPTION_TYPE $PASSED_SAME_SERVICE_ID_FLAG &
        CLIENT_PIDS+=($!)
    done
fi


# Start availability checker in order to wait until the services on the remote
# were started as well
./initial_event_test_availability_checker 1234 $PASSED_SAME_SERVICE_ID_FLAG &
PID_AVAILABILITY_CHECKER=$!

sleep 1

if [ $PASSED_SUBSCRIPTION_TYPE != "TCP_AND_UDP" ]
then
    print_starter_message
fi

# wait unti the services on the remote node were started as well
wait $PID_AVAILABILITY_CHECKER

# sleep to make sure the following started clients will have to get
# the cached event from the routing manager daemon
sleep 2

if [[ $PASSED_SUBSCRIPTION_TYPE == "TCP_AND_UDP" ]]
then
    ./initial_event_test_client 9010 $PASSED_SUBSCRIPTION_TYPE $PASSED_SAME_SERVICE_ID_FLAG &
    FIRST_PID=$!
    wait $FIRST_PID || FAIL=$(($FAIL+1))
else
    for client_number in $(seq 9010 9020)
    do
       ./initial_event_test_client $client_number $PASSED_SUBSCRIPTION_TYPE $PASSED_SAME_SERVICE_ID_FLAG &
       CLIENT_PIDS+=($!)
    done
fi


# Wait until all clients are finished
for job in ${CLIENT_PIDS[*]}
do
    # Fail gets incremented if a client exits with a non-zero exit code
    wait $job || FAIL=$(($FAIL+1))
done

# wait until all clients exited on slave side
./initial_event_test_stop_service MASTER &
PID_STOP_SERVICE=$!
wait $PID_STOP_SERVICE

# kill the services
kill $PID_SERVICE_THREE
kill $PID_SERVICE_TWO
kill $PID_SERVICE_ONE

sleep 1
echo ""

# Check if both exited successfully 
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
