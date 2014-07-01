// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_PROXY
#define VSOMEIP_SERVICE_DISCOVERY_PROXY

#include <memory>

#include "service_discovery.hpp"

namespace vsomeip {
namespace sd {

class service_discovery_host;

class service_discovery_proxy: public service_discovery {
public:
	service_discovery_proxy(service_discovery_host *_host);
	virtual ~service_discovery_proxy();

	boost::asio::io_service & get_io();

	void init();
	void start();
	void stop();

private:
	boost::asio::io_service &io_;
	service_discovery_host *host_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_PROXY




