//
// eventgroup_service_proxy.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_EVENTGROUP_SERVICE_PROXY_HPP
#define VSOMEIP_INTERNAL_SD_EVENTGROUP_SERVICE_PROXY_HPP

#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

class service_proxy;

class eventgroup_service_proxy {
public:
	eventgroup_service_proxy(service_proxy *_service, eventgroup_id _eventgroup);

	const endpoint * get_multicast() const;
	void set_multicast(const endpoint *_multicast);

	void subscribe(const endpoint *_reliable, const endpoint *_unreliable);
	void unsubscribe(const endpoint *_reliable, const endpoint *_unreliable);

private:
	service_proxy *service_;
	eventgroup_id eventgroup_;

	major_version version_;
	time_to_live ttl_;

	const endpoint *multicast_;

	std::set< const endpoint * > subscribers_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_EVENTGROUP_SERVICE_PROXY_HPP
