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

export VSOMEIP_CONFIGURATION=debug_diag_job_plugin_test_master.json
# start daemon
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(readlink -f ../plugins/mgu)
../daemon/./vsomeipd &
PID_VSOMEIPD=$!
# Start the services
# Array for service pids
SERVICE_PIDS=()
./debug_diag_job_plugin_test_service 1 &
SERVICE_PIDS+=($!)
./debug_diag_job_plugin_test_service 2 &
SERVICE_PIDS+=($!)

print_starter_message () {

if [ ! -z "$USE_LXC_TEST" ]; then
    echo "starting initial event test on slave LXC with params $PASSED_SUBSCRIPTION_TYPE $CLIENT_JSON_FILE $REMAINING_OPTIONS"
    ssh -tt -i $SANDBOX_ROOT_DIR/commonapi_main/lxc-config/.ssh/mgc_lxc/rsa_key_file.pub -o StrictHostKeyChecking=no root@$LXC_TEST_SLAVE_IP "bash -ci \"set -m; cd \\\$SANDBOX_TARGET_DIR/vsomeip/test; ./debug_diag_job_plugin_test_slave_starter.sh\"" &
elif [ ! -z "$USE_DOCKER" ]; then
    docker run --name ietms --cap-add NET_ADMIN $DOCKER_IMAGE sh -c "route add -net 224.0.0.0/4 dev eth0 && cd $DOCKER_TESTS && ./debug_diag_job_plugin_test_slave_starter.sh" &
else
cat <<End-of-message
*******************************************************************************
*******************************************************************************
** Please now run:
** debug_diag_job_plugin_test_slave_starter.sh
** from an external host to successfully complete this test.
**
** You probably will need to adapt the 'unicast' settings in
** debug_diag_job_plugin_test_master.json and
** debug_diag_job_plugin_test_slave.json to your personal setup.
*******************************************************************************
*******************************************************************************
End-of-message
fi
}
sleep 1
print_starter_message

FAIL=0
# Wait until all clients are finished
for job in ${SERVICE_PIDS[*]}
do
    # Fail gets incremented if a client exits with a non-zero exit code
    wait $job || FAIL=$(($FAIL+1))
done

kill $PID_VSOMEIPD

echo ""

if [ ! -z "$USE_DOCKER" ]; then
    docker stop ietms
    docker rm ietms
fi

# Check if both exited successfully 
exit $FAIL
