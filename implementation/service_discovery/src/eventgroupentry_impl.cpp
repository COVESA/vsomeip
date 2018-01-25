// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/constants.hpp"
#include "../include/eventgroupentry_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"
#include "../include/ipv4_option_impl.hpp"
#include "../include/ipv6_option_impl.hpp"

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

bool eventgroupentry_impl::is_matching_subscribe(
        const eventgroupentry_impl& _other,
        const message_impl::options_t& _options) const {
    if (ttl_ == 0
            && _other.ttl_ > 0
            && service_ == _other.service_
            && instance_ == _other.instance_
            && eventgroup_ == _other.eventgroup_
            && index1_ == _other.index1_
            && index2_ == _other.index2_
            && num_options_[0] == _other.num_options_[0]
            && num_options_[1] == _other.num_options_[1]
            && major_version_ == _other.major_version_
            && counter_ == _other.counter_) {
        return true;
    } else if (ttl_ == 0
            && _other.ttl_ > 0
            && service_ == _other.service_
            && instance_ == _other.instance_
            && eventgroup_ == _other.eventgroup_
            && major_version_ == _other.major_version_
            && counter_ == _other.counter_) {
        // check if entries reference options at different indexes but the
        // options itself are identical
        // check if number of options referenced is the same
        if (num_options_[0] + num_options_[1]
                != _other.num_options_[0] + _other.num_options_[1] ||
                num_options_[0] + num_options_[1] == 0) {
            return false;
        }
        // read out ip options of current and _other
        std::vector<std::shared_ptr<ip_option_impl>> its_options_current;
        std::vector<std::shared_ptr<ip_option_impl>> its_options_other;
        for (const auto option_run : {0,1}) {
            for (const auto option_index : options_[option_run]) {
                switch (_options[option_index]->get_type()) {
                    case option_type_e::IP4_ENDPOINT:
                        its_options_current.push_back(
                                std::static_pointer_cast<ipv4_option_impl>(
                                        _options[option_index]));
                        break;
                    case option_type_e::IP6_ENDPOINT:
                        its_options_current.push_back(
                                std::static_pointer_cast<ipv6_option_impl>(
                                        _options[option_index]));
                        break;
                    default:
                        break;
                }
            }
            for (const auto option_index : _other.options_[option_run]) {
                switch (_options[option_index]->get_type()) {
                    case option_type_e::IP4_ENDPOINT:
                        its_options_other.push_back(
                                std::static_pointer_cast<ipv4_option_impl>(
                                        _options[option_index]));
                        break;
                    case option_type_e::IP6_ENDPOINT:
                        its_options_other.push_back(
                                std::static_pointer_cast<ipv6_option_impl>(
                                        _options[option_index]));
                        break;
                    default:
                        break;
                }
            }
        }

        if (!its_options_current.size() || !its_options_other.size()) {
            return false;
        }

        // search every option of current in other
        for (const auto& c : its_options_current) {
            bool found(false);
            for (const auto& o : its_options_other) {
                if (*c == *o) {
                    switch (c->get_type()) {
                        case option_type_e::IP4_ENDPOINT:
                            if (static_cast<ipv4_option_impl*>(c.get())->get_address()
                                    == static_cast<ipv4_option_impl*>(o.get())->get_address()) {
                                found = true;
                            }
                            break;
                        case option_type_e::IP6_ENDPOINT:
                            if (static_cast<ipv6_option_impl*>(c.get())->get_address()
                                    == static_cast<ipv6_option_impl*>(o.get())->get_address()) {
                                found = true;
                            }
                            break;
                        default:
                            break;
                    }
                }
                if (found) {
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }
    return false;
}

void eventgroupentry_impl::add_target(
        const std::shared_ptr<endpoint_definition> &_target) {
    if (_target->is_reliable()) {
        target_reliable_ = _target;
    } else {
        target_unreliable_ = _target;
    }
}

std::shared_ptr<endpoint_definition> eventgroupentry_impl::get_target(
        bool _reliable) const {
    return _reliable ? target_reliable_ : target_unreliable_;
}

} // namespace sd
} // namespace vsomeip
