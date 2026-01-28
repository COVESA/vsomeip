// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "simple_command.hpp"

namespace vsomeip_v3 {
namespace protocol {

class deregister_application_command : public simple_command {

public:
    deregister_application_command();
};

} // namespace protocol
} // namespace vsomeip_v3
