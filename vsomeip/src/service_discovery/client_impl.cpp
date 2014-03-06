//
// client_impl.cpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright �� 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#include <vsomeip/factory.hpp>
#include <vsomeip/endpoint.hpp>
#include <vsomeip/internal/byteorder.hpp>
#include <vsomeip/internal/tcp_provider_impl.hpp>
#include <vsomeip/internal/udp_provider_impl.hpp>
#include <vsomeip/service_discovery/internal/client_impl.hpp>
#include <vsomeip/service_discovery/internal/consumer_impl.hpp>
#include <vsomeip/service_discovery/internal/provider_impl.hpp>

namespace vsomeip {
namespace service_discovery {

client_impl::~client_impl() {
}

consumer * client_impl::create_consumer(
				service_id _service, instance_id _instance) {
	consumer * new_consumer = 0;
	service_instance identifier = VSOMEIP_WORDS_TO_LONG(_service, _instance);

	std::map<service_instance, consumer*>::iterator found
		= consumers_.find(identifier);
	if (found == consumers_.end()) {
		new_consumer = new consumer_impl(_service, _instance, is_);
		consumers_[identifier] = new_consumer;
	} else {
		new_consumer = found->second;
	}

	return new_consumer;
}

provider * client_impl::create_provider(
		service_id _service, instance_id _instance, const endpoint *_source) {
	provider * new_provider = 0;
	std::map<const endpoint*, vsomeip::provider*>::iterator found
		= providers_.find(_source);
	if (found == providers_.end()) {
		vsomeip::provider * delegate = create_provider(_source);
		new_provider = new provider_impl(delegate, is_);
		providers_[_source] = new_provider;
	} else {
		new_provider = dynamic_cast<provider_impl *>(found->second);
	}

	return new_provider;
}

provider * client_impl::create_provider(const endpoint *_source) {
	provider * new_provider = 0;
	std::map<const endpoint*, provider*>::iterator found = providers_.find(_source);
	if (found == providers_.end()) {
		if (ip_protocol::UDP == _source->get_protocol())
			new_provider = new udp_provider_impl(
									factory::get_default_factory(),
									_source, is_);

		else if (ip_protocol::TCP == _source->get_protocol())
			new_provider = new tcp_provider_impl(
									factory::get_default_factory(),
									_source, is_);

		providers_[_source] = new_provider;
	} else {
		new_provider = found->second;
	}

	return new_provider;
}

} // namespace service_discovery
} // namespace vsomeip

