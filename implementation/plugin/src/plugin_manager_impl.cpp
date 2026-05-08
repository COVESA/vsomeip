// Copyright (C) 2016-2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sstream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#ifndef _WINSOCKAPI_
#include <Windows.h>
#endif
#else
#include <dlfcn.h>
#endif

#include <vsomeip/plugins/application_plugin.hpp>
#include <vsomeip/plugins/pre_configuration_plugin.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../../configuration/include/configuration_plugin_impl.hpp"
#include "../../service_discovery/include/runtime_impl.hpp"
#include "../include/plugin_manager_impl.hpp"

#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

#include "../../utility/include/utility.hpp"

namespace vsomeip_v3 {

std::shared_ptr<plugin_manager_impl> plugin_manager_impl::the_plugin_manager__ = nullptr;

namespace {

const char* plugin_type_to_string(plugin_type_e _type) {
    switch (_type) {
    case plugin_type_e::APPLICATION_PLUGIN:
        return "APPLICATION_PLUGIN";
    case plugin_type_e::CONFIGURATION_PLUGIN:
        return "CONFIGURATION_PLUGIN";
    case plugin_type_e::PRE_CONFIGURATION_PLUGIN:
        return "PRE_CONFIGURATION_PLUGIN";
    case plugin_type_e::SD_RUNTIME_PLUGIN:
        return "SD_RUNTIME_PLUGIN";
    default:
        return "UNKNOWN_PLUGIN";
    }
}

} // namespace

std::shared_ptr<plugin_manager_impl> plugin_manager_impl::get() {
    static std::once_flag initialization_flag;
    static std::once_flag builtin_plugin_registration_flag;
    std::call_once(initialization_flag, []() {
        the_plugin_manager__ = std::make_shared<plugin_manager_impl>();
    });
    std::call_once(builtin_plugin_registration_flag, []() {
        register_static_configuration_plugin(*the_plugin_manager__);
        sd::register_static_sd_runtime_plugin(*the_plugin_manager__);
    });
    return the_plugin_manager__;
}

plugin_manager_impl::plugin_manager_impl() { }

plugin_manager_impl::~plugin_manager_impl() {
    handles_.clear();
    plugins_.clear();
}

std::shared_ptr<plugin> plugin_manager_impl::get_plugin(plugin_type_e _type, const std::string& _name) {
    std::lock_guard<std::recursive_mutex> its_lock_start_stop(plugins_mutex_);
    std::cerr << "[vsomeip][plugin-debug] get_plugin requested type=" << plugin_type_to_string(_type) << " ("
              << static_cast<int>(_type) << "), name=\"" << _name << "\"." << std::endl;
    auto its_type = plugins_.find(_type);
    if (its_type != plugins_.end()) {
        auto its_name = its_type->second.find(_name);
        if (its_name != its_type->second.end()) {
            std::cerr << "[vsomeip][plugin-debug] Returning cached plugin for type=" << plugin_type_to_string(_type)
                      << ", name=\"" << _name << "\"." << std::endl;
            return its_name->second;
        }
        std::cerr << "[vsomeip][plugin-debug] Plugin cache contains type=" << plugin_type_to_string(_type)
                  << " but no entry named \"" << _name << "\". Loading it now." << std::endl;
    } else {
        std::cerr << "[vsomeip][plugin-debug] Plugin cache has no entries for type=" << plugin_type_to_string(_type)
                  << ". Loading requested plugin now." << std::endl;
    }
    return load_plugin(_name, _type, 1);
}

std::shared_ptr<plugin> plugin_manager_impl::load_plugin(const std::string& _library, plugin_type_e _type, uint32_t _version) {
    std::cerr << "[vsomeip][plugin-debug] load_plugin started for library=\"" << _library << "\", expected type="
              << plugin_type_to_string(_type) << " (" << static_cast<int>(_type) << "), expected version=" << _version << "."
              << std::endl;
    if (auto its_plugin = load_static_plugin(_type, _version)) {
        std::cerr << "[vsomeip][plugin-debug] Static plugin lookup succeeded for type=" << plugin_type_to_string(_type)
                  << ". Caching it under name=\"" << _library << "\"." << std::endl;
        handles_[_type][_library] = nullptr;
        add_plugin(its_plugin, _library);
        return its_plugin;
    }
    std::cerr << "[vsomeip][plugin-debug] Static plugin lookup failed for type=" << plugin_type_to_string(_type)
              << ". Shared-library plugin loading is disabled, so \"" << _library << "\" will not be dlopen'ed." << std::endl;
    std::cerr << "[vsomeip][plugin-debug] load_plugin failed for library=\"" << _library << "\", expected type="
              << plugin_type_to_string(_type) << " (" << static_cast<int>(_type) << "), expected version=" << _version
              << ". Returning nullptr." << std::endl;
    return nullptr;
}

std::shared_ptr<plugin> plugin_manager_impl::load_static_plugin(plugin_type_e _type, uint32_t _version) {
    std::lock_guard<std::recursive_mutex> its_lock_start_stop(plugins_mutex_);
    std::cerr << "[vsomeip][plugin-debug] Looking for registered static plugin factory for type=" << plugin_type_to_string(_type)
              << " (" << static_cast<int>(_type) << "), expected version=" << _version << "." << std::endl;
    auto its_factory = static_factories_.find(_type);
    if (its_factory == static_factories_.end()) {
        std::cerr << "[vsomeip][plugin-debug] No static plugin factory registered for type=" << plugin_type_to_string(_type)
                  << ". For the configuration plugin, this usually means the object containing register_static_plugin was not linked "
                     "or its static registration did not run."
                  << std::endl;
        return nullptr;
    }
    auto its_create_func = its_factory->second;
    if (!its_create_func) {
        std::cerr << "[vsomeip][plugin-debug] Static plugin factory entry exists for type=" << plugin_type_to_string(_type)
                  << " but the factory function pointer is nullptr." << std::endl;
        return nullptr;
    }
    auto its_plugin = (*its_create_func)();
    if (its_plugin && its_plugin->get_plugin_type() == _type && its_plugin->get_plugin_version() == _version) {
        std::cerr << "[vsomeip][plugin-debug] Static plugin factory created matching plugin: name=\""
                  << its_plugin->get_plugin_name() << "\", type=" << plugin_type_to_string(its_plugin->get_plugin_type()) << " ("
                  << static_cast<int>(its_plugin->get_plugin_type()) << "), version=" << its_plugin->get_plugin_version() << "."
                  << std::endl;
        return its_plugin;
    }
    if (its_plugin) {
        std::cerr << "[vsomeip][plugin-debug] Static plugin factory created a plugin with wrong type/version: name=\""
                  << its_plugin->get_plugin_name() << "\", expected type=" << plugin_type_to_string(_type) << " ("
                  << static_cast<int>(_type) << "), expected version=" << _version << "; got type="
                  << plugin_type_to_string(its_plugin->get_plugin_type()) << " ("
                  << static_cast<int>(its_plugin->get_plugin_type()) << "), got version=" << its_plugin->get_plugin_version() << "."
                  << std::endl;
        VSOMEIP_ERROR << "Plugin version mismatch. Ignoring static plugin " << its_plugin->get_plugin_name();
    } else {
        std::cerr << "[vsomeip][plugin-debug] Static plugin factory returned nullptr for type=" << plugin_type_to_string(_type)
                  << "." << std::endl;
    }
    return nullptr;
}

bool plugin_manager_impl::unload_plugin(plugin_type_e _type) {
    std::lock_guard<std::recursive_mutex> its_lock_start_stop(plugins_mutex_);
    const auto found_handle = handles_.find(_type);
    if (found_handle != handles_.end()) {
        for (const auto& its_name : found_handle->second) {
            if (!its_name.second) {
                continue;
            }
#ifdef _WIN32
            FreeLibrary((HMODULE)its_name.second);
#else
            if (dlclose(its_name.second)) {
                VSOMEIP_ERROR << "Unloading failed: (" << dlerror() << ")";
            }
#endif
        }
    } else {
        VSOMEIP_ERROR << "plugin_manager_impl::unload_plugin didn't find plugin"
                      << " type:" << static_cast<int>(_type);
        return false;
    }
    return plugins_.erase(_type);
}

void plugin_manager_impl::add_plugin(const std::shared_ptr<plugin>& _plugin, const std::string& _name) {
    std::cerr << "[vsomeip][plugin-debug] add_plugin storing plugin name=\"" << _name << "\", plugin_name=\""
              << _plugin->get_plugin_name() << "\", type=" << plugin_type_to_string(_plugin->get_plugin_type()) << " ("
              << static_cast<int>(_plugin->get_plugin_type()) << "), version=" << _plugin->get_plugin_version() << "."
              << std::endl;
    plugins_[_plugin->get_plugin_type()][_name] = _plugin;
}

void plugin_manager_impl::register_static_plugin(plugin_type_e _type, create_plugin_func _factory) {
    std::lock_guard<std::recursive_mutex> its_lock_start_stop(plugins_mutex_);
    std::cerr << "[vsomeip][plugin-debug] register_static_plugin called for type=" << plugin_type_to_string(_type) << " ("
              << static_cast<int>(_type) << "), factory=" << (_factory ? "set" : "nullptr") << "." << std::endl;
    static_factories_[_type] = _factory;
}

void* plugin_manager_impl::load_library(const std::string& _path) {
    std::cerr << "[vsomeip][plugin-debug] Shared-library loading is disabled. Refusing to load \"" << _path << "\"."
              << std::endl;
    return nullptr;
}

void* plugin_manager_impl::load_symbol(void* _handle, const std::string& _symbol_name) {
    (void)_handle;
    std::cerr << "[vsomeip][plugin-debug] Shared-library loading is disabled. Refusing to load symbol \"" << _symbol_name
              << "\"." << std::endl;
    return nullptr;
}

void plugin_manager_impl::unload_library(void* _handle) {
    (void)_handle;
    std::cerr << "[vsomeip][plugin-debug] Shared-library loading is disabled. Ignoring unload_library request." << std::endl;
}

} // namespace vsomeip_v3
