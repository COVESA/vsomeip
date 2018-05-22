#!/bin/bash
# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Purpose: This script is needed to start the client and service with
# one command. This is necessary as ctest - which is used to run the
# tests - isn't able to start two binaries for one testcase. Therefore
# the testcase simply executes this script. This script then runs client
# and service and checks that both exit sucessfully.

FAIL=0

# Start the service
export VSOMEIP_APPLICATION_NAME=local_payload_test_service
export VSOMEIP_CONFIGURATION=local_payload_test_service.json
./payload_test_service &
SERIVCE_PID=$!
sleep 1;

# Start the client
export VSOMEIP_APPLICATION_NAME=local_payload_test_client
export VSOMEIP_CONFIGURATION=local_payload_test_client.json
./payload_test_client --number-of-messages 100 --max-payload-size 10485760 &
CLIENT_PID=$!

# Wait until client and service are finished
for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || ((FAIL+=1))
done

# Check if client and server both exited sucessfully and the service didnt't
# have any open tcp/udp sockets
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
