// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <set>

#include <vsomeip/enumeration_types.hpp>

#include "command.hpp"

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif

namespace vsomeip_v3 {
namespace protocol {

class unregister_event_command : public command {
public:
    unregister_event_command();

    void serialize(std::vector<byte_t>& _buffer) const;
    void deserialize(const std::vector<byte_t>& _buffer, error_e& _error);

    // specific
    service_t get_service() const;
    void set_service(service_t _service);

    instance_t get_instance() const;
    void set_instance(instance_t _instance);

    event_t get_event() const;
    void set_event(event_t _event);

    bool is_provided() const;
    void set_provided(bool _is_provided);

private:
    service_t service_;
    instance_t instance_;
    event_t event_;
    bool is_provided_;
};

} // namespace protocol
} // namespace vsomeip_v3
