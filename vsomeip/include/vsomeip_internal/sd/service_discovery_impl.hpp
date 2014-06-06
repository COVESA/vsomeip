//
// service_discovery_impl.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_IMPL_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_IMPL_HPP

#include <map>
#include <set>

#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/shared_ptr.hpp>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip_internal/sd/service_discovery.hpp>

namespace vsomeip {

class application;
class daemon;
class endpoint;
class message_base;

namespace sd {

class client_proxy;
class deserializer;
class group;
class service_proxy;

class service_discovery_impl
		: public service_discovery {
public:
	service_discovery_impl(daemon& _owner);
	virtual ~service_discovery_impl();

	void init();
	void start();
	void stop();

	boost::asio::io_service & get_service();

	void on_provide_service(service_id _service, instance_id _instance, const endpoint *_location);
	void on_withdraw_service(service_id _service, instance_id _instance, const endpoint *_location);

	void on_start_service(service_id _service, instance_id _instance);
	void on_stop_service(service_id _service, instance_id _instance);

	void on_request_service(client_id _client, service_id _service, instance_id _instance);
	void on_release_service(client_id _client, service_id _service, instance_id _instance);

	void on_provide_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location);
	void on_withdraw_eventgroup(service_id _service, instance_id _instance, eventgroup_id _eventgroup, const endpoint *_location);

	void on_request_eventgroup(client_id _client, service_id _service, instance_id _instance, eventgroup_id _eventgroup);
	void on_release_eventgroup(client_id _client, service_id _service, instance_id _instance, eventgroup_id _eventgroup);

	void on_service_available(service_id _service, instance_id _instance, const endpoint *_reliable, const endpoint *_unreliable);

	void on_message(const uint8_t *_data, uint32_t _size,
					const endpoint *_source, const endpoint *_target);

	bool send(message *_message);

	bool update_client(client_id _client, service_id _service, instance_id _instance, const endpoint *_location, bool _is_available);

private:
	client_proxy * find_client(service_id _service, instance_id _instance) const;
	client_proxy * find_or_create_client(service_id _service, instance_id _instance);

	service_proxy * find_service(service_id _service, instance_id _instance) const;
	service_proxy * find_or_create_service(service_id _service, instance_id _instance);

	group * find_client_group(service_id _service, instance_id _instance) const;
	group * find_service_group(service_id _service, instance_id _instance) const;

	std::string get_broadcast_address(const std::string &_address, const std::string &_mask) const;

	void on_find_service(const service_entry *_entry, const std::vector< option * > &_options, const endpoint *_source, bool _is_unicast_enabled);
	void on_offer_service(const service_entry *_entry, const std::vector< option * > &_options, const endpoint *_source);

	void on_subscribe_eventgroup(const eventgroup_entry *_entry, const std::vector< option * > &_options, const endpoint *_source);
	void on_subscribe_eventgroup_ack(const eventgroup_entry *_entry, const std::vector< option * > &_options);

private:
	daemon &owner_;
	boost::asio::io_service &service_;
	boost::log::sources::severity_logger<
		boost::log::trivial::severity_level > & logger_;

	std::set< boost::shared_ptr< group > > groups_;
	boost::shared_ptr< group > default_group_;

	// Broadcast address of the network
	const endpoint *broadcast_;

	// Multicast address used to distribute service discovery messages
	const endpoint *multicast_;

	// Session - ID (incremented for each message that is sent to the network)
	session_id session_;

	// Helper to receive and send messages
	boost::shared_ptr< deserializer > deserializer_;
	factory *factory_;
};

} // sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_SERVICE_DISCOVERY_IMPL_HPP
