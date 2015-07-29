// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/event.hpp"
#include "../include/routing_manager.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../message/include/payload_impl.hpp"

namespace vsomeip {

event::event(routing_manager *_routing) :
        routing_(_routing),
        message_(runtime::get()->create_notification()),
        cycle_timer_(_routing->get_io()),
        is_updating_on_change_(true),
        is_set_(false) {
}

service_t event::get_service() const {
    return (message_->get_service());
}

void event::set_service(service_t _service) {
    message_->set_service(_service);
}

instance_t event::get_instance() const {
    return (message_->get_instance());
}

void event::set_instance(instance_t _instance) {
    message_->set_instance(_instance);
}

event_t event::get_event() const {
    return (message_->get_method());
}

void event::set_event(event_t _event) {
    message_->set_method(_event); // TODO: maybe we should check for the leading 0-bit
}

bool event::is_field() const {
    return (is_field_);
}

void event::set_field(bool _is_field) {
    is_field_ = _is_field;
}

bool event::is_reliable() const {
    return message_->is_reliable();
}

void event::set_reliable(bool _is_reliable) {
    message_->set_reliable(_is_reliable);
}

const std::shared_ptr<payload> event::get_payload() const {
    return (message_->get_payload());
}

void event::set_payload(std::shared_ptr<payload> _payload) {
	if (set_payload_helper(_payload)) {
		std::shared_ptr<payload> its_new_payload
			= runtime::get()->create_payload(
				_payload->get_data(), _payload->get_length());

		message_->set_payload(its_new_payload);
		if (is_updating_on_change_) {
			notify();
		}
	}
}

void event::set_payload(std::shared_ptr<payload> _payload, client_t _client) {
    set_payload_helper(_payload);
	std::shared_ptr<payload> its_new_payload
		= runtime::get()->create_payload(
			_payload->get_data(), _payload->get_length());

	message_->set_payload(its_new_payload);
	if (is_updating_on_change_) {
		notify_one(_client);
	}
}

void event::set_payload(std::shared_ptr<payload> _payload,
            const std::shared_ptr<endpoint_definition> _target) {

    set_payload_helper(_payload);
	std::shared_ptr<payload> its_new_payload
		= runtime::get()->create_payload(
			_payload->get_data(), _payload->get_length());

	message_->set_payload(its_new_payload);
	if (is_updating_on_change_) {
		notify_one(_target);
	}
}

void event::unset_payload() {
    is_set_ = false;
	message_->set_payload(std::make_shared<payload_impl>());
}

void event::set_update_on_change(bool _is_active) {
    is_updating_on_change_ = _is_active;
}

void event::set_update_cycle(std::chrono::milliseconds &_cycle) {
    cycle_ = _cycle;

    cycle_timer_.cancel();

    if (std::chrono::milliseconds::zero() != _cycle) {
        cycle_timer_.expires_from_now(cycle_);
        std::function<void(boost::system::error_code const &)> its_handler =
                std::bind(&event::update_cbk, shared_from_this(),
                        std::placeholders::_1);
        cycle_timer_.async_wait(its_handler);
    }
}

const std::set<eventgroup_t> & event::get_eventgroups() const {
    return (eventgroups_);
}

void event::add_eventgroup(eventgroup_t _eventgroup) {
    eventgroups_.insert(_eventgroup);
}

void event::set_eventgroups(const std::set<eventgroup_t> &_eventgroups) {
    eventgroups_ = _eventgroups;
}

void event::update_cbk(boost::system::error_code const &_error) {
    if (!_error) {
        cycle_timer_.expires_from_now(cycle_);
        notify();
        std::function<void(boost::system::error_code const &)> its_handler =
                std::bind(&event::update_cbk, shared_from_this(),
                        std::placeholders::_1);
        cycle_timer_.async_wait(its_handler);
    }
}

void event::notify() {
    if (is_set_) {
        routing_->send(VSOMEIP_ROUTING_CLIENT, message_, true);
    }
}

void event::notify_one(const std::shared_ptr<endpoint_definition> &_target) {
    if (is_set_)
        routing_->send_to(_target, message_);
}

void event::notify_one(client_t _client) {
    if (is_set_)
        routing_->send(_client, message_, true);
}

bool event::set_payload_helper(std::shared_ptr<payload> _payload) {
    std::shared_ptr<payload> its_payload = message_->get_payload();
    bool is_change = (its_payload->get_length() != _payload->get_length());
    if (!is_change) {
        std::size_t its_pos = 0;
        const byte_t *its_old_data = its_payload->get_data();
        const byte_t *its_new_data = _payload->get_data();
        while (!is_change && its_pos < its_payload->get_length()) {
            is_change = (*its_old_data++ != *its_new_data++);
            its_pos++;
        }
    }
    is_set_ = true;

    return is_change;
}

}  // namespace vsomeip
