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

if [ $# -lt 2 ]
then
    echo "Please pass a subscription method to this script."
    echo "For example: $0 UDP subscribe_notify_one_test_diff_client_ids_diff_ports_master.json"
    echo "Valid subscription types include:"
    echo "            [TCP_AND_UDP, PREFER_UDP, PREFER_TCP, UDP, TCP]"
    echo "Please pass a json file to this script."
    echo "For example: $0 UDP subscribe_notify_one_test_diff_client_ids_diff_ports_master.json"
    exit 1
fi

# Make sure only valid subscription types are passed to the script
SUBSCRIPTION_TYPES="TCP_AND_UDP PREFER_UDP PREFER_TCP UDP TCP"
VALID=0
for valid_subscription_type in $SUBSCRIPTION_TYPES
do
    if [ $valid_subscription_type == $1 ]
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

# replace master with slave to be able display the correct json file to be used
# with the slave script
MASTER_JSON_FILE=$2
CLIENT_JSON_FILE=${MASTER_JSON_FILE/master/slave}

FAIL=0

# Start the services
export VSOMEIP_APPLICATION_NAME=subscribe_notify_one_test_service_one
export VSOMEIP_CONFIGURATION=$2
./subscribe_notify_one_test_service 1 $1 &

export VSOMEIP_APPLICATION_NAME=subscribe_notify_one_test_service_two
export VSOMEIP_CONFIGURATION=$2
./subscribe_notify_one_test_service 2 $1 &

export VSOMEIP_APPLICATION_NAME=subscribe_notify_one_test_service_three
export VSOMEIP_CONFIGURATION=$2
./subscribe_notify_one_test_service 3 $1 &

sleep 1

if [ ! -z "$USE_LXC_TEST" ]; then
    echo "starting subscribe_notify_one_test_slave_starter.sh on slave LXC with parameters $1 $CLIENT_JSON_FILE"
    ssh -tt -i $SANDBOX_ROOT_DIR/commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub -o StrictHostKeyChecking=no root@$LXC_TEST_SLAVE_IP "bash -ci \"set -m; cd \\\$SANDBOX_TARGET_DIR/vsomeip/test; ./subscribe_notify_one_test_slave_starter.sh $1 $CLIENT_JSON_FILE\"" &
    echo "remote ssh job id: $!"
elif [ ! -z "$USE_DOCKER" ]; then
    docker run --name snotms --cap-add NET_ADMIN $DOCKER_IMAGE sh -c "route add -net 224.0.0.0/4 dev eth0 && cd $DOCKER_TESTS && ./subscribe_notify_one_test_slave_starter.sh $1 $CLIENT_JSON_FILE" &
else
    cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** subscribe_notify_one_test_slave_starter.sh $1 $CLIENT_JSON_FILE
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** subscribe_notify_one_test_diff_client_ids_diff_ports_master.json and
** subscribe_notify_one_test_diff_client_ids_diff_ports_slave.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message
fi

# Wait until client and service are finished
for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || ((FAIL+=1))
done

if [ ! -z "$USE_DOCKER" ]; then
    docker stop snotms
    docker rm snotms
fi

# Check if both exited successfully 
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
