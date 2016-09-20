// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/defines.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/event.hpp"
#include "../include/routing_manager.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../logging/include/logger.hpp"
#include "../../message/include/payload_impl.hpp"

namespace vsomeip {

event::event(routing_manager *_routing, bool _is_shadow) :
        routing_(_routing),
        message_(runtime::get()->create_notification()),
        is_field_(false),
        cycle_timer_(_routing->get_io()),
        is_updating_on_change_(true),
        is_set_(false),
        is_provided_(false),
        is_shadow_(_is_shadow),
        is_cache_placeholder_(false) {
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

major_version_t event::get_version() const {
    return message_->get_interface_version();
}

void event::set_version(major_version_t _major) {
    message_->set_interface_version(_major);
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

bool event::is_provided() const {
    return (is_provided_);
}

void event::set_provided(bool _is_provided) {
    is_provided_ = _is_provided;
}

bool event::is_set() const {
    return is_set_;
}

const std::shared_ptr<payload> event::get_payload() const {
    return (message_->get_payload());
}

void event::set_payload_dont_notify(std::shared_ptr<payload> _payload) {
    if(is_cache_placeholder_) {
        std::shared_ptr<payload> its_new_payload
            = runtime::get()->create_payload(
                _payload->get_data(), _payload->get_length());
        message_->set_payload(its_new_payload);
        is_set_ = true;
    } else {
        if (set_payload_helper(_payload)) {
            std::shared_ptr<payload> its_new_payload
                = runtime::get()->create_payload(
                    _payload->get_data(), _payload->get_length());
            message_->set_payload(its_new_payload);
        }
    }
}

void event::set_payload(std::shared_ptr<payload> _payload) {
    if (is_provided_) {
        if (set_payload_helper(_payload)) {
            std::shared_ptr<payload> its_new_payload
                = runtime::get()->create_payload(
                    _payload->get_data(), _payload->get_length());

            message_->set_payload(its_new_payload);
            if (is_updating_on_change_) {
                notify();
            }
        }
    } else {
        VSOMEIP_DEBUG << "Can't set payload for event " << std::hex
                << message_->get_method() << " as it isn't provided";
    }
}

void event::set_payload(std::shared_ptr<payload> _payload, client_t _client) {
    if (is_provided_) {
        set_payload_helper(_payload);
        std::shared_ptr<payload> its_new_payload
            = runtime::get()->create_payload(
                _payload->get_data(), _payload->get_length());

        message_->set_payload(its_new_payload);
        if (is_updating_on_change_) {
            notify_one(_client);
        }
    } else {
        VSOMEIP_DEBUG << "Can't set payload for event " << std::hex
                << message_->get_method() << " as it isn't provided";
    }
}

void event::set_payload(std::shared_ptr<payload> _payload,
            const std::shared_ptr<endpoint_definition> _target) {
    if (is_provided_) {
        if (set_payload_helper(_payload)) {
            std::shared_ptr<payload> its_new_payload
                = runtime::get()->create_payload(
                    _payload->get_data(), _payload->get_length());

            message_->set_payload(its_new_payload);
            if (is_updating_on_change_) {
                notify_one(_target);
            }
        }
    } else {
        VSOMEIP_DEBUG << "Can't set payload for event " << std::hex
                << message_->get_method() << " as it isn't provided";
    }
}

void event::unset_payload(bool _force) {
    if(_force) {
        is_set_ = false;
        message_->set_payload(std::make_shared<payload_impl>());
    } else {
        if (is_provided_) {
            is_set_ = false;
            message_->set_payload(std::make_shared<payload_impl>());
        }
    }
}

void event::set_update_on_change(bool _is_active) {
    if (is_provided_) {
        is_updating_on_change_ = _is_active;
    }
}

void event::set_update_cycle(std::chrono::milliseconds &_cycle) {
    if (is_provided_) {
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
    } else {
        VSOMEIP_DEBUG << "Notify event " << std::hex << message_->get_method()
                << "failed. Event payload not set!";
    }
}

void event::notify_one(const std::shared_ptr<endpoint_definition> &_target) {
    if (is_set_) {
        routing_->send_to(_target, message_);
    } else {
        VSOMEIP_DEBUG << "Notify one event " << std::hex << message_->get_method()
                << "failed. Event payload not set!";
    }
}

void event::notify_one(client_t _client, bool _is_initial) {
    if (is_set_) {
        const bool old_initial_value(message_->is_initial());
        if(_is_initial) {
            message_->set_initial(true);
        }
        routing_->send(_client, message_, true);
        if(_is_initial) {
            message_->set_initial(old_initial_value);
        }
    } else {
        VSOMEIP_DEBUG << "Notify one event " << std::hex << message_->get_method()
                << " to client " << _client << " failed. Event payload not set!";
    }
}

bool event::set_payload_helper(std::shared_ptr<payload> _payload) {
    std::shared_ptr<payload> its_payload = message_->get_payload();
    bool is_change(!is_field_);
    if (is_field_) {
        is_change = (its_payload->get_length() != _payload->get_length());
        if (!is_change) {
            std::size_t its_pos = 0;
            const byte_t *its_old_data = its_payload->get_data();
            const byte_t *its_new_data = _payload->get_data();
            while (!is_change && its_pos < its_payload->get_length()) {
                is_change = (*its_old_data++ != *its_new_data++);
                its_pos++;
            }
        }
    }
    is_set_ = true;

    return is_change;
}

void event::add_ref(client_t _client, bool _is_provided) {
    auto its_client = refs_.find(_client);
    if (its_client == refs_.end()) {
        refs_[_client][_is_provided] = 1;
    } else {
        auto its_provided = its_client->second.find(_is_provided);
        if (its_provided == its_client->second.end()) {
            refs_[_client][_is_provided] = 1;
        } else {
            its_provided->second++;
        }
    }
}

void event::remove_ref(client_t _client, bool _is_provided) {
    auto its_client = refs_.find(_client);
    if (its_client != refs_.end()) {
        auto its_provided = its_client->second.find(_is_provided);
        if (its_provided != its_client->second.end()) {
            its_provided->second--;
            if (0 == its_provided->second) {
                its_client->second.erase(_is_provided);
                if (0 == its_client->second.size()) {
                    refs_.erase(_client);
                }
            }
        }
    }
}

bool event::has_ref() {
    return refs_.size() != 0;
}

bool event::is_shadow() const {
    return is_shadow_;
}

void event::set_shadow(bool _shadow) {
    is_shadow_ = _shadow;
}

bool event::is_cache_placeholder() const {
    return is_cache_placeholder_;
}

void event::set_cache_placeholder(bool _is_cache_place_holder) {
    is_cache_placeholder_ = _is_cache_place_holder;
}

}  // namespace vsomeip
