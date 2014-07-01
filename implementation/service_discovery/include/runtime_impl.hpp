// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_RUNTIME_IMPL_HPP
#define VSOMEIP_SD_RUNTIME_IMPL_HPP

#include "runtime.hpp"

namespace vsomeip {
namespace sd {

class runtime_impl: public runtime {
public:
	static runtime * get();

	virtual ~runtime_impl();

	std::shared_ptr< service_discovery > create_service_discovery(service_discovery_host *_host);
	std::shared_ptr< service_discovery_stub > create_service_discovery_stub(service_discovery *_discovery);
	std::shared_ptr< service_discovery > create_service_discovery_proxy(service_discovery_host *_host);
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_RUNTIME_IMPL_HPP
