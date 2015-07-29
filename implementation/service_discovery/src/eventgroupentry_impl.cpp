// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/constants.hpp"
#include "../include/eventgroupentry_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

eventgroupentry_impl::eventgroupentry_impl() {
    eventgroup_ = 0xFFFF;
}

eventgroupentry_impl::eventgroupentry_impl(const eventgroupentry_impl &_entry)
        : entry_impl(_entry) {
    eventgroup_ = _entry.eventgroup_;
}

eventgroupentry_impl::~eventgroupentry_impl() {
}

eventgroup_t eventgroupentry_impl::get_eventgroup() const {
    return eventgroup_;
}

void eventgroupentry_impl::set_eventgroup(eventgroup_t _eventgroup) {
    eventgroup_ = _eventgroup;
}

bool eventgroupentry_impl::serialize(vsomeip::serializer *_to) const {
    bool is_successful = entry_impl::serialize(_to);

    is_successful = is_successful && _to->serialize(protocol::reserved_byte);

    is_successful = is_successful
            && _to->serialize(static_cast<uint32_t>(ttl_), true);

    is_successful = is_successful && _to->serialize(protocol::reserved_word);

    is_successful = is_successful
            && _to->serialize(static_cast<uint16_t>(eventgroup_));

    return is_successful;
}

bool eventgroupentry_impl::deserialize(vsomeip::deserializer *_from) {
    bool is_successful = entry_impl::deserialize(_from);

    uint8_t its_reserved0;
    is_successful = is_successful && _from->deserialize(its_reserved0);

    uint32_t its_ttl;
    is_successful = is_successful && _from->deserialize(its_ttl, true);
    ttl_ = static_cast<ttl_t>(its_ttl);

    uint16_t its_reserved1;
    is_successful = is_successful && _from->deserialize(its_reserved1);

    uint16_t its_eventgroup = 0;
    is_successful = is_successful && _from->deserialize(its_eventgroup);
    eventgroup_ = static_cast<eventgroup_t>(its_eventgroup);

    return is_successful;
}

} // namespace sd
} // namespace vsomeip
