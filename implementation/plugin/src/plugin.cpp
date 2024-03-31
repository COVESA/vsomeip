// Copyright (C) 2016-2021 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/plugin.hpp>

namespace vsomeip_v3 {

// non-inline destructors to make typeinfo of the type visible outside the shared library boundary
plugin::~plugin() {
}

// plugin_impl::~plugin_impl() {
// }

// endpoint::~endpoint() {
// }

// client_endpoint::~client_endpoint() {
// }

// configuration_plugin::~configuration_plugin() {
// }

}  // namespace vsomeip_v3
