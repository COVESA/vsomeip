//
// service_proxy.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright ������ 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_SERVICE_PROXY_HPP
#define VSOMEIP_INTERNAL_SD_SERVICE_PROXY_HPP

#include <map>
#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

class group;
class eventgroup_service_proxy;

class service_proxy {
public:
	service_proxy(group *_group, service_id _service, instance_id _instance);

	service_id get_service() const;
	instance_id get_instance() const;
	major_version get_major_version() const;
	minor_version get_minor_version() const;
	time_to_live get_time_to_live() const;

	const endpoint * get_reliable() const;
	const endpoint * get_unreliable() const;

	bool is_local() const;
	bool is_started() const;

	void provide(const endpoint *_location);
	void withdraw(const endpoint *_location);

	void start();
	void stop();

	void provide_eventgroup(eventgroup_id _eventgroup, const endpoint *_location);
	void withdraw_eventgroup(eventgroup_id _eventgroup);

	void subscribe(eventgroup_id _eventgroup, const endpoint *_source, const endpoint *_reliable, const endpoint *_unreliable, bool _stop);

	const std::set< client_id > & get_requesters() const;
	void add_requester(client_id _client);
	void remove_requester(client_id _client);

private:
	group *group_;

	service_id service_;
	instance_id instance_;
	major_version major_version_;
	minor_version minor_version_;
	time_to_live ttl_;

	bool is_local_;
	bool is_started_;

	const endpoint *unreliable_;
	const endpoint *reliable_;

	std::map< eventgroup_id, boost::shared_ptr< eventgroup_service_proxy > > eventgroups_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_SERVICE_STATE_HPP
