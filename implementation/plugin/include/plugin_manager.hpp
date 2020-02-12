// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_PLUGIN_MANAGER_HPP
#define VSOMEIP_V3_PLUGIN_MANAGER_HPP

#include <memory>
#include <string>

#include <vsomeip/export.hpp>
#include <vsomeip/plugin.hpp>

namespace vsomeip_v3 {

class plugin_manager {
public:
    VSOMEIP_EXPORT virtual ~plugin_manager() {};
    VSOMEIP_EXPORT static std::shared_ptr<plugin_manager> get();
    VSOMEIP_EXPORT virtual void load_plugins() = 0;
    VSOMEIP_EXPORT virtual std::shared_ptr<plugin> get_plugin(plugin_type_e _type, std::string _name) = 0;

};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_PLUGIN_MANAGER_HPP
