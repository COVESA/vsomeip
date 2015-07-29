#!/bin/bash
# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Purpose: This script is needed to start the client and service with
# one command. This is necessary as ctest - which is used to run the
# tests - isn't able to start two binaries for one testcase. Therefore
# the testcase simply executes this script. This script then runs client
# and service and checks that both exit sucessfully.

FAIL=0

# Parameter 1: the pid to check
# Parameter 2: number of TCP/UDP sockets the process should have open
check_tcp_udp_sockets_are_open ()
{
    # Check that the passed pid/process does listen on at least one TCP/UDP socket 
    # awk is used to avoid the case when a inode number is the same as a PID. The awk
    # program filters the netstat output down to the protocol (1st field) and 
    # the PID/Program name (last field) fields.
    SERVICE_SOCKETS_LISTENING=$(netstat -tulpen 2> /dev/null | awk '{print $1 "\t"  $NF}' | grep $1 | wc -l)
    if [ $SERVICE_SOCKETS_LISTENING -lt $2 ]
    then
        ((FAIL+=1))
    fi
}

# Parameter 1: the pid to check
check_tcp_udp_sockets_are_closed ()
{
    # Check that the passed pid/process does not listen on any TCP/UDP socket 
    # or has any active connection via a TCP/UDP socket
    # awk is used to avoid the case when a inode number is the same as a PID. The awk
    # program filters the netstat output down to the protocol (1st field) and 
    # the PID/Program name (last field) fields.
    SERVICE_SOCKETS_LISTENING=$(netstat -tulpen 2> /dev/null | awk '{print $1 "\t"  $NF}' | grep $1 | wc -l)
    if [ $SERVICE_SOCKETS_LISTENING -ne 0 ]
    then
        ((FAIL+=1))
    fi

    SERVICE_SOCKETS_CONNECTED=$(netstat -tupen 2> /dev/null | awk '{print $1 "\t"  $NF}' | grep $1 | wc -l)
    if [ $SERVICE_SOCKETS_CONNECTED -ne 0 ]
    then
        ((FAIL+=1))
    fi
}

# Display a message to show the user that he must now call the external service
# to finish the test successfully
cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** magic_cookies_test_service_start.sh
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** vsomeip-magic-cookies-client.json and
** vsomeip-magic-cookies-service.json to your personal setup.
** 
** As soon as the service is started please press any key to continue this test
*******************************************************************************
*******************************************************************************
End-of-message
read

# Start the client for magic-cookies test
export VSOMEIP_APPLICATION_NAME=client-sample
export VSOMEIP_CONFIGURATION_FILE=vsomeip-magic-cookies-client.json
./magic-cookies-test-client &
CLIENT_PID=$!

# Wait until client is finished
for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || ((FAIL+=1))
done

# Check if server exited sucessfully
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
