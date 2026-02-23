// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "service_state.hpp"
#include <vector>
#include <optional>

namespace vsomeip_v3::testing {

struct availability_checker {

    [[nodiscard]] bool operator==(service_availability const& _availability) const {
        if (service_instance_ && service_instance_ != _availability.si_) {
            return false;
        }
        if (availability_state_ && availability_state_ != _availability.state_) {
            return false;
        }
        return true;
    }
    [[nodiscard]] bool operator!=(service_availability const& _availability) const { return !(*this == _availability); }
    [[nodiscard]] bool operator()(std::vector<service_availability> const& _availabilities) const {
        return std::any_of(_availabilities.begin(), _availabilities.end(), [this](auto const& _rhs) { return *this == _rhs; });
    }

    std::optional<service_instance> service_instance_{};
    std::optional<availability_state_e> availability_state_{};
};
}
