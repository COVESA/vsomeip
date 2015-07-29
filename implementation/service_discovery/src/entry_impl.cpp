// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>

#include "../include/entry_impl.hpp"
#include "../include/message_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

// TODO: throw exception if this constructor is used
entry_impl::entry_impl() {
    type_ = entry_type_e::UNKNOWN;
    major_version_ = 0;
    service_ = 0x0;
    instance_ = 0x0;
    ttl_ = 0x0;
}

entry_impl::entry_impl(const entry_impl &_entry) {
    type_ = _entry.type_;
    major_version_ = _entry.major_version_;
    service_ = _entry.service_;
    instance_ = _entry.instance_;
    ttl_ = _entry.ttl_;
}

entry_impl::~entry_impl() {
}

entry_type_e entry_impl::get_type() const {
    return type_;
}

void entry_impl::set_type(entry_type_e _type) {
    type_ = _type;
}

service_t entry_impl::get_service() const {
    return service_;
}

void entry_impl::set_service(service_t _service) {
    service_ = _service;
}

instance_t entry_impl::get_instance() const {
    return instance_;
}

void entry_impl::set_instance(instance_t _instance) {
    instance_ = _instance;
}

major_version_t entry_impl::get_major_version() const {
    return major_version_;
}

void entry_impl::set_major_version(major_version_t _major_version) {
    major_version_ = _major_version;
}

ttl_t entry_impl::get_ttl() const {
    return ttl_;
}

void entry_impl::set_ttl(ttl_t _ttl) {
    ttl_ = _ttl;
}

const std::vector<uint8_t> & entry_impl::get_options(uint8_t _run) const {
    static std::vector<uint8_t> invalid_options;
    if (_run > 0 && _run <= VSOMEIP_MAX_OPTION_RUN)
        return options_[_run - 1];

    return invalid_options;
}

void entry_impl::assign_option(const std::shared_ptr<option_impl> &_option,
        uint8_t _run) {
    if (_run > 0 && _run <= VSOMEIP_MAX_OPTION_RUN) {
        _run--; // Index = Run-1

        uint8_t option_index = get_owning_message()->get_option_index(_option);
        if (0x10 > option_index) { // as we have only a nibble for the option counter
            options_[_run].push_back(option_index);
            std::sort(options_[_run].begin(), options_[_run].end());
        } else {
            // TODO: decide what to do if option does not belong to the message.
        }
    } else {
        // TODO: decide what to do if an illegal index for the option run is provided
    }
}

bool entry_impl::serialize(vsomeip::serializer *_to) const {
    bool is_successful = (0 != _to
            && _to->serialize(static_cast<uint8_t>(type_)));

    uint8_t index_first_option_run = 0;
    if (options_[0].size() > 0)
        index_first_option_run = options_[0][0];
    is_successful = is_successful && _to->serialize(index_first_option_run);

    uint8_t index_second_option_run = 0;
    if (options_[1].size() > 0)
        index_second_option_run = options_[1][0];
    is_successful = is_successful && _to->serialize(index_second_option_run);

    uint8_t number_of_options = ((((uint8_t) options_[0].size()) << 4)
            | (((uint8_t) options_[1].size()) & 0x0F));
    is_successful = is_successful && _to->serialize(number_of_options);

    is_successful = is_successful
            && _to->serialize(static_cast<uint16_t>(service_));

    is_successful = is_successful
            && _to->serialize(static_cast<uint16_t>(instance_));

    return is_successful;
}

bool entry_impl::deserialize(vsomeip::deserializer *_from) {
    bool is_successful = (0 != _from);

    uint8_t its_type;
    is_successful = is_successful && _from->deserialize(its_type);
    type_ = static_cast<entry_type_e>(its_type);

    uint8_t its_index1;
    is_successful = is_successful && _from->deserialize(its_index1);

    uint8_t its_index2;
    is_successful = is_successful && _from->deserialize(its_index2);

    uint8_t its_numbers;
    is_successful = is_successful && _from->deserialize(its_numbers);

    uint8_t its_numbers1 = (its_numbers >> 4);
    uint8_t its_numbers2 = (its_numbers & 0xF);

    for (uint8_t i = its_index1; i < its_index1 + its_numbers1; ++i)
        options_[0].push_back(i);

    for (uint8_t i = its_index2; i < its_index2 + its_numbers2; ++i)
        options_[1].push_back(i);

    uint16_t its_id;
    is_successful = is_successful && _from->deserialize(its_id);
    service_ = static_cast<service_t>(its_id);

    is_successful = is_successful && _from->deserialize(its_id);
    instance_ = static_cast<instance_t>(its_id);

    return is_successful;
}

bool entry_impl::is_service_entry() const {
    return (type_ <= entry_type_e::REQUEST_SERVICE);
}

bool entry_impl::is_eventgroup_entry() const {
    return (type_ >= entry_type_e::FIND_EVENT_GROUP
            && type_ <= entry_type_e::SUBSCRIBE_EVENTGROUP_ACK);
}

} // namespace sd
} // namespace vsomeip
