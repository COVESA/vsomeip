// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/error.hpp>

namespace vsomeip {

const char *ERROR_INFO[] = { "Missing vsomeip configuration",
        "Missing port configuration", "Client endpoint creation failed",
        "Server endpoint creation failed", "Service property mismatch" };

} // namespace vsomeip
