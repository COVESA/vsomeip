// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include <vsomeip/defines.hpp>
#include <vsomeip/logger.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/event.hpp"
#include "../include/routing_manager.hpp"
#include "../../configuration/include/internal.hpp"

namespace vsomeip {

event::event(routing_manager *_routing)
    : routing_(_routing),
      message_(runtime::get()->create_notification()),
      cycle_timer_(_routing->get_io()),
      is_updating_on_change_(true) {
}

service_t event::get_service() const {
  return message_->get_service();
}

void event::set_service(service_t _service) {
  message_->set_service(_service);
}

instance_t event::get_instance() const {
  return message_->get_instance();
}

void event::set_instance(instance_t _instance) {
  message_->set_instance(_instance);
}

event_t event::get_event() const {
  return message_->get_method();
}

void event::set_event(event_t _event) {
  message_->set_method(_event);  // TODO: maybe we should check for the leading 0-bit
}

bool event::is_field() const {
  return is_field_;
}

void event::set_field(bool _is_field) {
  is_field_ = _is_field;
}

std::shared_ptr<payload> event::get_payload() const {
  return message_->get_payload();
}

void event::set_payload(std::shared_ptr<payload> _payload) {
  bool is_change = _payload != message_->get_payload();
  if (true) {
    message_->set_payload(_payload);
    if (is_updating_on_change_) {
      notify();
    }
  }
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
  routing_->send(VSOMEIP_ROUTING_CLIENT, message_, true, true);
}

}  // namespace vsomeip
