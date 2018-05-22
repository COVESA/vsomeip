// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_CONFIGURATION_OPTION_IMPL_HPP
#define VSOMEIP_SD_CONFIGURATION_OPTION_IMPL_HPP

#include <map>
#include <string>
#include <vector>

#include "option_impl.hpp"

namespace vsomeip {

class serializer;
class deserializer;

namespace sd {

class configuration_option_impl: public option_impl {

public:
    configuration_option_impl();
    virtual ~configuration_option_impl();
    bool operator==(const configuration_option_impl &_other) const;

    void add_item(const std::string &_key, const std::string &_value);
    void remove_item(const std::string &_key);

    std::vector<std::string> get_keys() const;
    std::vector<std::string> get_values() const;
    std::string get_value(const std::string &_key) const;

    bool serialize(vsomeip::serializer *_to) const;
    bool deserialize(vsomeip::deserializer *_from);

private:
    std::map<std::string, std::string> configuration_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_CONFIGURATION_OPTION_IMPL_HPP
