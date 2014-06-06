//
// eventgroup_service_proxy.cpp
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip_internal/sd/eventgroup_service_proxy.hpp>

namespace vsomeip {
namespace sd {

eventgroup_service_proxy::eventgroup_service_proxy(service_proxy *_service, eventgroup_id _eventgroup)
	: service_(_service),
	  eventgroup_(_eventgroup) {
}

const endpoint * eventgroup_service_proxy::get_multicast() const {
	return multicast_;
}

void eventgroup_service_proxy::set_multicast(const endpoint *_multicast) {
	multicast_ = _multicast;
}

void eventgroup_service_proxy::subscribe(const endpoint *_reliable, const endpoint *_unreliable) {
	if (0 != multicast_ && 0 == _reliable) {
		subscribers_.insert(_unreliable);
	} else {
		subscribers_.insert(_reliable);
	}
}

void eventgroup_service_proxy::unsubscribe(const endpoint *_reliable, const endpoint *_unreliable) {
	if (0 != _reliable) subscribers_.erase(_reliable);
	if (0 != _unreliable) subscribers_.erase(_unreliable);
}

} // namespace sd
} // namespace vsomeip



