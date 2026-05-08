// Copyright (C) 2024 Apex.AI, Inc.

#include <mutex>

#include <vsomeip/internal/plugin_manager.hpp>

#include "../implementation/configuration/include/configuration_plugin_impl.hpp"
#include "../implementation/service_discovery/include/runtime_impl.hpp"

namespace vsomeip_v3 {
namespace {

void initialize_builtin_plugins() {
    static std::once_flag registration_flag;
    std::call_once(registration_flag, []() {
        plugin_manager::register_static_plugin(plugin_type_e::CONFIGURATION_PLUGIN,
                                               configuration_plugin_impl::get_plugin);
        plugin_manager::register_static_plugin(plugin_type_e::SD_RUNTIME_PLUGIN,
                                               sd::runtime_impl::get_plugin);
    });
}

struct builtin_plugin_initializer {
    builtin_plugin_initializer() {
        initialize_builtin_plugins();
    }
};

const builtin_plugin_initializer builtin_plugin_initializer_{};

} // namespace
} // namespace vsomeip_v3
