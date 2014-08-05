// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_PROXY_HPP
#define VSOMEIP_ROUTING_MANAGER_PROXY_HPP

#include <map>
#include <mutex>

#include <boost/asio/io_service.hpp>

#include "routing_manager.hpp"
#include "../../endpoints/include/endpoint_host.hpp"

namespace vsomeip {

class configuration;
class routing_manager_host;

class routing_manager_proxy: public routing_manager,
		public endpoint_host,
		public std::enable_shared_from_this<routing_manager_proxy> {
public:
	routing_manager_proxy(routing_manager_host *_host);
	virtual ~routing_manager_proxy();

	boost::asio::io_service & get_io();

	void init();
	void start();
	void stop();

	void offer_service(client_t _client, service_t _service,
			instance_t _instance, major_version_t _major,
			minor_version_t _minor, ttl_t _ttl);

	void stop_offer_service(client_t _client, service_t _service,
			instance_t _instance);

	void request_service(client_t _client, service_t _service,
			instance_t _instance, major_version_t _major,
			minor_version_t _minor, ttl_t _ttl);

	void release_service(client_t _client, service_t _service,
			instance_t _instance);

	void subscribe(client_t _client, service_t _service, instance_t _instance,
			eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl);

	void unsubscribe(client_t _client, service_t _service, instance_t _instance,
			eventgroup_t _eventgroup);

	bool send(client_t _client, std::shared_ptr<message> _message, bool _flush,
	bool _reliable);

	bool send(client_t _client, const byte_t *_data, uint32_t _size,
			instance_t _instance, bool _flush = true, bool _reliable = false);

	bool send_to(const std::shared_ptr<endpoint_definition> &_target,
			std::shared_ptr<message> _message);

	bool send_to(const std::shared_ptr<endpoint_definition> &_target,
			const byte_t *_data, uint32_t _size);

	bool get(client_t _client, session_t _session,
			service_t _service,	instance_t _instance, event_t _event);

	bool set(client_t _client, session_t _session,
			service_t _service,	instance_t _instance, event_t _event,
			const std::shared_ptr<payload> &_value);

	void on_connect(std::shared_ptr<endpoint> _endpoint);
	void on_disconnect(std::shared_ptr<endpoint> _endpoint);
	void on_message(const byte_t *_data, length_t _length, endpoint *_receiver);

	void on_routing_info(const byte_t *_data, uint32_t _size);

	bool is_available(service_t _service, instance_t _instance) const;

	std::shared_ptr<endpoint> find_local(client_t _client);
	std::shared_ptr<endpoint> find_local(service_t _service,
			instance_t _instance);
	std::shared_ptr<endpoint> find_or_create_local(client_t _client);
	void remove_local(client_t _client);

private:
	void register_application();
	void deregister_application();

	std::shared_ptr<endpoint> create_local(client_t _client);

	bool is_request(byte_t _message_type) const;

	void send_pong() const;

private:
	boost::asio::io_service &io_;
	routing_manager_host *host_;
	client_t client_; // store locally as it is needed in each message

	std::shared_ptr<serializer> serializer_;
	std::shared_ptr<deserializer> deserializer_;

	std::shared_ptr<endpoint> sender_;	// --> stub
	std::shared_ptr<endpoint> receiver_;  // --> from everybody

	std::map<client_t, std::shared_ptr<endpoint> > local_endpoints_;
	std::map<service_t, std::map<instance_t, client_t> > local_services_;

	std::mutex send_mutex_;
	std::mutex serialize_mutex_;
	std::mutex deserialize_mutex_;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_PROXY_HPP
