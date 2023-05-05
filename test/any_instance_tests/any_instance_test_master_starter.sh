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
# Find multiple instances that are already running

# Array for service pids
SERVICE_PIDS=()
NUMBER_SERVICES=3
if [[ $# -gt 0 && $1 == "TCP" ]]; then
    export VSOMEIP_CONFIGURATION=any_instance_test_master_tcp.json
    RELIABILITY_TYPES=($1)
elif [[ $# -gt 0 && $1 == "UDP" ]]; then
    export VSOMEIP_CONFIGURATION=any_instance_test_master_udp.json
    RELIABILITY_TYPES=($1)
else
    # service provides TCP and UDP, client tests TCP, then UDP
    export VSOMEIP_CONFIGURATION=any_instance_test_master.json
    RELIABILITY_TYPES=(TCP UDP)
fi
for RELIABILITY_TYPE in ${RELIABILITY_TYPES[*]}
do
    # Start daemon
    ../examples/routingmanagerd/./routingmanagerd &
    PID_VSOMEIPD=$!
    # Start the services
    for i in $(seq 0 $((NUMBER_SERVICES - 1)))
    do
        ./any_instance_test_service $i &
        SERVICE_PIDS+=($!)
    done

    # Start the client. It will send find_service calls that should be responded to
    # by our service.

    if [ ! -z "$USE_LXC_TEST" ]; then
        echo "starting client test on slave LXC any_instance_test_slave_starter.sh"
        ssh -tt -i $SANDBOX_ROOT_DIR/commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub -o StrictHostKeyChecking=no root@$LXC_TEST_SLAVE_IP "bash -ci \"set -m; cd \\\$SANDBOX_TARGET_DIR/vsomeip_lib/test; ./any_instance_test_slave_starter.sh $RELIABILITY_TYPE\"" &
    elif [ ! -z "$USE_DOCKER" ]; then
        docker exec $DOCKER_IMAGE sh -c "cd $DOCKER_TESTS; ./any_instance_test_slave_starter.sh $RELIABILITY_TYPE" &
    elif [ ! -z "$JENKINS" ]; then
        ssh -tt -i $PRV_KEY -o StrictHostKeyChecking=no jenkins@$IP_SLAVE "bash -ci \"set -m; cd $WS_ROOT/build/test ; ./any_instance_test_slave_starter.sh $RELIABILITY_TYPE\" >> $WS_ROOT/slave_test_output 2>&1" &
    else
    cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** any_instance_test_slave_starter.sh $RELIABILITY_TYPE
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** any_instance_test_master.json and
** any_instance_test_slave.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message
    fi

    # Wait until all services are finished
    for job in ${SERVICE_PIDS[*]}
    do
        # Fail gets incremented if a client exits with a non-zero exit code
        echo "waiting for $job"
        wait $job || FAIL=$(($FAIL+1))
    done

    # Kill the routing manager
    kill $PID_VSOMEIPD
    sleep 1

    # Now run the client first and then start the service

    if [ ! -z "$USE_LXC_TEST" ]; then
        echo "starting client test on slave LXC any_instance_test_slave_starter.sh"
        ssh -tt -i $SANDBOX_ROOT_DIR/commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub -o StrictHostKeyChecking=no root@$LXC_TEST_SLAVE_IP "bash -ci \"set -m; cd \\\$SANDBOX_TARGET_DIR/vsomeip_lib/test; ./any_instance_test_slave_starter.sh\"" &
    elif [ ! -z "$USE_DOCKER" ]; then
        docker exec $DOCKER_IMAGE sh -c "cd $DOCKER_TESTS; ./any_instance_test_slave_starter.sh" &
    elif [ ! -z "$JENKINS" ]; then
        ssh -tt -i $PRV_KEY -o StrictHostKeyChecking=no jenkins@$IP_SLAVE "bash -ci \"set -m; cd $WS_ROOT/build/test ; ./any_instance_test_slave_starter.sh\" >> $WS_ROOT/slave_test_output 2>&1" &
    else
    cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** any_instance_test_slave_starter.sh $RELIABILITY_TYPE
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** any_instance_test_master.json and
** any_instance_test_slave.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message
    fi

    # Wait until the client on the remote node was started
    echo "WAITING FOR CLIENT AVAILABILITY"

    ./any_instance_test_client_availability_checker
    PID_AVAILABILITY_CHECKER=$!

    sleep 1

    wait $PID_AVAILABILITY_CHECKER
    echo "CLIENT IS AVAILABLE NOW"

    # Wait until the client has send all find_service calls. we want it to find
    # the service from the multicast discovery for the next test case
    sleep 5

    # Array for service pids
    SERVICE_PIDS=()
    NUMBER_SERVICES=3
    export VSOMEIP_CONFIGURATION=any_instance_test_master.json
    # Start daemon
    ../examples/routingmanagerd/./routingmanagerd &
    PID_VSOMEIPD=$!
    # Start the services
    for i in $(seq 0 $((NUMBER_SERVICES - 1)))
    do
        ./any_instance_test_service $i &
        SERVICE_PIDS+=($!)
    done

    # Wait until all services are finished
    for job in ${SERVICE_PIDS[*]}
    do
        # Fail gets incremented if a client exits with a non-zero exit code
        echo "waiting for $job"
        wait $job || FAIL=$(($FAIL+1))
    done

    # Kill the routing manager
    kill $PID_VSOMEIPD
    sleep 1
done

# Check if everything went well
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
