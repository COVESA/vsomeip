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
    std::call_once(initialization_flag, []() {
        the_plugin_manager__ = std::make_shared<plugin_manager_impl>();
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
              << ". Falling back to dynamic library loading for \"" << _library << "\"." << std::endl;
    void* handle = load_library(_library);
    if (!handle) {
        std::cerr << "[vsomeip][plugin-debug] Dynamic library loading returned nullptr for \"" << _library
                  << "\". No plugin init symbol can be loaded from this library." << std::endl;
    }
    plugin_init_func its_init_func = reinterpret_cast<plugin_init_func>(load_symbol(handle, VSOMEIP_PLUGIN_INIT_SYMBOL));
    if (its_init_func) {
        std::cerr << "[vsomeip][plugin-debug] Found plugin init symbol \"" << VSOMEIP_PLUGIN_INIT_SYMBOL << "\" in \"" << _library
                  << "\". Calling it to obtain the create_plugin function." << std::endl;
        create_plugin_func its_create_func = (*its_init_func)();
        if (its_create_func) {
            std::cerr << "[vsomeip][plugin-debug] Plugin init function returned a create_plugin function for \"" << _library
                      << "\". Creating plugin instance." << std::endl;
            handles_[_type][_library] = handle;
            auto its_plugin = (*its_create_func)();
            if (its_plugin) {
                std::cerr << "[vsomeip][plugin-debug] Dynamic plugin instance created: name=\"" << its_plugin->get_plugin_name()
                          << "\", type=" << plugin_type_to_string(its_plugin->get_plugin_type()) << " ("
                          << static_cast<int>(its_plugin->get_plugin_type()) << "), version=" << its_plugin->get_plugin_version()
                          << "." << std::endl;
                if (its_plugin->get_plugin_type() == _type && its_plugin->get_plugin_version() == _version) {
                    std::cerr << "[vsomeip][plugin-debug] Dynamic plugin type/version match. Adding plugin \"" << _library
                              << "\" to cache." << std::endl;
                    add_plugin(its_plugin, _library);
                    return its_plugin;
                } else {
                    std::cerr << "[vsomeip][plugin-debug] Dynamic plugin type/version mismatch. Expected type="
                              << plugin_type_to_string(_type) << " (" << static_cast<int>(_type) << "), version=" << _version
                              << "; got type=" << plugin_type_to_string(its_plugin->get_plugin_type()) << " ("
                              << static_cast<int>(its_plugin->get_plugin_type()) << "), version="
                              << its_plugin->get_plugin_version() << "." << std::endl;
                    VSOMEIP_ERROR << "Plugin version mismatch. Ignoring plugin " << its_plugin->get_plugin_name();
                }
            } else {
                std::cerr << "[vsomeip][plugin-debug] Dynamic create_plugin function returned nullptr for \"" << _library << "\"."
                          << std::endl;
            }
        } else {
            std::cerr << "[vsomeip][plugin-debug] Plugin init symbol returned a nullptr create_plugin function for \"" << _library
                      << "\"." << std::endl;
        }
    } else {
        std::cerr << "[vsomeip][plugin-debug] Plugin init symbol \"" << VSOMEIP_PLUGIN_INIT_SYMBOL << "\" was not available for \""
                  << _library << "\"." << std::endl;
    }
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
                     "or its static registrar did not run."
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
    std::cerr << "[vsomeip][plugin-debug] Attempting to load dynamic plugin library \"" << _path << "\"." << std::endl;
#ifdef _WIN32
    void* handle = LoadLibrary(_path.c_str());
    if (handle == nullptr) {
        std::cerr << "[vsomeip][plugin-debug] LoadLibrary failed for \"" << _path << "\" with error code " << GetLastError()
                  << "." << std::endl;
    } else {
        std::cerr << "[vsomeip][plugin-debug] LoadLibrary succeeded for \"" << _path << "\"." << std::endl;
    }
    return handle;
#else
    void* handle = dlopen(_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (handle == nullptr) {
        const char* its_error = dlerror();
        std::cerr << "[vsomeip][plugin-debug] dlopen failed for \"" << _path << "\" due to: "
                  << (its_error ? its_error : "<no dlerror available>") << "." << std::endl;
        VSOMEIP_ERROR << "Could not dlopen \"" << _path << "\" due to err: " << (its_error ? its_error : "<no dlerror available>");
    } else {
        std::cerr << "[vsomeip][plugin-debug] dlopen succeeded for \"" << _path << "\"." << std::endl;
    }

    return handle;
#endif
}

void* plugin_manager_impl::load_symbol(void* _handle, const std::string& _symbol_name) {
    void* symbol = nullptr;
    if (_handle) {
        std::cerr << "[vsomeip][plugin-debug] Attempting to load symbol \"" << _symbol_name << "\" from dynamic plugin handle."
                  << std::endl;
#ifdef _WIN32
        symbol = GetProcAddress(reinterpret_cast<HMODULE>(_handle), _symbol_name.c_str());
#else
        symbol = dlsym(_handle, _symbol_name.c_str());
#endif

        if (!symbol) {
            char* error_message = nullptr;

#ifdef _WIN32
            DWORD error_code = GetLastError();
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&error_message), 0, nullptr);
#else
            error_message = dlerror();
#endif

#ifdef __QNX__
            VSOMEIP_ERROR << "Cannot load symbol " << std::quoted(_symbol_name.c_str()) << " because: " << error_message;
#else
            VSOMEIP_ERROR << "Cannot load symbol " << std::quoted(_symbol_name) << " because: " << error_message;
#endif
            std::cerr << "[vsomeip][plugin-debug] Loading symbol \"" << _symbol_name
                      << "\" failed because: " << (error_message ? error_message : "<no symbol error available>") << "."
                      << std::endl;

#ifdef _WIN32
            // Required to release memory allocated by FormatMessageA()
            LocalFree(error_message);
#endif
        } else {
            std::cerr << "[vsomeip][plugin-debug] Loading symbol \"" << _symbol_name << "\" succeeded." << std::endl;
        }
    } else {
        std::cerr << "[vsomeip][plugin-debug] Skipping symbol lookup for \"" << _symbol_name
                  << "\" because the dynamic library handle is nullptr." << std::endl;
    }
    return symbol;
}

void plugin_manager_impl::unload_library(void* _handle) {
    if (_handle) {
#ifdef _WIN32
        FreeLibrary(reinterpret_cast<HMODULE>(_handle));
#else
        dlclose(_handle);
#endif
    }
}

} // namespace vsomeip_v3
