#!/bin/bash
# Copyright (C) 2015-2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FAIL=0

if [ $# -lt 1 ]
then
    echo "Please pass a test mode to this script."
    echo "For example: $0 IN_SEQUENCE"
    echo "Valid subscription types include:"
    echo "            [IN_SEQUENCE, MIXED, INCOMPLETE, DUPLICATE, OVERLAP, OVERLAP_FRONT_BACK]"
    exit 1
fi
TESTMODE=$1
export VSOMEIP_CONFIGURATION=someip_tp_test_master.json
# start daemon
../examples/routingmanagerd/./routingmanagerd &
PID_VSOMEIPD=$!
# Start the services
./someip_tp_test_service $1 &
PID_SERIVCE=$!

sleep 1

if [ ! -z "$USE_LXC_TEST" ]; then
    echo "Waiting for 5s"
    sleep 5
    ssh -tt -i $SANDBOX_ROOT_DIR/commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub -o StrictHostKeyChecking=no root@$LXC_TEST_SLAVE_IP "bash -ci \"set -m; cd \\\$SANDBOX_TARGET_DIR/vsomeip_lib/test; ./someip_tp_test_msg_sender XXX.XXX.XXX.XXX XXX.XXX.XXX.XXX $TESTMODE\"" &
    echo "remote ssh pid: $!"
elif [ ! -z "$USE_DOCKER" ]; then
    echo "Waiting for 5s"
    sleep 5
    docker exec $DOCKER_IMAGE sh -c "cd $DOCKER_TESTS && ./someip_tp_test_msg_sender XXX.XXX.XXX.XXX XXX.XXX.XXX.XXX $TESTMODE" &
else
cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** someip_tp_test_msg_sender XXX.XXX.XXX.XXX XXX.XXX.XXX.XXX $TESTMODE
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** someip_tp_test_master.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message
fi

# Wait until all clients and services are finished
for job in $PID_SERIVCE
do
    # Fail gets incremented if a client exits with a non-zero exit code
    echo "waiting for $job"
    wait $job || FAIL=$(($FAIL+1))
done

# kill the services
kill $PID_VSOMEIPD
sleep 1

# Check if everything went well
exit $FAIL
