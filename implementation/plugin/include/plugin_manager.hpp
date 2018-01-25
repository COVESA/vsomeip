// Copyright (C) 2016-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_PLUGIN_MANAGER_HPP
#define VSOMEIP_PLUGIN_MANAGER_HPP

#include <map>
#include <chrono>
#include <mutex>
#include <set>

#include <vsomeip/constants.hpp>
#include <vsomeip/export.hpp>
#include <vsomeip/plugin.hpp>

namespace vsomeip {

class plugin_manager {
public:
        VSOMEIP_EXPORT static std::shared_ptr<plugin_manager> get();

        plugin_manager();

        ~plugin_manager();

        VSOMEIP_EXPORT void load_plugins();

        VSOMEIP_EXPORT std::shared_ptr<plugin> get_plugin(plugin_type_e _type, std::string _name);

        VSOMEIP_EXPORT std::shared_ptr<plugin> load_plugin(
                const std::string _library, plugin_type_e _type,
                const uint32_t _version);

        VSOMEIP_EXPORT bool unload_plugin(plugin_type_e _type);

private:
        void add_plugin(const std::shared_ptr<plugin> &_plugin, const std::string _name);

        void * load_library(const std::string &_path);
        void * load_symbol(void * _handle, const std::string &_symbol);

        bool plugins_loaded_;
        std::mutex loader_mutex_;

        std::map<plugin_type_e, std::map<std::string, std::shared_ptr<plugin> > > plugins_;
        std::map<plugin_type_e, std::map<std::string, void*> > handles_;
        std::mutex plugins_mutex_;

        static std::shared_ptr<plugin_manager> the_plugin_manager__;
};
}

#endif // VSOMEIP_PLUGIN_MANAGER_HPP
