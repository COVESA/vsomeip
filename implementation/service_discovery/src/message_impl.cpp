// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <typeinfo>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>

#include "../include/constants.hpp"
#include "../include/defines.hpp"
#include "../include/eventgroupentry_impl.hpp"
#include "../include/serviceentry_impl.hpp"
#include "../include/configuration_option_impl.hpp"
#include "../include/ipv4_option_impl.hpp"
#include "../include/ipv6_option_impl.hpp"
#include "../include/load_balancing_option_impl.hpp"
#include "../include/protection_option_impl.hpp"
#include "../include/message_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/payload_impl.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

message_impl::message_impl() :
    flags_(0x0),
    options_length_(0x0),
    number_required_acks_(0x0),
    number_contained_acks_(0x0),
    initial_events_required_(false) {
    header_.service_ = 0xFFFF;
    header_.method_ = 0x8100;
    header_.protocol_version_ = 0x01;
}

message_impl::~message_impl() {
}

length_t message_impl::get_length() const {
    length_t current_length = VSOMEIP_SOMEIP_SD_DATA_SIZE;
    if( entries_.size()) {
        current_length += VSOMEIP_SOMEIP_SD_ENTRY_LENGTH_SIZE;
        current_length += uint32_t(entries_.size() * VSOMEIP_SOMEIP_SD_ENTRY_SIZE);
    }

    current_length += VSOMEIP_SOMEIP_SD_OPTION_LENGTH_SIZE;
    if(options_.size()) {
        for (size_t i = 0; i < options_.size(); ++i) {
            current_length += (options_[i]->get_length()
                    + VSOMEIP_SOMEIP_SD_OPTION_HEADER_SIZE);
        }
    }
    return current_length;
}

#define VSOMEIP_REBOOT_FLAG 0x80

bool message_impl::get_reboot_flag() const {
    return ((flags_ & VSOMEIP_REBOOT_FLAG) != 0);
}

void message_impl::set_reboot_flag(bool _is_set) {
    if (_is_set)
        flags_ |= flags_t(VSOMEIP_REBOOT_FLAG);
    else
        flags_ &= flags_t(~VSOMEIP_REBOOT_FLAG);
}

#define VSOMEIP_UNICAST_FLAG 0x40

bool message_impl::get_unicast_flag() const {
    return ((flags_ & VSOMEIP_UNICAST_FLAG) != 0);
}

void message_impl::set_unicast_flag(bool _is_set) {
    if (_is_set)
        flags_ |= flags_t(VSOMEIP_UNICAST_FLAG);
    else
        flags_ &= flags_t(~VSOMEIP_UNICAST_FLAG);
}

void message_impl::set_length(length_t _length) {
    (void)_length;
}

std::shared_ptr<eventgroupentry_impl> message_impl::create_eventgroup_entry() {
    std::shared_ptr < eventgroupentry_impl
            > its_entry(std::make_shared<eventgroupentry_impl>());
    //TODO: throw OutOfMemoryException if allocation fails
    its_entry->set_owning_message(this);
    entries_.push_back(its_entry);
    return its_entry;
}

std::shared_ptr<serviceentry_impl> message_impl::create_service_entry() {
    std::shared_ptr < serviceentry_impl
            > its_entry(std::make_shared<serviceentry_impl>());
    //TODO: throw OutOfMemoryException if allocation fails
    its_entry->set_owning_message(this);
    entries_.push_back(its_entry);
    return its_entry;
}

std::shared_ptr<configuration_option_impl> message_impl::create_configuration_option() {
    std::shared_ptr < configuration_option_impl
            > its_option(std::make_shared<configuration_option_impl>());
    //TODO: throw OutOfMemoryException if allocation fails
    its_option->set_owning_message(this);
    options_.push_back(its_option);
    return its_option;
}

std::shared_ptr<ipv4_option_impl> message_impl::create_ipv4_option(
        bool _is_multicast) {
    std::shared_ptr < ipv4_option_impl
            > its_option(std::make_shared < ipv4_option_impl > (_is_multicast));
    //TODO: throw OutOfMemoryException if allocation fails
    its_option->set_owning_message(this);
    options_.push_back(its_option);
    return its_option;
}

std::shared_ptr<ipv6_option_impl> message_impl::create_ipv6_option(
        bool _is_multicast) {
    std::shared_ptr < ipv6_option_impl
            > its_option(std::make_shared < ipv6_option_impl > (_is_multicast));
    //TODO: throw OutOfMemoryException if allocation fails
    its_option->set_owning_message(this);
    options_.push_back(its_option);
    return its_option;
}

std::shared_ptr<load_balancing_option_impl> message_impl::create_load_balancing_option() {
    std::shared_ptr < load_balancing_option_impl
            > its_option(std::make_shared<load_balancing_option_impl>());
    //TODO: throw OutOfMemoryException if allocation fails
    its_option->set_owning_message(this);
    options_.push_back(its_option);
    return its_option;
}

std::shared_ptr<protection_option_impl> message_impl::create_protection_option() {
    std::shared_ptr < protection_option_impl
            > its_option(std::make_shared<protection_option_impl>());
    //TODO: throw OutOfMemoryException if allocation fails
    its_option->set_owning_message(this);
    options_.push_back(its_option);
    return its_option;
}

const message_impl::entries_t & message_impl::get_entries() const {
    return entries_;
}

const message_impl::options_t & message_impl::get_options() const {
    return options_;
}

// TODO: throw exception to signal "OptionNotFound"
int16_t message_impl::get_option_index(
        const std::shared_ptr<option_impl> &_option) const {
    int16_t i = 0;

    while (i < int16_t(options_.size())) {
        if (options_[i] == _option)
            return i;
        i++;
    }

    return -1;
}

uint32_t message_impl::get_options_length() {
    return options_length_;
}

std::shared_ptr<payload> message_impl::get_payload() const {
    return std::make_shared<payload_impl>();
}

void message_impl::set_payload(std::shared_ptr<payload> _payload) {
    (void)_payload;
}

bool message_impl::serialize(vsomeip::serializer *_to) const {
    bool is_successful = header_.serialize(_to);

    is_successful = is_successful && _to->serialize(flags_);
    is_successful = is_successful
            && _to->serialize(protocol::reserved_long, true);

    uint32_t entries_length = uint32_t(entries_.size() * VSOMEIP_SOMEIP_SD_ENTRY_SIZE);
    is_successful = is_successful && _to->serialize(entries_length);

    for (auto it = entries_.begin(); it != entries_.end(); ++it)
        is_successful = is_successful && (*it)->serialize(_to);

    uint32_t options_length = 0;
    for (auto its_option : options_)
        options_length += its_option->get_length()
                + VSOMEIP_SOMEIP_SD_OPTION_HEADER_SIZE;
    is_successful = is_successful && _to->serialize(options_length);

    for (auto its_option : options_)
        is_successful = is_successful && its_option->serialize(_to);

    return is_successful;
}

bool message_impl::deserialize(vsomeip::deserializer *_from) {
    bool is_successful;
    bool option_is_successful(true);

    // header
    is_successful = header_.deserialize(_from);

    // flags
    is_successful = is_successful && _from->deserialize(flags_);

    // reserved
    uint32_t reserved;
    is_successful = is_successful && _from->deserialize(reserved, true);

    // entries
    uint32_t entries_length = 0;
    is_successful = is_successful && _from->deserialize(entries_length);

    // backup the current remaining length
    uint32_t save_remaining = uint32_t(_from->get_remaining());

    // set remaining bytes to length of entries array
    _from->set_remaining(entries_length);

    // deserialize the entries
    while (is_successful && _from->get_remaining()) {
        std::shared_ptr < entry_impl > its_entry(deserialize_entry(_from));
        if (its_entry) {
            entries_.push_back(its_entry);
            if (its_entry->get_type() == entry_type_e::SUBSCRIBE_EVENTGROUP
                    && its_entry->get_ttl() > 0) {
                const std::uint8_t num_options =
                        static_cast<std::uint8_t>(
                                its_entry->get_num_options(1) +
                                its_entry->get_num_options(2));
                number_required_acks_ =
                        static_cast<std::uint8_t>(number_required_acks_
                                + num_options);
            }
        } else {
            is_successful = false;
        }
    }

    // set length to remaining bytes after entries array
    _from->set_remaining(save_remaining - entries_length);

    // Don't try to deserialize options if there aren't any
    if(_from->get_remaining() == 0) {
        return is_successful;
    }

    // deserialize the options
    is_successful = is_successful && _from->deserialize(options_length_);

    // check if there is unreferenced data behind the last option and discard it
    if(_from->get_remaining() > options_length_) {
        _from->set_remaining(options_length_);
    }

    while (option_is_successful && _from->get_remaining()) {
        std::shared_ptr < option_impl > its_option(deserialize_option(_from));
        if (its_option) {
            options_.push_back(its_option);
        }  else {
            option_is_successful = false;
        }
    }

    return is_successful;
}

entry_impl * message_impl::deserialize_entry(vsomeip::deserializer *_from) {
    entry_impl *deserialized_entry = 0;
    uint8_t tmp_entry_type;

    if (_from->look_ahead(0, tmp_entry_type)) {
        entry_type_e deserialized_entry_type =
                static_cast<entry_type_e>(tmp_entry_type);

        switch (deserialized_entry_type) {
        case entry_type_e::FIND_SERVICE:
        case entry_type_e::OFFER_SERVICE:
            //case entry_type_e::STOP_OFFER_SERVICE:
        case entry_type_e::REQUEST_SERVICE:
            deserialized_entry = new serviceentry_impl;
            break;

        case entry_type_e::FIND_EVENT_GROUP:
        case entry_type_e::PUBLISH_EVENTGROUP:
            //case entry_type_e::STOP_PUBLISH_EVENTGROUP:
        case entry_type_e::SUBSCRIBE_EVENTGROUP:
            //case entry_type_e::STOP_SUBSCRIBE_EVENTGROUP:
        case entry_type_e::SUBSCRIBE_EVENTGROUP_ACK:
            //case entry_type_e::STOP_SUBSCRIBE_EVENTGROUP_ACK:
            deserialized_entry = new eventgroupentry_impl;
            break;

        default:
            break;
        };

        // deserialize object
        if (0 != deserialized_entry) {
            deserialized_entry->set_owning_message(this);
            if (!deserialized_entry->deserialize(_from)) {
                delete deserialized_entry;
                deserialized_entry = 0;
            };
        }
    }

    return deserialized_entry;
}

option_impl * message_impl::deserialize_option(vsomeip::deserializer *_from) {
    option_impl *deserialized_option = 0;
    uint8_t tmp_option_type;

    if (_from->look_ahead(2, tmp_option_type)) {

        option_type_e deserialized_option_type =
                static_cast<option_type_e>(tmp_option_type);

        switch (deserialized_option_type) {

        case option_type_e::CONFIGURATION:
            deserialized_option = new configuration_option_impl;
            break;
        case option_type_e::LOAD_BALANCING:
            deserialized_option = new load_balancing_option_impl;
            break;
        case option_type_e::PROTECTION:
            deserialized_option = new protection_option_impl;
            break;
        case option_type_e::IP4_ENDPOINT:
            deserialized_option = new ipv4_option_impl(false);
            break;
        case option_type_e::IP4_MULTICAST:
            deserialized_option = new ipv4_option_impl(true);
            break;
        case option_type_e::IP6_ENDPOINT:
            deserialized_option = new ipv6_option_impl(false);
            break;
        case option_type_e::IP6_MULTICAST:
            deserialized_option = new ipv6_option_impl(true);
            break;

        default:
            deserialized_option = new option_impl();
            break;
        };

        // deserialize object
        if (0 != deserialized_option
                && !deserialized_option->deserialize(_from)) {
            delete deserialized_option;
            deserialized_option = 0;
        };
    }

    return deserialized_option;
}

length_t message_impl::get_someip_length() const {
    return header_.length_;
}

std::uint8_t message_impl::get_number_required_acks() const {
    return number_required_acks_;
}

std::uint8_t message_impl::get_number_contained_acks() const {
    return number_contained_acks_;
}

void message_impl::set_number_required_acks(std::uint8_t _required_acks) {
    number_required_acks_ = _required_acks;
}

void message_impl::increase_number_required_acks(std::uint8_t _amount) {
    number_required_acks_ += _amount;
}

void message_impl::decrease_number_required_acks(std::uint8_t _amount) {
    number_required_acks_ -= _amount;
}

void message_impl::increase_number_contained_acks() {
    number_contained_acks_++;
}

bool message_impl::all_required_acks_contained() const {
    return number_contained_acks_ >= number_required_acks_;
}

std::unique_lock<std::mutex> message_impl::get_message_lock() {
    return std::unique_lock<std::mutex>(message_mutex_);
}

void message_impl::forced_initial_events_add(forced_initial_events_t _entry) {
    std::lock_guard<std::mutex> its_lock(forced_initial_events_mutex_);
    forced_initial_events_info_.push_back(_entry);
}

const std::vector<message_impl::forced_initial_events_t>
message_impl::forced_initial_events_get() {
    std::lock_guard<std::mutex> its_lock(forced_initial_events_mutex_);
    return forced_initial_events_info_;
}

void message_impl::set_initial_events_required(bool _initial_events_required) {
    initial_events_required_ = _initial_events_required;
}

bool message_impl::initial_events_required() const {
    return initial_events_required_;
}

} // namespace sd
} // namespace vsomeip
