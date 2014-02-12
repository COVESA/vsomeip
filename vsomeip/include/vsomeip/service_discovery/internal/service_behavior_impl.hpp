//
// service_behavior_impl.hpp
//
// Author: Lutz Bichler <Lutz.Bichler@bmwgroup.com>
//
// This file is part of the BMW Some/IP implementation.
//
// Copyright © 2013, 2014 Bayerische Motoren Werke AG (BMW).
// All rights reserved.
//

#ifndef VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_BEHAVIOR_IMPL_HPP
#define VSOMEIP_SERVICE_DISCOVERY_INTERNAL_SERVICE_BEHAVIOR_IMPL_HPP

#include <boost/asio/io_service.hpp>
#include <boost/shared_ptr.hpp>
#include <vsomeip/service_discovery/internal/events.hpp>

namespace vsomeip {
namespace service_discovery {

class service_behavior_impl {
public:
	service_behavior_impl(boost::asio::io_service &_is);

	void init();
	void start();
	void stop();

	template<class Event>
	void process_event(const Event &e);
	virtual void offer_service() = 0;
	virtual void stop_offer_service() = 0;

protected:
	struct state_machine;
	boost::shared_ptr< state_machine > state_machine_;

// the meta programming part ....
private:
	typedef boost::variant<
			ev_timer_expired,
			ev_daemon_status_change,
			ev_service_status_change,
			ev_request_change,
			ev_find_service > event_variant;
	virtual void process_event(event_variant e);
};

} // namespace service_discovery
} // namespace vsomeip

#endif // VSOMEIP_SERVICE_DISCOVERY_INTERNAL_CLIENT_BEHAVIOR_IMPL_HPP
