// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {
namespace cfg {

struct service_instance_range {
    service_t first_service_;
    service_t last_service_;
    instance_t first_instance_;
    instance_t last_instance_;
};

} // namespace cfg
} // namespace vsomeip_v3
