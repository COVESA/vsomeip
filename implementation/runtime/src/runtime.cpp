// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/runtime.hpp>
#include <vsomeip/plugin.hpp>

#include "../include/runtime_impl.hpp"
#include "../../endpoints/include/endpoint.hpp"
#include "../../endpoints/include/client_endpoint.hpp"
#include "../../configuration/include/configuration_plugin.hpp"

namespace vsomeip_v3 {

std::string runtime::get_property(const std::string &_name) {
    return runtime_impl::get_property(_name);
}

void runtime::set_property(const std::string &_name, const std::string &_value) {
    runtime_impl::set_property(_name, _value);
}

std::shared_ptr<runtime> runtime::get() {
    return runtime_impl::get();
}

// non-inline destructors to make typeinfo of the type visible outside the shared library boundary
#ifdef ANDROID
plugin::~plugin() {
}

endpoint::~endpoint() {
}

client_endpoint::~client_endpoint() {
}

configuration_plugin::~configuration_plugin() {
}
#endif

} // namespace vsomeip_v3
