// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_STUB_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_STUB_IMPL_HPP

#include <memory>

#include <boost/asio/io_service.hpp>

#include "service_discovery_stub.hpp"

namespace vsomeip {
namespace sd {

class service_discovery;

class service_discovery_stub_impl: public service_discovery_stub {
public:
	service_discovery_stub_impl(service_discovery *_discovery);
	~service_discovery_stub_impl();

	void init();
	void start();
	void stop();

private:
	boost::asio::io_service &io_;
	service_discovery  *discovery_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_STUB_HPP
