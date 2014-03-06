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
#include <vsomeip/internal/client_impl.hpp>
#include <vsomeip/internal/endpoint_impl.hpp>
#include <vsomeip/internal/tcp_consumer_impl.hpp>
#include <vsomeip/internal/tcp_provider_impl.hpp>
#include <vsomeip/internal/udp_consumer_impl.hpp>
#include <vsomeip/internal/udp_provider_impl.hpp>

namespace vsomeip {

client_impl::~client_impl() {
}

consumer * client_impl::create_consumer(const endpoint *_target) {
	consumer * new_consumer = 0;
	std::map<const endpoint*, consumer*>::iterator found = consumers_.find(_target);
	if (found == consumers_.end()) {
		if (ip_protocol::UDP == _target->get_protocol())
			new_consumer = new udp_consumer_impl(
									factory::get_default_factory(),
									_target, is_);

		else if (ip_protocol::TCP == _target->get_protocol())
			new_consumer = new tcp_consumer_impl(
									factory::get_default_factory(),
									_target, is_);

		consumers_[_target] = new_consumer;
	} else {
		new_consumer = found->second;
	}

	return new_consumer;
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

} // namespace vsomeip

