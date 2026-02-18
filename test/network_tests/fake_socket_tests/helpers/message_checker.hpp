// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "service_state.hpp"
#include <vector>
#include <optional>

namespace vsomeip_v3::testing {

struct message_checker {

    [[nodiscard]] bool operator==(message const& _message) const {
        if (client_session_ && client_session_ != _message.client_session_) {
            return false;
        }
        if (service_instance_ && service_instance_ != _message.service_instance_) {
            return false;
        }
        if (method_ && method_ != _message.method_) {
            return false;
        }
        if (message_type_ && message_type_ != _message.message_type_) {
            return false;
        }
        if (payload_ && payload_ != _message.payload_) {
            return false;
        }
        return true;
    }
    [[nodiscard]] bool operator!=(message const& _message) const { return !(*this == _message); }
    [[nodiscard]] bool operator()(std::vector<message> const& _messages) const {
        return std::any_of(_messages.begin(), _messages.end(), [this](auto const& _rhs) { return *this == _rhs; });
    }

    std::optional<client_session> client_session_;
    std::optional<service_instance> service_instance_{};
    std::optional<vsomeip::method_t> method_{};
    std::optional<vsomeip::message_type_e> message_type_{};

    std::optional<std::vector<unsigned char>> payload_{};
};
}
