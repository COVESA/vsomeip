// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/policy_manager_impl.hpp"

namespace vsomeip_v3 {

std::shared_ptr<policy_manager>
policy_manager::get() {
    static std::shared_ptr<policy_manager> the_policy_manager
        = std::make_shared<policy_manager_impl>();
    return the_policy_manager;
}

} // namespace vsomeip_v3
