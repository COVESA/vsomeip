// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/message.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/event_impl.hpp"

namespace vsomeip {

event_impl::event_impl(boost::asio::io_service &_io)
	: update_(runtime::get()->create_notification()),
	  cycle_timer_(_io),
	  is_updating_on_change_(false) {
}

event_impl::~event_impl() {
}

service_t event_impl::get_service() const {
	return update_->get_service();
}

void event_impl::set_service(service_t _service) {
	update_->set_service(_service);
}

instance_t event_impl::get_instance() const {
	return update_->get_instance();
}

void event_impl::set_instance(instance_t _instance) {
	update_->set_instance(_instance);
}

event_t event_impl::get_event() const {
	return update_->get_method();
}

void event_impl::set_event(event_t _event) {
	update_->set_method(_event); // TODO: maybe we should check for the leading 0-bit
}

std::shared_ptr< payload > event_impl::get_payload() const {
	return update_->get_payload();
}

void event_impl::set_payload(std::shared_ptr< payload > _payload) {
	std::unique_lock< std::mutex > its_lock(mutex_);

}

void event_impl::set_update_on_change(bool _is_active) {
	is_updating_on_change_ = _is_active;
}

void event_impl::set_update_cycle(std::chrono::milliseconds &_cycle) {
	cycle_ = _cycle;
	cycle_timer_.cancel();

	if (std::chrono::milliseconds::zero() != _cycle) {
		cycle_timer_.expires_from_now(cycle_);

		std::function< void (boost::system::error_code const &) > its_handler
					= [this, its_handler] (boost::system::error_code const &_error) {
							// TODO: Tell the RM[I|P] to do the update
							cycle_timer_.expires_from_now(cycle_);
							cycle_timer_.async_wait(its_handler);
					  };

		cycle_timer_.async_wait(its_handler);
	}
}

} // namespace vsomeip
