//
// client_proxy.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_CLIENT_PROXY_HPP
#define VSOMEIP_INTERNAL_SD_CLIENT_PROXY_HPP

#include <map>
#include <set>

#include <boost/shared_ptr.hpp>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

class eventgroup_client_proxy;
class group;

class client_proxy {
public:
	client_proxy(group *_group, service_id _service, instance_id _instance);

	service_id get_service() const;
	instance_id get_instance() const;
	major_version get_major_version() const;
	minor_version get_minor_version() const;
	time_to_live get_time_to_live() const;

	bool is_available() const;
	void set_available(const endpoint *_tcp, const endpoint *_udp, bool _is_available);

	void request(client_id _client);
	void release(client_id _client);

	void subscribe(client_id _client, eventgroup_id _eventgroup);
	void unsubscribe(client_id _client, eventgroup_id _eventgroup);

	const std::map< eventgroup_id, boost::shared_ptr< eventgroup_client_proxy > > & get_eventgroups() const;

	void set_local(const endpoint *_reliable, const endpoint *_unreliable);

private:
	void update_clients(const endpoint *_location, bool _is_available);

private:
	group *group_;

	service_id service_;
	instance_id instance_;

	const endpoint *reliable_local_;
	const endpoint *reliable_remote_;

	const endpoint *unreliable_local_;
	const endpoint *unreliable_remote_;

	std::set< client_id > requesters_;
	std::map< eventgroup_id, boost::shared_ptr< eventgroup_client_proxy > > eventgroups_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_CLIENT_PROXY_HPP
