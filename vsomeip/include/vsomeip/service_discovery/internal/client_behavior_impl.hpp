//
// client_base_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_BEHAVIOR_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_BEHAVIOR_IMPL_HPP

#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>

namespace vsomeip {
namespace service_discovery {

class client_behavior_impl {
public:
	client_behavior_impl(boost::asio::io_service &_is);

	void init();
	void start();
	void stop();

	template<class Event>
	void process_event(const Event &e);
	virtual void find_service() = 0;

protected:
	struct state_machine;
	boost::shared_ptr< state_machine > state_machine_;

private:
	// This for Meta-programming only...
	virtual void process_event(event_variant e);
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_BEHAVIOR_IMPL_HPP
