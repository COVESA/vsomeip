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
    echo "For example: $0 UDP subscribe_notify_one_test_diff_client_ids_diff_ports_slave.json"
    echo "Valid subscription types include:"
    echo "            [TCP_AND_UDP, PREFER_UDP, PREFER_TCP, UDP, TCP]"
    echo "Please pass a json file to this script."
    echo "For example: $0 UDP subscribe_notify_one_test_diff_client_ids_diff_ports_slave.json"
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


FAIL=0
# Start the services
export VSOMEIP_APPLICATION_NAME=subscribe_notify_one_test_service_four
export VSOMEIP_CONFIGURATION=$2
./subscribe_notify_one_test_service 4 $1 &

export VSOMEIP_APPLICATION_NAME=subscribe_notify_one_test_service_five
export VSOMEIP_CONFIGURATION=$2
./subscribe_notify_one_test_service 5 $1 &

export VSOMEIP_APPLICATION_NAME=subscribe_notify_one_test_service_six
export VSOMEIP_CONFIGURATION=$2
./subscribe_notify_one_test_service 6 $1 &

# Wait until all applications are finished
for job in $(jobs -p)
do
    # Fail gets incremented if one of the binaries exits
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
