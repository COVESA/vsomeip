// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_SERVICE_DISCOVERY_IMPL
#define VSOMEIP_SERVICE_DISCOVERY_IMPL

#include <map>
#include <memory>
#include <set>

#include "service_discovery.hpp"
#include "../../endpoints/include/endpoint_host.hpp"
#include "../../routing/include/routing_types.hpp"

namespace vsomeip {

class endpoint;

namespace sd {

class service_discovery_fsm;
class service_discovery_host;

class service_discovery_impl:
		public service_discovery,
		public endpoint_host,
		public std::enable_shared_from_this< service_discovery_impl > {
public:
	service_discovery_impl(service_discovery_host *_host);
	virtual ~service_discovery_impl();

	std::shared_ptr< configuration > get_configuration() const;
	boost::asio::io_service & get_io();

	void init();
	void start();
	void stop();

	void offer_service(service_t _service, instance_t _instance);
	void stop_offer_service(service_t _service, instance_t _instance);

	void request_service(service_t _service, instance_t _instance,
			major_version_t _major, minor_version_t _minor, ttl_t _ttl);
	void release_service(service_t _service, instance_t _instance);

	void send(const std::string &_name, bool _is_announcing);

	// endpoint_host
	void on_message(const byte_t *_data, length_t _length, endpoint *_receiver);

private:
	void insert_service_entries(std::shared_ptr< message_impl > &_message,
			service_map_t &_services, bool _is_offer);

private:
	boost::asio::io_service &io_;
	service_discovery_host *host_;

	std::shared_ptr< service_discovery_fsm > default_;
	std::map< std::string,
			  std::shared_ptr< service_discovery_fsm > > additional_;

	service_map_t requested_;

	std::shared_ptr< endpoint > endpoint_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_IMPL
