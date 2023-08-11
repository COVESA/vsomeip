// Copyright (C) 2014-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SD_CONFIGURATION_OPTION_IMPL_HPP_
#define VSOMEIP_V3_SD_CONFIGURATION_OPTION_IMPL_HPP_

#include <map>
#include <string>
#include <vector>

#include "option_impl.hpp"

namespace vsomeip_v3 {

class serializer;
class deserializer;

namespace sd {

class configuration_option_impl: public option_impl {

    struct configuration_value {
        bool only_present_;
        std::string value_;

        bool operator==(const configuration_value& other) const;
    };

public:
    configuration_option_impl();
    virtual ~configuration_option_impl();

    bool equals(const option_impl &_other) const;

    void add_item(const std::string &_key, const std::string &_value);
    void add_item(const std::string &_key);
    void remove_item(const std::string &_key);

    std::vector<std::string> get_keys() const;
    std::vector<std::string> get_values() const;
    std::string get_value(const std::string &_key, int occurence = 0) const;
    uint is_present(const std::string &_key) const;
    bool has_key(const std::string &_key, int occurence = 0) const;
    bool has_value(const std::string &_key, int occurence = 0) const;

    bool serialize(vsomeip_v3::serializer *_to) const;
    bool deserialize(vsomeip_v3::deserializer *_from);

private:
    std::multimap<std::string, configuration_value> configuration_;
};

} // namespace sd
} // namespace vsomeip_v3

#endif // VSOMEIP_V3_SD_CONFIGURATION_OPTION_IMPL_HPP_
