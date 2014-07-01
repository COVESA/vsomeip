// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/service_discovery.hpp"
#include "../include/service_discovery_stub_impl.hpp"

namespace vsomeip {
namespace sd {

service_discovery_stub_impl::service_discovery_stub_impl(
		service_discovery *_discovery) : discovery_(_discovery), io_(_discovery->get_io()) {

}

service_discovery_stub_impl::~service_discovery_stub_impl() {

}

void service_discovery_stub_impl::init() {

}

void service_discovery_stub_impl::start() {

}

void service_discovery_stub_impl::stop() {

}

} // namespace sd
} // namespace vsomeip
