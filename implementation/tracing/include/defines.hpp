// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#define VSOMEIP_TC_DEFAULT_CHANNEL_NAME             "Trace Connector Network Logging"
#define VSOMEIP_TC_DEFAULT_FILTER_TYPE              "positive"

// Payloads larger than this (in bytes) are logged header-only unless an
// explicit positive filter forces full logging. 0 disables the threshold.
#define VSOMEIP_TC_DEFAULT_FULL_LOGGING_THRESHOLD   2048

#define VSOMEIP_TC_INSTANCE_POS_MIN                 8
#define VSOMEIP_TC_INSTANCE_POS_MAX                 9
