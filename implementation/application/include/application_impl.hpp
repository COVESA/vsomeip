// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_APPLICATION_IMPL_HPP
#define VSOMEIP_APPLICATION_IMPL_HPP

#include <map>
#include <string>

#include <boost/asio/signal_set.hpp>

#include <vsomeip/application.hpp>

#include "../../routing/include/routing_manager_host.hpp"
#include "../../service_discovery/include/service_discovery_host.hpp"

namespace vsomeip {

class configuration;
class logger;
class routing_manager;
class routing_manager_stub;

namespace sd {
class service_discovery;
class service_discovery_stub;
} // namespace sd

class application_impl
		: public application,
		  public routing_manager_host,
		  public sd::service_discovery_host,
		  public std::enable_shared_from_this< application_impl > {
public:
	application_impl(const std::string &_name);
	~application_impl();

	bool init(int _argc, char **_argv);
	void start();
	void stop();

	void offer_service(service_t _service, instance_t _instance,
					   major_version_t _major, minor_version_t _minor, ttl_t _ttl);

	void stop_offer_service(service_t _service, instance_t _instance);

	void publish_eventgroup(service_t _service, instance_t _instance,
							eventgroup_t _eventgroup,
							major_version_t _major, ttl_t _ttl);

	void stop_publish_eventgroup(service_t _service, instance_t _instance,
								 eventgroup_t _eventgroup);

	// Consume services
	void request_service(service_t _service, instance_t _instance,
					major_version_t _major, minor_version_t _minor,
					ttl_t _ttl);
	void release_service(service_t _service, instance_t _instance);

	void subscribe(service_t _service, instance_t _instance,
				   eventgroup_t _eventgroup);

	void unsubscribe(service_t _service, instance_t _instance,
					 eventgroup_t _eventgroup);

	bool is_available(service_t _service, instance_t _instance);

	// Define content of eventgroups
	void add_event(service_t _service, instance_t _instance,
				   eventgroup_t _eventgroup, event_t _event);

	void add_field(service_t _service, instance_t _instance,
				   eventgroup_t _eventgroup, event_t _event,
				   std::vector< uint8_t > &_value);

	void remove_event_or_field(service_t _service, instance_t _instance,
							   eventgroup_t _eventgroup, event_t _event);


	void send(std::shared_ptr< message > _message, bool _flush, bool _reliable);
	void set(service_t _service, instance_t _instance,
			 event_t _event, std::vector< byte_t > &_value);

	bool register_message_handler(service_t _service, instance_t _instance,
			method_t _method, message_handler_t _handler);
	bool unregister_message_handler(service_t _service, instance_t _instance,
			method_t _method);

	bool register_availability_handler(service_t _service, instance_t _instance,
			availability_handler_t _handler);
	bool unregister_availability_handler(service_t _service, instance_t _instance);

	// routing_manager_host
	const std::string & get_name() const;
	client_t get_client() const;
	std::shared_ptr< configuration > get_configuration() const;
	boost::asio::io_service & get_io();
	void on_availability(service_t _service, instance_t _instance, bool _is_available) const;
	void on_message(std::shared_ptr< message > _message);
	void on_error();

	// service_discovery_host

private:
	void service(boost::asio::io_service &_io);

private:
	client_t client_; // unique application identifier
	session_t session_;

	std::string name_;
	std::shared_ptr< configuration > configuration_;

	boost::asio::io_service host_io_;

	// Proxy to or the Routing Manager itself
	std::shared_ptr< routing_manager > routing_;

	// Stub to export the Routing Manager
	std::shared_ptr< routing_manager_stub > routing_stub_;

	// Proxy to or the Service Discovery itself
	std::shared_ptr< sd::service_discovery > discovery_;

	// Stub to export the Service Discovery
	std::shared_ptr< sd::service_discovery_stub > discovery_stub_;

	// Method/Event (=Member) handlers
	std::map< service_t,
	          std::map< instance_t,
	                    std::map< method_t,
	                              message_handler_t > > > members_;

	// Availability handlers
	std::map< service_t,
			  std::map< instance_t,
			  	  	    availability_handler_t > > availability_;

	// Signals
	boost::asio::signal_set signals_;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_IMPL_HPP
