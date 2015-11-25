// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/configuration_impl.hpp"

namespace vsomeip {

std::shared_ptr<configuration> configuration::get(
        const std::set<std::string> &_input) {
    return cfg::configuration_impl::get(_input);
}

void configuration::reset() {
    cfg::configuration_impl::reset();
}

} // namespace vsomeip
