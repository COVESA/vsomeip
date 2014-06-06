//
// eventgroup_client_proxy.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/sd/client_proxy.hpp>
#include <vsomeip_internal/sd/eventgroup_client_proxy.hpp>

namespace vsomeip {
namespace sd {

eventgroup_client_proxy::eventgroup_client_proxy(client_proxy *_client, eventgroup_id _eventgroup)
	: client_(_client),
	  eventgroup_(_eventgroup),
	  is_acknowledged_(false),
	  multicast_(0) {
}

bool eventgroup_client_proxy::is_acknowledged() const {
	return is_acknowledged_;
}

void eventgroup_client_proxy::set_acknowledged(bool _is_acknowledged) {
	is_acknowledged_ = _is_acknowledged;
}

const endpoint * eventgroup_client_proxy::get_multicast() const {
	return multicast_;
}

void eventgroup_client_proxy::set_multicast(const endpoint *_multicast) {
	multicast_ = _multicast;
}

void eventgroup_client_proxy::subscribe(client_id _client) {
	subscribers_.insert(_client);
}

void eventgroup_client_proxy::unsubscribe(client_id _client) {
	subscribers_.erase(_client);
}

} // namespace sd
} // namespace vsomeip



