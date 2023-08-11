// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>
#include <vsomeip/internal/logger.hpp>

#include "../include/configuration_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip_v3 {
namespace sd {

namespace {
    bool verify_key(const std::string& _key) {
        for (const char c: _key) {
            if (c < 0x20 || c > 0x7E) {
                VSOMEIP_ERROR << "configuration_option_impl::" << __func__ <<
                    "key must only contain ASCII characters";
                    return false;
            }
            if (c == 0x3D) {
                VSOMEIP_ERROR << "configuration_option_impl::" << __func__ <<
                    "key must not contain an '=' character.";
                    return false;
            }
        }
        return true;
    }
}

configuration_option_impl::configuration_option_impl() {
    length_ = 2; // always contains "Reserved" and the trailing '\0'
    type_ = option_type_e::CONFIGURATION;
}

configuration_option_impl::~configuration_option_impl() {
}

bool
configuration_option_impl::equals(const option_impl &_other) const {
    bool is_equal(option_impl::equals(_other));

    if (is_equal) {
        const configuration_option_impl &its_other
            = dynamic_cast<const configuration_option_impl &>(_other);
        is_equal = (configuration_ == its_other.configuration_);
    }

    return is_equal;
}

void configuration_option_impl::add_item(const std::string &_key,
        const std::string &_value) {
    if (!verify_key(_key)) {
        return;
    }
    configuration_.emplace(std::make_pair(_key, configuration_value{false,_value}));
    length_ = uint16_t(length_ + _key.length() + _value.length() + 2u); // +2 for the '=' and length
}

void configuration_option_impl::add_item(const std::string &_key) {
    if (!verify_key(_key)) {
        return;
    }
    configuration_.emplace(std::make_pair(_key, configuration_value{true, std::string()}));
    length_ = uint16_t(length_ + _key.length() + 1u); // +1 for the 'length
}

void configuration_option_impl::remove_item(const std::string &_key) {
    auto it = configuration_.find(_key);
    if (it != configuration_.end()) {
        length_ = uint16_t(length_ - (it->first.length() + it->second.value_.length() + 1u));
        // +1 for the '=' sign.
        if (!it->second.only_present_)
            length_++;
        configuration_.erase(it);
    }
}

std::vector<std::string> configuration_option_impl::get_keys() const {
    std::vector < std::string > l_keys;
    for (const auto& elem : configuration_)
        l_keys.push_back(elem.first);
    return l_keys;
}

std::vector<std::string> configuration_option_impl::get_values() const {
    std::vector < std::string > l_values;
    for (const auto& elem : configuration_)
        l_values.push_back(elem.second.value_);
    return l_values;
}

std::string configuration_option_impl::get_value(
        const std::string &_key,
        const int occurence) const {
    auto iterators = configuration_.equal_range(_key);
    auto it = iterators.first;
    if (occurence >= std::distance(iterators.first, iterators.second)) {
        return std::string();
    }

    return it->second.value_;
}

bool configuration_option_impl::has_key(const std::string &_key, int occurence) const {
    auto iterators = configuration_.equal_range(_key);
    if (occurence >= std::distance(iterators.first, iterators.second)) {
        return false;
    }

    return true;
}

bool configuration_option_impl::has_value(const std::string &_key, int occurence) const {
    auto iterators = configuration_.equal_range(_key);
    if (occurence >= std::distance(iterators.first, iterators.second)) {
        return false;
    }

    auto it = iterators.first;
    std::advance(it, occurence);
    if (it->second.only_present_) {
        return false;
    }

    return true;
}

uint configuration_option_impl::is_present(const std::string &_key) const {
    return static_cast<uint>(configuration_.count(_key));
}

bool configuration_option_impl::serialize(vsomeip_v3::serializer *_to) const {
    bool is_successful;
    std::string configuration_string;

    for (auto i = configuration_.begin(); i != configuration_.end(); ++i) {
        // If key is just present, there will not be any '=' sign or value at all.
        if (i->second.only_present_) {
            configuration_string.push_back(static_cast<char>(i->first.length()));
            configuration_string.append(i->first);
            continue;
        }
        char l_length = char(1 + i->first.length() + i->second.value_.length());
        configuration_string.push_back(l_length);
        configuration_string.append(i->first);
        configuration_string.push_back('=');
        configuration_string.append(i->second.value_);
    }
    configuration_string.push_back('\0');

    is_successful = option_impl::serialize(_to);
    if (is_successful) {
        is_successful = _to->serialize(
                reinterpret_cast<const uint8_t*>(configuration_string.c_str()),
                uint32_t(configuration_string.length()));
    }

    return is_successful;
}

bool configuration_option_impl::deserialize(vsomeip_v3::deserializer *_from) {
    if (!option_impl::deserialize(_from)) {
        VSOMEIP_WARNING << __func__ << "Could not deserialize Option header.";
        return false;
    }

    // Length contains reserved byte.
    const uint32_t string_length = length_ - 1;
    std::string raw_string(string_length, 0);

    if (string_length == 0) {
        VSOMEIP_WARNING << "Configuration Option: Invalid String length.";
        return false;
    }

    if (!_from->deserialize(raw_string, string_length)) {
        VSOMEIP_WARNING << "Configuration Option: Could not deserialize Configuration String.";
        return false;
    }

    uint32_t substring_size_index = 0;
    uint8_t substring_size = static_cast<uint8_t>(raw_string[substring_size_index]);
    while (substring_size != 0) {
        const uint32_t substring_begin_index = substring_size_index + 1;
        const uint32_t substring_end_index = substring_begin_index + substring_size;

        if (substring_end_index > string_length) {
            VSOMEIP_WARNING << "Configuration Option: Invalid Configuration substring size.";
            return false;
        }

        const char* const sub_string = raw_string.data() + substring_begin_index;
        uint32_t equal_sign_index = 0;
        if (sub_string[0] == 0x3D) {
            VSOMEIP_WARNING << "Configuration Option: Substring of Configuration Option starts with '='.";
            return false;
        }
        for (uint32_t i = 0; i < substring_size; ++i) {
            const char c = sub_string[i];
            if (c < 0x20 || c > 0x7E) {
                VSOMEIP_WARNING << "Configuration Option: Non ASCII character in Configuration Option key.";
                return false;
            }
            if (c == 0x3D) {
                equal_sign_index = i;
                break;
            }
        }

        if (equal_sign_index == 0) {
            // No '=' sign means that key is present. (SWS_SD_00466)
            configuration_.emplace(
                std::make_pair(
                    std::string(sub_string, substring_size),
                    configuration_value{
                        true,
                        std::string()
                    }
                )
            );
        } else {
            configuration_.emplace(
                std::make_pair(
                    std::string(sub_string, equal_sign_index),
                    configuration_value{
                        false,
                        std::string(
                            sub_string + equal_sign_index + 1,
                            substring_size - equal_sign_index - 1
                        )
                    }
                )
            );
        }

        substring_size_index = substring_end_index;
        substring_size = static_cast<uint8_t>(raw_string[substring_size_index]);
    }

    if (substring_size_index < string_length - 1) {
        VSOMEIP_WARNING << "Configuration Option: String length exceeds escape character.";
    }

    return true;
}

bool configuration_option_impl::configuration_value::operator==(const configuration_value& other) const {
    return only_present_ == other.only_present_ && value_ == other.value_;
}

}

// namespace sd
} // namespace vsomeip_v3
