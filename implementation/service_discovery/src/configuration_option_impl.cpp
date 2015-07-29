// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>

#include "../include/configuration_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip {
namespace sd {

configuration_option_impl::configuration_option_impl() {
    length_ = 2; // always contains "Reserved" and the trailing '\0'
    type_ = option_type_e::CONFIGURATION;
}

configuration_option_impl::~configuration_option_impl() {
}

bool configuration_option_impl::operator ==(const option_impl &_other) const {
    if (_other.get_type() != option_type_e::CONFIGURATION)
        return false;

    const configuration_option_impl& other =
            dynamic_cast<const configuration_option_impl &>(_other);

    return (configuration_ == other.configuration_);
}

void configuration_option_impl::add_item(const std::string &_key,
        const std::string &_value) {
    configuration_[_key] = _value;
    length_ += (_key.length() + _value.length() + 2); // +2 for the '=' and length
}

void configuration_option_impl::remove_item(const std::string &_key) {
    auto it = configuration_.find(_key);
    if (it != configuration_.end()) {
        length_ -= (it->first.length() + it->second.length() + 2);
        configuration_.erase(it);
    }
}

std::vector<std::string> configuration_option_impl::get_keys() const {
    std::vector < std::string > l_keys;
    for (auto elem : configuration_)
        l_keys.push_back(elem.first);
    return l_keys;
}

std::vector<std::string> configuration_option_impl::get_values() const {
    std::vector < std::string > l_values;
    for (auto elem : configuration_)
        l_values.push_back(elem.second);
    return l_values;
}

std::string configuration_option_impl::get_value(
        const std::string &_key) const {
    std::string l_value("");
    auto l_elem = configuration_.find(_key);
    if (l_elem != configuration_.end())
        l_value = l_elem->second;
    return l_value;
}

bool configuration_option_impl::serialize(vsomeip::serializer *_to) const {
    bool is_successful;
    std::string configuration_string;

    for (auto i = configuration_.begin(); i != configuration_.end(); ++i) {
        char l_length = 1 + i->first.length() + i->second.length();
        configuration_string.push_back(l_length);
        configuration_string.append(i->first);
        configuration_string.push_back('=');
        configuration_string.append(i->second);
    }
    configuration_string.push_back('\0');

    is_successful = option_impl::serialize(_to);
    if (is_successful) {
        is_successful = _to->serialize(
                reinterpret_cast<const uint8_t*>(configuration_string.c_str()),
                configuration_string.length());
    }

    return is_successful;
}

bool configuration_option_impl::deserialize(vsomeip::deserializer *_from) {
    bool is_successful = option_impl::deserialize(_from);
    uint8_t l_length = 0;
    std::string l_item(256, 0), l_key, l_value;

    do {
        is_successful = is_successful && _from->deserialize(l_length);
        if (l_length > 0) {
            is_successful = is_successful
                    && _from->deserialize((uint8_t*) &l_item[0], l_length);
            if (is_successful) {
                size_t l_eqPos = l_item.find('=');
                l_key = l_item.substr(0, l_eqPos);
                l_value = l_item.substr(l_eqPos + 1);

                if (configuration_.end() == configuration_.find(l_key)) {
                    configuration_[l_key] = l_value;
                } else {
                    // TODO: log reason for failing deserialization
                    is_successful = false;
                }
            }
        }

    } while (is_successful && l_length > 0);

    return is_successful;
}

} // namespace sd
} // namespace vsomeip
