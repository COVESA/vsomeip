// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string>

#include <boost/property_tree/ptree.hpp>

namespace vsomeip_v3 {

struct configuration_element {
    std::string name_;
    boost::property_tree::ptree tree_;

    configuration_element(const std::string& _name, const boost::property_tree::ptree& _tree) noexcept : name_(_name), tree_(_tree) { }

    configuration_element(configuration_element&& _source) noexcept : name_(std::move(_source.name_)), tree_(std::move(_source.tree_)) { }

    bool operator<(const configuration_element& _other) const { return (name_ < _other.name_); }
};

} // namespace vsomeip_v3
