//
// daemon.cpp
//
// Author: 	Lutz Bichler
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <algorithm>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/thread.hpp>

#include <vsomeip/provider.hpp>
#include <vsomeip/internal/endpoint_impl.hpp>
#include <vsomeip/internal/tcp_provider_impl.hpp>
#include <vsomeip/internal/udp_provider_impl.hpp>
#include <vsomeip/service_discovery/configuration.hpp>
#include <vsomeip/service_discovery/factory.hpp>
#include <vsomeip/service_discovery/message.hpp>
#include <vsomeip/service_discovery/entry.hpp>
#include <vsomeip/service_discovery/eventgroup_entry.hpp>
#include <vsomeip/service_discovery/service_entry.hpp>
#include <vsomeip/service_discovery/internal/daemon-config.hpp>
#include <vsomeip/service_discovery/internal/daemon.hpp>
#include <vsomeip/service_discovery/internal/service_administrator.hpp>

#define SERVICE_DISCOVERY_SERVICE_ID 0xFFFF
#define SERVICE_DISCOVERY_METHOD_ID  0x8100

using namespace boost::log::trivial;

namespace vsomeip {
namespace service_discovery {

daemon*
daemon::get_instance() {
	static daemon daemon__;
	return &daemon__;
}

daemon::daemon()
	: acceptor_(is_) {
	port_ = VSOMEIP_VALUE_PORT_DEFAULT;
	registry_path_ = VSOMEIP_VALUE_REGISTRY_PATH_DEFAULT;
	use_virtual_mode_ = VSOMEIP_VALUE_IS_VIRTUAL_MODE_DEFAULT;
	use_service_discovery_
		= VSOMEIP_VALUE_IS_SERVICE_DISCOVERY_MODE_DEFAULT;
}

void daemon::init(int argc, char **argv) {
	vsomeip::service_discovery::configuration *c
		= vsomeip::service_discovery::configuration::get_instance();
	c->init(argc, argv);

	BOOST_LOG_SEV(log_, info)
		<< "Using configuration file " << c->get_configuration_file_path();

	use_virtual_mode_ = c->use_virtual_mode();
	use_service_discovery_ = c->use_service_discovery();
	port_ = 30490; //c->get_service_discovery_port();
}

void daemon::start() {
	// Check whether the daemon makes any sense
	if (!use_virtual_mode_ && !use_service_discovery_) {
		BOOST_LOG_SEV(log_, error)<< "Neither virtual mode nor service discovery "
		"mode active, exiting.";
		stop();
	} else {
		BOOST_LOG_SEV(log_, info) << "Using port " << port_;
		//BOOST_LOG_DEV(log_, info) << "Using log level " << _log_level;

		if (use_virtual_mode_) {
			BOOST_LOG_SEV(log_, info) << "Virtual mode: on";
		}

		if (use_service_discovery_) {
			BOOST_LOG_SEV(log_, info) << "Service discovery mode: on";
		}

		// Enable internal registry interface
		::unlink(registry_path_.c_str());
		registry_ = new service_registry(this);

		// create a UDP & a TCP service as we want to listen to a port and
		// send/receive Some/IP messages
		endpoint *udp_endpoint = new endpoint_impl("127.0.0.1", port_,
				ip_protocol::UDP, ip_version::V4);

		endpoint *tcp_endpoint = new endpoint_impl("127.0.0.1", port_,
				ip_protocol::TCP, ip_version::V4);

		factory *default_factory = factory::get_default_factory();

		udp_daemon_	= new udp_provider_impl(default_factory, udp_endpoint, is_);
		tcp_daemon_	= new tcp_provider_impl(default_factory, tcp_endpoint, is_);

		tcp_daemon_->register_for(this,
				SERVICE_DISCOVERY_SERVICE_ID,
				SERVICE_DISCOVERY_METHOD_ID);
		udp_daemon_->register_for(this,
				SERVICE_DISCOVERY_SERVICE_ID,
				SERVICE_DISCOVERY_METHOD_ID);

		udp_daemon_->start();
		tcp_daemon_->start();

		boost::thread io_thread(boost::bind(&daemon::run_service, this));

		is_running_ = true;

		io_thread.join();
	}
}

void daemon::stop() {
	is_running_ = false;
}

void daemon::run_service() {
	while (is_running_) {
		BOOST_LOG_SEV(log_, info)<< "(Re-)Starting service";
		is_.run();
	}
}

boost::asio::io_service & daemon::get_is() {
	return is_;
}

void daemon::receive(const message_base *_message) {
	const message *requests = dynamic_cast<const message *>(_message);
	if (0 != requests) {
		const std::vector<entry *>& entries = requests->get_entries();
		for (auto e : entries) {
			consume_request(*e, _message->get_endpoint());
		}
	} else {
		BOOST_LOG_SEV(log_, warning)<< "Received unstructued Some/IP service discovery message!";
	}
}

bool daemon::send(const message_base *_message) {
	return udp_daemon_->send(_message);
}

void daemon::consume_request(const entry &_entry, endpoint *_target) {
	if (_entry.get_type() == entry_type::FIND_SERVICE) {
		const service_entry& s = reinterpret_cast<const service_entry &>(_entry);
		/* service_behavior_impl *found = registry_->find(s.get_service_id(), s.get_instance_id());

		if (found) {
			//found->process_event(ev_find_service(_target));
		} else {
			BOOST_LOG_SEV(log_, warning)
					<< "Service " << std::hex
					<< (int)s.get_service_id()
					<< "."
					<< (int)s.get_instance_id()
					<< " not available!";
		} */
	}
}

} // namespace service_discovery
} // namespace vsomeip

