// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

#include "subscribe_command_base.hpp"

namespace vsomeip_v3 {

struct debounce_filter_impl_t;

namespace protocol {

class subscribe_command : public subscribe_command_base {

public:
    subscribe_command();

    std::shared_ptr<debounce_filter_impl_t> get_filter() const;
    void set_filter(const std::shared_ptr<debounce_filter_impl_t>& _filter);

    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);

private:
    std::shared_ptr<debounce_filter_impl_t> filter_;
};

} // namespace protocol
} // namespace vsomeip_v3
