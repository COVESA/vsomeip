// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_STUB_HPP
#define VSOMEIP_SERVICE_DISCOVERY_STUB_HPP

#include <memory>

#include <boost/asio/io_service.hpp>

namespace vsomeip {
namespace sd {

class service_discovery;

class service_discovery_stub {
public:
	virtual ~service_discovery_stub() {};

	virtual void init() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_STUB_HPP
