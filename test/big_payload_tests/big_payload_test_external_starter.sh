#!/bin/bash
# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Purpose: This script is needed to start the client and service with
# one command. This is necessary as ctest - which is used to run the
# tests - isn't able to start two binaries for one testcase. Therefore
# the testcase simply executes this script. This script then runs client
# and service and checks that both exit successfully.

FAIL=0

# Start the client
export VSOMEIP_CONFIGURATION=big_payload_test_tcp_client.json
export VSOMEIP_APPLICATION_NAME=big_payload_test_client
./big_payload_test_client &

cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** big_payload_test_service_external_start.sh
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** big_payload_test_tcp_service.json and
** big_payload_test_tcp_client.json to your personal setup.
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

# Check if client and server both exited successfully 
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
