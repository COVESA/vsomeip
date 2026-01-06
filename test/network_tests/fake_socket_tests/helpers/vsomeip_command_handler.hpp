// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_TESTING_VSOMEIP_COMMAND_HANDLER_
#define VSOMEIP_V3_TESTING_VSOMEIP_COMMAND_HANDLER_

#include "command_message.hpp"
#include <functional>

namespace vsomeip_v3::testing {

using vsomeip_command_handler = std::function<bool(command_message const&)>;

}

#endif
