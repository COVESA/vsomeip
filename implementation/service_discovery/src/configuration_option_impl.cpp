// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstring>

#include "../include/configuration_option_impl.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../message/include/serializer.hpp"

namespace vsomeip_v3 {
namespace sd {

configuration_option_impl::configuration_option_impl() {
    length_ = 2; // always contains "Reserved" and the trailing '\0'
    type_ = option_type_e::CONFIGURATION;
}

configuration_option_impl::~configuration_option_impl() { }

bool configuration_option_impl::equals(const option_impl& _other) const {
    bool is_equal(option_impl::equals(_other));

    if (is_equal) {
        const configuration_option_impl& its_other = dynamic_cast<const configuration_option_impl&>(_other);
        is_equal = (configuration_ == its_other.configuration_);
    }

    return is_equal;
}

void configuration_option_impl::add_item(const std::string& _key, const std::string& _value) {
    configuration_[_key] = _value;
    length_ = uint16_t(length_ + _key.length() + _value.length() + 2u); // +2 for the '=' and length
}

void configuration_option_impl::remove_item(const std::string& _key) {
    auto it = configuration_.find(_key);
    if (it != configuration_.end()) {
        length_ = uint16_t(length_ - (it->first.length() + it->second.length() + 2u));
        configuration_.erase(it);
    }
}

std::vector<std::string> configuration_option_impl::get_keys() const {
    std::vector<std::string> l_keys;
    for (const auto& elem : configuration_)
        l_keys.push_back(elem.first);
    return l_keys;
}

std::vector<std::string> configuration_option_impl::get_values() const {
    std::vector<std::string> l_values;
    for (const auto& elem : configuration_)
        l_values.push_back(elem.second);
    return l_values;
}

std::string configuration_option_impl::get_value(const std::string& _key) const {
    std::string l_value("");
    auto l_elem = configuration_.find(_key);
    if (l_elem != configuration_.end())
        l_value = l_elem->second;
    return l_value;
}

bool configuration_option_impl::serialize(vsomeip_v3::serializer* _to) const {
    bool is_successful;
    std::string configuration_string;

    for (auto i = configuration_.begin(); i != configuration_.end(); ++i) {
        char l_length = char(1 + i->first.length() + i->second.length());
        configuration_string.push_back(l_length);
        configuration_string.append(i->first);
        configuration_string.push_back('=');
        configuration_string.append(i->second);
    }
    configuration_string.push_back('\0');

    is_successful = option_impl::serialize(_to);
    if (is_successful) {
        is_successful =
                _to->serialize(reinterpret_cast<const uint8_t*>(configuration_string.c_str()), uint32_t(configuration_string.length()));
    }

    return is_successful;
}

bool configuration_option_impl::deserialize(vsomeip_v3::deserializer* _from) {
    bool is_successful = option_impl::deserialize(_from);
    if (!is_successful) {
        return false;
    }

    // option_impl::deserialize has consumed length_ + type + reserved.
    // length_ covers the reserved byte (already consumed) plus the
    // configuration items plus the trailing '\0' terminator
    // (SWS_SD_00292). Bytes that still belong to *this* option, and
    // which the loop below is allowed to consume, are length_ - 1.
    //
    // Without the bytes_remaining budget tracked here, the loop would
    // only stop on a 0-length item or when the outer deserializer's
    // remaining_ ran out — i.e. it could greedily consume bytes from
    // subsequent SD options if a multicast peer sent a CONFIGURATION
    // option that omitted its trailing terminator or claimed an
    // item-length larger than the option's declared payload.
    if (length_ < 1) {
        // length_ is too small to even cover the already-consumed
        // reserved byte; the option is malformed.
        return false;
    }
    std::size_t bytes_remaining = static_cast<std::size_t>(length_) - 1u;

    uint8_t l_itemLength = 0;
    std::string l_item(256, 0), l_key, l_value;

    while (bytes_remaining > 0) {
        l_itemLength = 0;
        l_key.clear();
        l_value.clear();
        l_item.assign(256, '\0');

        if (!_from->deserialize(l_itemLength)) {
            return false;
        }
        bytes_remaining -= 1;

        if (l_itemLength == 0) {
            // The terminating empty entry per SWS_SD_00292. Any bytes
            // left over (bytes_remaining > 0 here) are option padding
            // not described by the spec — leave them alone; the parent
            // dispatcher's set_remaining bookkeeping will keep the
            // overall option-stream walk honest, and being lenient
            // about trailing padding matches the previous behaviour
            // for well-formed inputs.
            return true;
        }

        if (l_itemLength > bytes_remaining) {
            // The item declares more bytes than fit inside the option's
            // declared payload; reject before the read so the loop can
            // never consume bytes that belong to the next option.
            return false;
        }

        if (!_from->deserialize(l_item, static_cast<std::size_t>(l_itemLength))) {
            return false;
        }
        bytes_remaining -= l_itemLength;

        size_t l_eqPos = l_item.find('='); // SWS_SD_00292
        l_key = l_item.substr(0, l_eqPos);

        // if no "=" is found, no value is present for key (SWS_SD_00466)
        if (l_eqPos != std::string::npos)
            l_value = l_item.substr(l_eqPos + 1);
        if (configuration_.end() == configuration_.find(l_key)) {
            configuration_[l_key] = l_value;
        } else {
            // TODO: log reason for failing deserialization
            return false;
        }
    }

    // Ran out of bytes within the option without seeing the terminator;
    // malformed.
    return false;
}

} // namespace sd
} // namespace vsomeip_v3
