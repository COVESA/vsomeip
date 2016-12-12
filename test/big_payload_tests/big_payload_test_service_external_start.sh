#!/bin/bash
# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if [[ $# -gt 0 && $1 != "RANDOM" && $1 != "LIMITED" ]]
then
    echo "The only allowed parameter to this script is RANDOM or LIMITED."
    echo "Like $0 RANDOM"
    exit 1
fi

# Start the service
if [[ $# -gt 0 && $1 == "RANDOM" ]]
then
    export VSOMEIP_CONFIGURATION=big_payload_test_tcp_service_random.json
else
    export VSOMEIP_CONFIGURATION=big_payload_test_tcp_service.json
fi
./big_payload_test_service $1
