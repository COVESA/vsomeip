// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>
#include <sstream>

#include <vsomeip/constants.hpp>
#include <vsomeip/defines.hpp>
#include <vsomeip/message.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/event.hpp"
#include "../include/routing_manager.hpp"
#include "../../message/include/payload_impl.hpp"

#include "../../endpoints/include/endpoint_definition.hpp"

namespace vsomeip_v3 {

event::event(routing_manager *_routing, bool _is_shadow) :
        routing_(_routing),
        message_(runtime::get()->create_notification()),
        type_(event_type_e::ET_EVENT),
        cycle_timer_(_routing->get_io()),
        cycle_(std::chrono::milliseconds::zero()),
        change_resets_cycle_(false),
        is_updating_on_change_(true),
        is_set_(false),
        is_provided_(false),
        is_shadow_(_is_shadow),
        is_cache_placeholder_(false),
        epsilon_change_func_(std::bind(&event::compare, this,
                std::placeholders::_1, std::placeholders::_2)),
        reliability_(reliability_type_e::RT_UNKNOWN) {
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
    message_->set_method(_event);
}

event_type_e event::get_type() const {
    return (type_);
}

void event::set_type(const event_type_e _type) {
    type_ = _type;
}

bool event::is_field() const {
    return (type_ == event_type_e::ET_FIELD);
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
    std::lock_guard<std::mutex> its_lock(mutex_);
    return (message_->get_payload());
}

bool event::set_payload_dont_notify(const std::shared_ptr<payload> &_payload) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (is_cache_placeholder_) {
        reset_payload(_payload);
        is_set_ = true;
    } else {
        if (set_payload_helper(_payload, false)) {
            reset_payload(_payload);
        } else {
            return false;
        }
    }
    return true;
}

void event::set_payload(const std::shared_ptr<payload> &_payload, bool _force) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (is_provided_) {
        if (set_payload_helper(_payload, _force)) {
            reset_payload(_payload);
            if (is_updating_on_change_) {
                if (change_resets_cycle_)
                    stop_cycle();

                notify();

                if (change_resets_cycle_)
                    start_cycle();
            }
        }
    } else {
        VSOMEIP_INFO << "Can't set payload for event " << std::hex
                << message_->get_method() << " as it isn't provided";
    }
}

void event::set_payload(const std::shared_ptr<payload> &_payload, client_t _client,
            bool _force) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (is_provided_) {
        if (set_payload_helper(_payload, _force)) {
            reset_payload(_payload);
            if (is_updating_on_change_) {
                notify_one_unlocked(_client);
            }
        }
    } else {
        VSOMEIP_INFO << "Can't set payload for event " << std::hex
                << message_->get_method() << " as it isn't provided";
    }
}

void event::set_payload(const std::shared_ptr<payload> &_payload,
        const client_t _client,
        const std::shared_ptr<endpoint_definition>& _target,
        bool _force) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (is_provided_) {
        if (set_payload_helper(_payload, _force)) {
            reset_payload(_payload);
            if (is_updating_on_change_) {
                notify_one_unlocked(_client, _target);
            }
        }
    } else {
        VSOMEIP_INFO << "Can't set payload for event " << std::hex
                << message_->get_method() << " as it isn't provided";
    }
}

bool event::set_payload_notify_pending(const std::shared_ptr<payload> &_payload) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (!is_set_ && is_provided_) {
        reset_payload(_payload);

        // Send pending initial events.
        for (const auto &its_target : pending_) {
            message_->set_session(routing_->get_session());
            routing_->send_to(VSOMEIP_ROUTING_CLIENT,
                    its_target, message_);
        }
        pending_.clear();

        return true;
    }

    return false;
}

void event::unset_payload(bool _force) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    if (_force) {
        is_set_ = false;
        stop_cycle();
        message_->set_payload(std::make_shared<payload_impl>());
    } else {
        if (is_provided_) {
            is_set_ = false;
            stop_cycle();
            message_->set_payload(std::make_shared<payload_impl>());
        }
    }
}

void event::set_update_cycle(std::chrono::milliseconds &_cycle) {
    if (is_provided_) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        stop_cycle();
        cycle_ = _cycle;
        start_cycle();
    }
}

void event::set_change_resets_cycle(bool _change_resets_cycle) {
    change_resets_cycle_ = _change_resets_cycle;
}

void event::set_update_on_change(bool _is_active) {
    if (is_provided_) {
        is_updating_on_change_ = _is_active;
    }
}

void event::set_epsilon_change_function(const epsilon_change_func_t &_epsilon_change_func) {
    if (_epsilon_change_func) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        epsilon_change_func_ = _epsilon_change_func;
    }
}

const std::set<eventgroup_t> event::get_eventgroups() const {
    std::set<eventgroup_t> its_eventgroups;
    {
        std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
        for (const auto& e : eventgroups_) {
            its_eventgroups.insert(e.first);
        }
    }
    return its_eventgroups;
}

std::set<eventgroup_t> event::get_eventgroups(client_t _client) const {
    std::set<eventgroup_t> its_eventgroups;

    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (auto e : eventgroups_) {
        if (e.second.find(_client) != e.second.end())
            its_eventgroups.insert(e.first);
    }
    return its_eventgroups;
}

void event::add_eventgroup(eventgroup_t _eventgroup) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    if (eventgroups_.find(_eventgroup) == eventgroups_.end())
        eventgroups_[_eventgroup] = std::set<client_t>();
}

void event::set_eventgroups(const std::set<eventgroup_t> &_eventgroups) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (auto e : _eventgroups)
        eventgroups_[e] = std::set<client_t>();
}

void event::update_cbk(boost::system::error_code const &_error) {
    if (!_error) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        cycle_timer_.expires_from_now(cycle_);
        notify();
        auto its_handler =
                std::bind(&event::update_cbk, shared_from_this(),
                        std::placeholders::_1);
        cycle_timer_.async_wait(its_handler);
    }
}

void event::notify() {
    if (is_set_) {
        message_->set_session(routing_->get_session());
        routing_->send(VSOMEIP_ROUTING_CLIENT, message_);
    } else {
        VSOMEIP_INFO << __func__
                << ": Notifying " << std::hex << get_event()
                << " failed. Event payload not (yet) set!";
    }
}

void event::notify_one(client_t _client,
        const std::shared_ptr<endpoint_definition> &_target) {
    if (_target) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        notify_one_unlocked(_client, _target);
    } else {
        VSOMEIP_WARNING << __func__
                << ": Notifying " << std::hex << get_event()
                << " failed. Target undefined";
    }
}

void event::notify_one_unlocked(client_t _client,
        const std::shared_ptr<endpoint_definition> &_target) {
    if (_target) {
        if (is_set_) {
            message_->set_session(routing_->get_session());
            routing_->send_to(_client, _target, message_);
        } else {
            VSOMEIP_INFO << __func__
                    << ": Notifying " << std::hex << get_event()
                    << " failed. Event payload not (yet) set!";
            pending_.insert(_target);
        }
    } else {
        VSOMEIP_WARNING << __func__
                << ": Notifying " << std::hex << get_event()
                << " failed. Target undefined";
    }
}

void event::notify_one(client_t _client) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    notify_one_unlocked(_client);
}

void event::notify_one_unlocked(client_t _client) {
    if (is_set_) {
        message_->set_session(routing_->get_session());
        routing_->send(_client, message_);
    } else {
        VSOMEIP_INFO << __func__
                << ": Notifying "
                << std::hex << message_->get_method()
                << " to client " << _client
                << " failed. Event payload not set!";
    }
}

bool event::set_payload_helper(const std::shared_ptr<payload> &_payload, bool _force) {
    std::shared_ptr<payload> its_payload = message_->get_payload();
    bool is_change(type_ != event_type_e::ET_FIELD);
    if (!is_change) {
        is_change = _force || epsilon_change_func_(its_payload, _payload);
    }
    return is_change;
}

void event::reset_payload(const std::shared_ptr<payload> &_payload) {
    std::shared_ptr<payload> its_new_payload
        = runtime::get()->create_payload(
              _payload->get_data(), _payload->get_length());
    message_->set_payload(its_new_payload);

    if (!is_set_)
        start_cycle();

    is_set_ = true;
}

void event::add_ref(client_t _client, bool _is_provided) {
    std::lock_guard<std::mutex> its_lock(refs_mutex_);
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
    std::lock_guard<std::mutex> its_lock(refs_mutex_);
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
    std::lock_guard<std::mutex> its_lock(refs_mutex_);
    return refs_.size() != 0;
}

bool event::add_subscriber(eventgroup_t _eventgroup, client_t _client, bool _force) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    bool ret = false;
    if (_force // remote events managed by rm_impl
            || is_provided_ // events provided by rm_proxies
            || is_shadow_ // local events managed by rm_impl
            || is_cache_placeholder_) {
        ret = eventgroups_[_eventgroup].insert(_client).second;
    } else {
        VSOMEIP_WARNING << __func__ << ": Didnt' insert client "
                << std::hex << std::setw(4) << std::setfill('0') << _client
                << " to eventgroup 0x"
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup;
    }
    return ret;
}

void event::remove_subscriber(eventgroup_t _eventgroup, client_t _client) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    auto find_eventgroup = eventgroups_.find(_eventgroup);
    if (find_eventgroup != eventgroups_.end())
        find_eventgroup->second.erase(_client);
}

bool event::has_subscriber(eventgroup_t _eventgroup, client_t _client) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    auto find_eventgroup = eventgroups_.find(_eventgroup);
    if (find_eventgroup != eventgroups_.end()) {
        if (_client == ANY_CLIENT) {
            return (find_eventgroup->second.size() > 0);
        } else {
            return (find_eventgroup->second.find(_client)
                    != find_eventgroup->second.end());
        }
    }
    return false;
}

std::set<client_t> event::get_subscribers() {
    std::set<client_t> its_subscribers;
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (const auto &e : eventgroups_)
        its_subscribers.insert(e.second.begin(), e.second.end());
    return its_subscribers;
}

void event::clear_subscribers() {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (auto &e : eventgroups_)
        e.second.clear();
}

bool event::has_ref(client_t _client, bool _is_provided) {
    std::lock_guard<std::mutex> its_lock(refs_mutex_);
    auto its_client = refs_.find(_client);
    if (its_client != refs_.end()) {
        auto its_provided = its_client->second.find(_is_provided);
        if (its_provided != its_client->second.end()) {
            if(its_provided->second > 0) {
                return true;
            }
        }
    }
    return false;
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

void event::start_cycle() {
    if (std::chrono::milliseconds::zero() != cycle_) {
        cycle_timer_.expires_from_now(cycle_);
        auto its_handler =
                std::bind(&event::update_cbk, shared_from_this(),
                        std::placeholders::_1);
        cycle_timer_.async_wait(its_handler);
    }
}

void event::stop_cycle() {
    if (std::chrono::milliseconds::zero() != cycle_) {
        boost::system::error_code ec;
        cycle_timer_.cancel(ec);
    }
}

bool event::compare(const std::shared_ptr<payload> &_lhs,
        const std::shared_ptr<payload> &_rhs) const {
    bool is_change = (_lhs->get_length() != _rhs->get_length());
    if (!is_change) {
        std::size_t its_pos = 0;
        const byte_t *its_old_data = _lhs->get_data();
        const byte_t *its_new_data = _rhs->get_data();
        while (!is_change && its_pos < _lhs->get_length()) {
            is_change = (*its_old_data++ != *its_new_data++);
            its_pos++;
        }
    }
    return is_change;
}

std::set<client_t> event::get_subscribers(eventgroup_t _eventgroup) {
    std::set<client_t> its_subscribers;
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    auto found_eventgroup = eventgroups_.find(_eventgroup);
    if (found_eventgroup != eventgroups_.end()) {
        its_subscribers = found_eventgroup->second;
    }
    return its_subscribers;
}

bool event::is_subscribed(client_t _client) {
    std::lock_guard<std::mutex> its_lock(eventgroups_mutex_);
    for (const auto &egp : eventgroups_) {
        if (egp.second.find(_client) != egp.second.end()) {
            return true;
        }
    }
    return false;
}

reliability_type_e
event::get_reliability() const {
    return reliability_;
}

void
event::set_reliability(const reliability_type_e _reliability) {
    reliability_ = _reliability;
}

void
event::remove_pending(const std::shared_ptr<endpoint_definition> &_target) {
    std::lock_guard<std::mutex> its_lock(mutex_);
    pending_.erase(_target);
}

} // namespace vsomeip_v3
