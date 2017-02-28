// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/constants.hpp"
#include "../include/eventgroupentry_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

eventgroupentry_impl::eventgroupentry_impl() :
    reserved_(0) {
    eventgroup_ = 0xFFFF;
    counter_ = 0;
}

eventgroupentry_impl::eventgroupentry_impl(const eventgroupentry_impl &_entry)
        : entry_impl(_entry),
          reserved_(0) {
    eventgroup_ = _entry.eventgroup_;
    counter_ = _entry.counter_;
}

eventgroupentry_impl::~eventgroupentry_impl() {
}

eventgroup_t eventgroupentry_impl::get_eventgroup() const {
    return eventgroup_;
}

void eventgroupentry_impl::set_eventgroup(eventgroup_t _eventgroup) {
    eventgroup_ = _eventgroup;
}

uint16_t eventgroupentry_impl::get_reserved() const {
    return reserved_;
}

void eventgroupentry_impl::set_reserved(uint16_t _reserved) {
    reserved_ = _reserved;
}

uint8_t eventgroupentry_impl::get_counter() const {
    return counter_;
}

void eventgroupentry_impl::set_counter(uint8_t _counter) {
    counter_ = _counter;
}

bool eventgroupentry_impl::serialize(vsomeip::serializer *_to) const {
    bool is_successful = entry_impl::serialize(_to);

    is_successful = is_successful && _to->serialize(major_version_);

    is_successful = is_successful
            && _to->serialize(static_cast<uint32_t>(ttl_), true);

    // 4Bit only for counter field
    if (counter_ >= 16) {
        is_successful = false;
    }
    uint16_t counter_and_reserved = protocol::reserved_word;
    if (!reserved_ ) {
        //reserved was not set -> just store counter as uint16
        counter_and_reserved = static_cast<uint16_t>(counter_);
    }
    else {
        //reserved contains values -> put reserved and counter into 16 bit variable
        counter_and_reserved = (uint16_t) (((uint16_t) reserved_ << 4) | counter_);
    }

    is_successful = is_successful
            && _to->serialize((uint8_t)(counter_and_reserved >> 8)); // serialize reserved part 1
    is_successful = is_successful
            && _to->serialize((uint8_t)counter_and_reserved); // serialize reserved part 2 and counter
    is_successful = is_successful
            && _to->serialize(static_cast<uint16_t>(eventgroup_));

    return is_successful;
}

bool eventgroupentry_impl::deserialize(vsomeip::deserializer *_from) {
    bool is_successful = entry_impl::deserialize(_from);

    uint8_t tmp_major_version(0);
    is_successful = is_successful && _from->deserialize(tmp_major_version);
    major_version_ = static_cast<major_version_t>(tmp_major_version);

    uint32_t its_ttl(0);
    is_successful = is_successful && _from->deserialize(its_ttl, true);
    ttl_ = static_cast<ttl_t>(its_ttl);

    uint8_t reserved1(0), reserved2(0);
    is_successful = is_successful && _from->deserialize(reserved1); // deserialize reserved part 1
    is_successful = is_successful && _from->deserialize(reserved2); // deserialize reserved part 2 and counter

    reserved_ = (uint16_t) (((uint16_t)reserved1 << 8) | reserved2); // combine reserved parts and counter
    reserved_ = (uint16_t) (reserved_ >> 4);  //remove counter from reserved field

    //set 4 bits of reserved part 2 field to zero
    counter_ = (uint8_t) (reserved2 & (~(0xF0)));

    // 4Bit only for counter field
    if (counter_ >= 16) {
        is_successful = false;
    }
    uint16_t its_eventgroup = 0;
    is_successful = is_successful && _from->deserialize(its_eventgroup);
    eventgroup_ = static_cast<eventgroup_t>(its_eventgroup);

    return is_successful;
}

} // namespace sd
} // namespace vsomeip
