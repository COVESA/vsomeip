// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_EVENT_IMPL_HPP
#define VSOMEIP_EVENT_IMPL_HPP

#include <memory>
#include <mutex>

#include <boost/asio/io_service.hpp>
#include <boost/asio/system_timer.hpp>

#include <vsomeip/event.hpp>

namespace vsomeip {

class message;

class event_impl: public event {
public:
	event_impl(boost::asio::io_service &_io);
	virtual ~event_impl();

	service_t get_service() const;
	void set_service(service_t _service);

	instance_t get_instance() const;
	void set_instance(instance_t _instance);

	event_t get_event() const;
	void set_event(event_t _event);

	std::shared_ptr< payload > get_payload() const;
	void set_payload(std::shared_ptr< payload > _payload);

	// SIP_RPC_357
	void set_update_cycle(std::chrono::milliseconds &_cycle);

	// SIP_RPC_358
	void set_update_on_change(bool _is_on);

	// SIP_RPC_359 (epsilon change) is not supported!

private:
	std::mutex mutex_;
	std::shared_ptr< message > update_;

	boost::asio::system_timer cycle_timer_;
	std::chrono::milliseconds cycle_;

	bool is_updating_on_change_;
};

} // namespace vsomeip

#endif // VSOMEIP_EVENT_IMPL_HPP
