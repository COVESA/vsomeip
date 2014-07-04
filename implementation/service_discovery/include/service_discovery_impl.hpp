// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL
#define VSOMEIP_SERVICE_DISCOVERY_IMPL

#include <memory>
#include <set>

#include "service_discovery.hpp"
#include "../../endpoints/include/endpoint_host.hpp"

namespace vsomeip {
namespace sd {

class servicegroup;
class service_discovery_host;

class service_discovery_impl:
		public service_discovery,
		public endpoint_host {
public:
	service_discovery_impl(service_discovery_host *_host);
	virtual ~service_discovery_impl();

	boost::asio::io_service & get_io();

	void init();
	void start();
	void stop();

	void offer_service(service_t _service, instance_t _instance);
	void stop_offer_service(service_t _service, instance_t _instance);

	void request_service(service_t _service, instance_t _instance,
			major_version_t _major, minor_version_t _minor, ttl_t _ttl);
	void release_service(service_t _service, instance_t _instance);

	// endpoint_host
	void on_message(const byte_t *_data, length_t _length, endpoint *_receiver);

private:
	boost::asio::io_service &io_;
	service_discovery_host *host_;

	std::set< std::unique_ptr< servicegroup > > groups_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL
