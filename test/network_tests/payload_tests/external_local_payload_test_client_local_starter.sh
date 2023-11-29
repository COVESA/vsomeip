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

# Start the service
export VSOMEIP_APPLICATION_NAME=external_local_payload_test_service
export VSOMEIP_CONFIGURATION=external_local_payload_test_service.json
./payload_test_service &
SERIVCE_PID=$!
sleep 1;

# The service should listen on a TCP and UDP socket now
check_tcp_udp_sockets_are_open $SERIVCE_PID 2

# Start the client
export VSOMEIP_APPLICATION_NAME=external_local_payload_test_client_local
export VSOMEIP_CONFIGURATION=external_local_payload_test_client_local.json
./payload_test_client &
CLIENT_PID=$!

# The service should still listen on a TCP and UDP socket now
check_tcp_udp_sockets_are_open  $SERIVCE_PID 2
# The client should use the shortcut over a local UDS instead of TCP/UDP,
# therefore he shouldn't have any open TCP/UDP sockets
check_tcp_udp_sockets_are_closed  $CLIENT_PID

if [ ! -z "$USE_DOCKER" ]; then
    FAIL=0
fi

# Wait until client and service are finished
for job in $(jobs -p)
do
    # Fail gets incremented if either client or service exit
    # with a non-zero exit code
    wait $job || ((FAIL+=1))
done

# Check if client and server both exited sucessfully
if [ $FAIL -eq 0 ]
then
    exit 0
else
    exit 1
fi
