// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ROUTING_MANAGER_IMPL_HPP
#define VSOMEIP_ROUTING_MANAGER_IMPL_HPP

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/asio/io_service.hpp>

#include "routing_manager.hpp"
#include "../../endpoints/include/endpoint_host.hpp"
#include "../../service_discovery/include/service_discovery_host.hpp"

namespace vsomeip {

class client_endpoint;
class configuration;
class deserializer;
class routing_manager_host;
class routing_manager_stub;
class servicegroup;
class serviceinfo;
class serializer;
class service_endpoint;

namespace sd {

class service_discovery;

} // namespace sd

class routing_manager_impl:
		public routing_manager,
		public endpoint_host,
		public sd::service_discovery_host,
		public std::enable_shared_from_this< routing_manager_impl > {
public:
	routing_manager_impl(routing_manager_host *_host);
	~routing_manager_impl();

	boost::asio::io_service & get_io();
	std::shared_ptr< configuration > get_configuration() const;

	void init();
	void start();
	void stop();

	void offer_service(client_t _client,
			service_t _service, instance_t _instance,
			major_version_t _major, minor_version_t _minor, ttl_t _ttl);

	void stop_offer_service(client_t _client,
			service_t _service, instance_t _instance);


	void publish_eventgroup(client_t _client,
			service_t _service, instance_t _instance,
			eventgroup_t _eventgroup,
			major_version_t _major, ttl_t _ttl);

	void stop_publish_eventgroup(client_t _client,
			service_t _service, instance_t _instance,
			eventgroup_t _eventgroup);

	std::shared_ptr< event > add_event(client_t _client,
			service_t _service, instance_t _instance,
			eventgroup_t _eventgroup, event_t _event);

	std::shared_ptr< event > add_field(client_t _client,
			service_t _service, instance_t _instance,
			eventgroup_t _eventgroup, event_t _event,
			std::shared_ptr< payload > _payload);

	void remove_event_or_field(std::shared_ptr< event > _event);

	void request_service(client_t _client,
			service_t _service, instance_t _instance,
			major_version_t _major, minor_version_t _minor, ttl_t _ttl);

	void release_service(client_t _client, service_t _service, instance_t _instance);

	void subscribe(client_t _client,
			service_t _service, instance_t _instance, eventgroup_t _eventgroup);

	void unsubscribe(client_t _client,
			service_t _service, instance_t _instance, eventgroup_t _eventgroup);

	void send(client_t _client,
			std::shared_ptr< message > _message, bool _reliable, bool _flush);

	void send(client_t _client,
			const byte_t *_data, uint32_t _size, instance_t _instance, bool _flush, bool _reliable);

	void set(client_t _client,
			service_t _service, instance_t _instance,
	      	event_t _event, const std::vector< byte_t > &_value);

	void on_message(const byte_t *_data, length_t _length, endpoint *_receiver);

	endpoint * find_local(client_t _client);
	endpoint * find_or_create_local(client_t _client);
	void remove_local(client_t _client);
	endpoint * find_local(service_t _service, instance_t _instance);

	const std::map< std::string, std::shared_ptr< servicegroup > > & get_servicegroups() const;

private:
	void on_message(const byte_t *_data, length_t _length, instance_t _instance);

	client_t find_client(service_t _service, instance_t _instance);
	instance_t find_instance(service_t _service, endpoint *_endpoint);

	serviceinfo * find_service(service_t _service, instance_t _instance);
	void create_service(service_t _service, instance_t _instance,
						major_version_t _major, minor_version_t _minor, ttl_t _ttl);

	endpoint * find_service_endpoint(uint16_t _port, bool _reliable);
	endpoint * create_service_endpoint(uint16_t _port, bool _reliable);
	endpoint * find_or_create_service_endpoint(uint16_t _port, bool _reliable);

	endpoint * create_local(client_t _client);

private:
	boost::asio::io_service &io_;
	routing_manager_host *host_;

	std::shared_ptr< configuration > configuration_;

	std::shared_ptr< deserializer > deserializer_;
	std::shared_ptr< serializer > serializer_;

	std::shared_ptr< routing_manager_stub > stub_;
	std::shared_ptr< sd::service_discovery > discovery_;

	// Routing info
	std::map< client_t, std::shared_ptr< endpoint > > local_clients_;
	std::map< service_t, std::map< instance_t, client_t > > local_services_;

	std::map< client_t, std::shared_ptr< endpoint > > clients_;
	std::map< uint16_t, std::map< bool, std::shared_ptr< endpoint > > > service_endpoints_;
	std::map< service_t, std::map< endpoint *, instance_t > > service_instances_;

	// Servicegroups
	std::map< std::string, std::shared_ptr< servicegroup > > servicegroups_;
	std::map< service_t, std::map< instance_t, std::shared_ptr< serviceinfo > > > services_;

	// Eventgroups
	std::map< service_t,
			  std::map< instance_t,
			  	  	    std::map< eventgroup_t,
			  	  	    		  std::set< std::shared_ptr< endpoint > > > > > eventgroups_;
	std::map< service_t, std::map< instance_t, std::map< event_t, eventgroup_t > > > events_;

	std::recursive_mutex endpoint_mutex_;
	std::mutex serialize_mutex_;
};

} // namespace vsomeip

#endif // VSOMEIP_ROUTING_MANAGER_IMPL_HPP
