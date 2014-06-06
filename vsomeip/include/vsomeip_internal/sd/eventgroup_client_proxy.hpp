//
// eventgroup_client_proxy.hpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_INTERNAL_SD_EVENTGROUP_CLIENT_PROXY_HPP
#define VSOMEIP_INTERNAL_SD_EVENTGROUP_CLIENT_PROXY_HPP

#include <set>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

class endpoint;

namespace sd {

class client_proxy;

class eventgroup_client_proxy {
public:
	eventgroup_client_proxy(client_proxy *_client, eventgroup_id _eventgroup);

	const endpoint * get_multicast() const;
	void set_multicast(const endpoint *_multicast);

	bool is_acknowledged() const;
	void set_acknowledged(bool _is_acknowledged);

	void subscribe(client_id _client);
	void unsubscribe(client_id _client);

private:
	client_proxy *client_;
	eventgroup_id eventgroup_;

	bool is_acknowledged_;
	const endpoint *multicast_;

	std::set< client_id > subscribers_;
};

} // namespace sd
} // namespace vsomeip

#endif // VSOMEIP_INTERNAL_SD_EVENTGROUP_SERVICE_PROXY_HPP
