#!/bin/bash
# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

export VSOMEIP_APPLICATION_NAME=service-sample
export VSOMEIP_CONFIGURATION_FILE=vsomeip-magic-cookies-service.json
.././response-sample --tcp --static-routing
