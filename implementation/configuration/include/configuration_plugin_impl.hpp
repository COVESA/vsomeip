// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "configuration_plugin.hpp"

namespace vsomeip_v3 {
namespace cfg {

class configuration_impl;

} // namespace cfg

// The configuration used to be loaded as a runtime plugin (libvsomeip3-cfg).
// It is now compiled directly into libvsomeip3, but we keep this class as a
// process-wide singleton so that multiple application_impl instances in the
// same process continue to share a single configuration per name.
class configuration_plugin_impl : public configuration_plugin {
public:
    static std::shared_ptr<configuration_plugin_impl> get();

    configuration_plugin_impl();
    virtual ~configuration_plugin_impl();

    std::shared_ptr<configuration> get_configuration(const std::string& _name, const std::string& _path);
    void clear_configurations() override;

protected:
    // Overridable factory — tests replace this to inject mock configurations.
    virtual std::shared_ptr<cfg::configuration_impl> make_configuration(const std::string& _path);

private:
    std::string get_cache_key(const std::string& _name, const std::string& _path) const;

    static std::shared_ptr<configuration_plugin_impl> the_configuration_plugin__;
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<cfg::configuration_impl>> configurations_;
};

} // namespace vsomeip_v3
