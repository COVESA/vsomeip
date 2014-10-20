// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER
#define VSOMEIP_ROUTING_MANAGER

#include <memory>
#include <set>
#include <vector>

#include <boost/asio/io_service.hpp>

#include <vsomeip/message.hpp>

namespace vsomeip {

class endpoint;
class endpoint_definition;
class event;
class payload;
class service_info;

class routing_manager {
public:
	virtual ~routing_manager() {};

	virtual boost::asio::io_service & get_io() = 0;
	virtual client_t get_client() const = 0;

	virtual void init() = 0;
	virtual void start() = 0;
	virtual void stop() = 0;

	virtual void offer_service(client_t _client, service_t _service,
			instance_t _instance, major_version_t _major,
			minor_version_t _minor, ttl_t _ttl) = 0;

	virtual void stop_offer_service(client_t _client, service_t _service,
			instance_t _instance) = 0;

	virtual void request_service(client_t _client, service_t _service,
			instance_t _instance, major_version_t _major,
			minor_version_t _minor, ttl_t _ttl) = 0;

	virtual void release_service(client_t _client, service_t _service,
			instance_t _instance) = 0;

	virtual void subscribe(client_t _client, service_t _service,
			instance_t _instance, eventgroup_t _eventgroup,
			major_version_t _major, ttl_t _ttl) = 0;

	virtual void unsubscribe(client_t _client, service_t _service,
			instance_t _instance, eventgroup_t _eventgroup) = 0;

	virtual bool send(client_t _client, std::shared_ptr<message> _message,
			bool _flush, bool _reliable) = 0;

	virtual bool send(client_t _client, const byte_t *_data, uint32_t _size,
			instance_t _instance, bool _flush, bool _reliable) = 0;

	virtual bool send_to(const std::shared_ptr<endpoint_definition> &_target,
			std::shared_ptr<message>) = 0;

	virtual bool send_to(const std::shared_ptr<endpoint_definition> &_target,
			const byte_t *_data, uint32_t _size) = 0;

	virtual void notify(service_t _service, instance_t _instance, event_t _event,
						std::shared_ptr<payload> _payload) const = 0;

	virtual bool is_available(service_t _service,
			instance_t _instance) const = 0;
};

}  // namespace vsomeip

#endif
