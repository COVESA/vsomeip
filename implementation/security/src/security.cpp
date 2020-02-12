// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/security_impl.hpp"

namespace vsomeip_v3 {

std::shared_ptr<security>
security::get() {
    return security_impl::get();
}

} // namespace vsomeip_v3
