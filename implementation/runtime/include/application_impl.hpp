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

namespace vsomeip {

class configuration;
class logger;
class routing_manager;
class routing_manager_stub;

class application_impl: public application,
		public routing_manager_host,
		public std::enable_shared_from_this<application_impl> {
public:
	application_impl(const std::string &_name);
	~application_impl();

	bool init();
	void start();
	void stop();

	void offer_service(service_t _service, instance_t _instance,
			major_version_t _major, minor_version_t _minor, ttl_t _ttl);

	void stop_offer_service(service_t _service, instance_t _instance);

	// Consume services
	void request_service(service_t _service, instance_t _instance,
			major_version_t _major, minor_version_t _minor, ttl_t _ttl);
	void release_service(service_t _service, instance_t _instance);

	void subscribe(service_t _service, instance_t _instance,
			eventgroup_t _eventgroup, major_version_t _major, ttl_t _ttl);

	void unsubscribe(service_t _service, instance_t _instance,
			eventgroup_t _eventgroup);

	bool is_available(service_t _service, instance_t _instance);

	void send(std::shared_ptr<message> _message, bool _flush, bool _reliable);
	void set(service_t _service, instance_t _instance, event_t _event,
			const std::shared_ptr<payload> &_payload);

	void register_event_handler(event_handler_t _handler);
	void unregister_event_handler();

	void register_message_handler(service_t _service, instance_t _instance,
			method_t _method, message_handler_t _handler);
	void unregister_message_handler(service_t _service, instance_t _instance,
			method_t _method);

	void register_availability_handler(service_t _service, instance_t _instance,
			availability_handler_t _handler);
	void unregister_availability_handler(service_t _service,
			instance_t _instance);

	// routing_manager_host
	const std::string & get_name() const;
	client_t get_client() const;
	std::shared_ptr<configuration> get_configuration() const;
	boost::asio::io_service & get_io();

	void on_event(event_type_e _event);
	void on_availability(service_t _service, instance_t _instance,
			bool _is_available) const;
	void on_message(std::shared_ptr<message> _message);
	void on_error(error_code_e _error);

	// service_discovery_host
	routing_manager * get_routing_manager() const;

private:
	void service(boost::asio::io_service &_io);

private:
	client_t client_; // unique application identifier
	session_t session_;

	std::string name_;
	std::shared_ptr<configuration> configuration_;

	boost::asio::io_service host_io_;

	// Proxy to or the Routing Manager itself
	std::shared_ptr<routing_manager> routing_;

	// (Non-SOME/IP) Event handler
	event_handler_t handler_;

	// Method/Event (=Member) handlers
	std::map<service_t,
			std::map<instance_t, std::map<method_t, message_handler_t> > > members_;

	// Availability handlers
	std::map<service_t, std::map<instance_t, availability_handler_t> > availability_;

	// Signals
	boost::asio::signal_set signals_;
};

} // namespace vsomeip

#endif // VSOMEIP_APPLICATION_IMPL_HPP
