// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/defines.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/message_impl.hpp"
#include "../../utility/include/byteorder.hpp"

namespace vsomeip {

message_impl::message_impl()
    : payload_(runtime::get()->create_payload()) {
}

message_impl::~message_impl() {
}

length_t message_impl::get_length() const {
    return (VSOMEIP_SOMEIP_HEADER_SIZE
            + (payload_ ? payload_->get_length() : 0));
}

std::shared_ptr< payload > message_impl::get_payload() const {
    return payload_;
}

void message_impl::set_payload(std::shared_ptr< payload > _payload) {
    payload_ = _payload;
}

bool message_impl::serialize(serializer *_to) const {
    return (header_.serialize(_to)
            && (payload_ ? payload_->serialize(_to) : true));
}

bool message_impl::deserialize(deserializer *_from) {
    payload_ = runtime::get()->create_payload();
    bool is_successful = header_.deserialize(_from);
    if (is_successful) {
        payload_->set_capacity(header_.length_ - VSOMEIP_SOMEIP_HEADER_SIZE);
        is_successful = payload_->deserialize(_from);
    }
    return is_successful;
}

} // namespace vsomeip
