// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SD_RUNTIME_HPP
#define VSOMEIP_SD_RUNTIME_HPP

#include <memory>

namespace vsomeip {
namespace sd {

class service_discovery;
class service_discovery_host;

class runtime {
public:
	static runtime * get();
	virtual ~runtime() {};

	virtual std::shared_ptr< service_discovery > create_service_discovery(service_discovery_host *_host) const = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SD_RUNTIME_HPP

