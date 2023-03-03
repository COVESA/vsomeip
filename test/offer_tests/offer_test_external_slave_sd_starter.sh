#!/bin/bash
# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FAIL=0
# Rejecting offer for which there is already a remote offer:
# * start daemon
# * start application which offers service
# * start daemon remotely
# * start same application which offers the same service again remotely
#   -> should be rejected as there is already a service instance
#   running in the network

export VSOMEIP_CONFIGURATION=offer_test_external_slave.json
# start daemon
../examples/routingmanagerd/./routingmanagerd &
PID_VSOMEIPD=$!

echo "calling availabiliy checker"

./offer_test_service_availability_checker &

PID_AVAILABILITY_CHECKER=$!

echo "waiting for offer_test_service_availability_checker"

# wait until the services on the remote node were started as well
wait $PID_AVAILABILITY_CHECKER

# kill the routing manager services
kill $PID_VSOMEIPD

./offer_test_external_sd_msg_sender $1 &



