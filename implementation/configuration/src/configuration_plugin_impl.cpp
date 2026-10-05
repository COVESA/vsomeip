// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/configuration_plugin_impl.hpp"
#include "../include/configuration_impl.hpp"

VSOMEIP_PLUGIN(vsomeip_v3::configuration_plugin_impl)

namespace vsomeip_v3 {

configuration_plugin_impl::configuration_plugin_impl() :
    plugin_impl("vsomeip-configuration-plugin", VSOMEIP_CONFIG_PLUGIN_VERSION, plugin_type_e::CONFIGURATION_PLUGIN) { }

configuration_plugin_impl::~configuration_plugin_impl() { }

std::string configuration_plugin_impl::get_cache_key(const std::string& _name, const std::string& _path) const {
    // Per-application env var takes highest priority.
    std::string its_named_var = VSOMEIP_ENV_CONFIGURATION;
    its_named_var += "_" + _name;
    const char* its_env = VSOMEIP_GETENV(its_named_var.c_str());
    if (its_env != nullptr) {
        return std::string(its_env);
    }

    // Fall back to the process-wide env var.
    its_env = VSOMEIP_GETENV(VSOMEIP_ENV_CONFIGURATION);
    if (its_env != nullptr) {
        return std::string(its_env);
    }

    // Fall back to the explicitly supplied path.
    return _path;
}

std::shared_ptr<configuration> configuration_plugin_impl::get_configuration(const std::string& _name, const std::string& _path) {
    std::scoped_lock its_lock(mutex_);

    const std::string its_key = get_cache_key(_name, _path);

    if (auto its_key_iter = configurations_.find(its_key); its_key_iter != configurations_.end()) {
        return its_key_iter->second;
    }

    // No existing configuration for this key — create and load one.
    auto its_configuration = make_configuration(_path);
    its_configuration->load(_name);
    configurations_[its_key] = its_configuration;

    return its_configuration;
}

std::shared_ptr<cfg::configuration_impl> configuration_plugin_impl::make_configuration(const std::string& _path) {
    return std::make_shared<cfg::configuration_impl>(_path);
}

void configuration_plugin_impl::clear_configurations() {
    std::scoped_lock its_lock(mutex_);
    configurations_.clear();
}

} // namespace vsomeip_v3
