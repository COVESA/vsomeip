// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/defines.hpp>

#include "../include/runtime_impl.hpp"
#include "../include/service_discovery_impl.hpp"
#include "../include/service_discovery_proxy.hpp"
#include "../include/service_discovery_stub_impl.hpp"

namespace vsomeip {
namespace sd {

runtime * runtime_impl::get() {
	static runtime_impl the_runtime;
	return &the_runtime;
}

runtime_impl::~runtime_impl() {
}

std::shared_ptr< service_discovery > runtime_impl::create_service_discovery(service_discovery_host *_host) {
	return std::make_shared< service_discovery_impl >(_host);
}

std::shared_ptr< service_discovery_stub > runtime_impl::create_service_discovery_stub(service_discovery *_discovery) {
	return std::make_shared< service_discovery_stub_impl >(_discovery);
}

std::shared_ptr< service_discovery > runtime_impl::create_service_discovery_proxy(service_discovery_host *_host) {
	return std::make_shared< service_discovery_proxy >(_host);
}

} // namespace sd
} // namespace vsomeip
