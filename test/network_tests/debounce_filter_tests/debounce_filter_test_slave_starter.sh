#!/bin/bash
# Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FAIL=0

# Cleanup
rm -f /tmp/vsomeip*

# Exclude external libraries from Thread Sanitizer
export TSAN_OPTIONS="suppressions=tsan-suppressions.txt"

export VSOMEIP_CONFIGURATION=debounce_filter_test_service.json
../../examples/routingmanagerd/routingmanagerd &
PID_VSOMEIPD=$!

sleep 1

./debounce_filter_test_service &
PID_SLAVE=$!

# Wait until all slaves are finished
for job in $PID_SLAVE
do
    # Fail gets incremented if a client exits with a non-zero exit code
    echo "waiting for $job"
    wait $job || FAIL=$(($FAIL+1))
done

# kill the services
kill $PID_VSOMEIPD
sleep 3

# Check if everything went well
exit $FAIL
