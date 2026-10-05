// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/provider_event.hpp"

#include "logger_ext.hpp"
#include "../include/debounce_func.hpp"
#include "../../utility/include/utility.hpp"
#include "../../configuration/include/debounce_filter_impl.hpp"

#include <vsomeip/runtime.hpp>

#include <sstream>

#define VSOMEIP_LOG_PREFIX "provider_event"

namespace vsomeip_v3 {
provider_event::provider_event(definition const& _definition, options const& _options, std::chrono::milliseconds _cycle,
                               boost::asio::io_context& _io, std::function<void()> _periodic_poke) :
    definition_(_definition), options_(_options) {
    if (_cycle != std::chrono::milliseconds::zero()) {
        timer_ = timer::create(_io, _cycle, [_periodic_poke = std::move(_periodic_poke)]() {
            _periodic_poke();
            return true;
        });
    }
    for (auto group : definition_.groups_) {
        subscriber_[group];
    }
}

provider_event::provider_event() : is_placeholder_(true) { }

void provider_event::set_version(major_version_t _major) {
    major_ = _major;
}

bool provider_event::is_placeholder() const {
    return is_placeholder_;
}

bool provider_event::is_subscribed(client_t _client) const {
    for (const auto& [_, subscriber] : subscriber_) {
        if (subscriber.count(_client) > 0) {
            return true;
        }
    }
    return false;
}

bool provider_event::is_part_of(eventgroup_t _group) const {
    return subscriber_.count(_group) > 0;
}

event_t provider_event::get_event() const {
    return definition_.event_;
}

std::set<eventgroup_t> const& provider_event::groups() const {
    return definition_.groups_;
}

std::map<eventgroup_t, std::set<client_t>> provider_event::clear_subscriber() {
    auto tmp = std::move(subscriber_);
    // Preserve the structural groups from the definition (empty), so the event can accept subscribers
    // again after e.g. a host reconnect. A placeholder has no definition groups, so it ends up empty.
    subscriber_.clear();
    for (auto group : definition_.groups_) {
        subscriber_[group];
    }
    filters_.clear();
    return tmp;
}

std::set<eventgroup_t> provider_event::unsubscribe(client_t _client) {
    std::set<eventgroup_t> groups;
    for (auto& [group, subscriptions] : subscriber_) {
        if (subscriptions.count(_client) > 0) {
            groups.insert(group);
            subscriptions.erase(_client);
        }
    }
    return groups;
}

void provider_event::add_subscriber(eventgroup_t _group, client_t _client, const std::shared_ptr<debounce_filter_impl_t>& _filter) {
    if (auto it = subscriber_.find(_group); it != subscriber_.end()) {
        it->second.insert(_client);
    } else if (is_placeholder_) {
        subscriber_[_group].insert(_client);
    } else {
        VSOMEIP_ERROR_P << "client 0x" << hex4(_client) << " tried to subscribe for unknown eventgroup: " << hex4(_group) << " for event: ["
                        << hex4(definition_.service_) << "." << hex4(definition_.instance_) << "." << static_cast<int>(major_) << ":"
                        << hex4(definition_.event_) << "]";
        return;
    }

    // A per-client debounce filter overrides the event-wide default epsilon for that client;
    // a null filter clears any previously installed one (falls back to the default epsilon).
    if (_filter) {
        VSOMEIP_WARNING_P << "using client [" << hex4(_client) << "] specific filter configuration for event: ["
                          << hex4(definition_.service_) << "." << hex4(definition_.instance_) << ":" << hex4(definition_.event_) << "]";
        std::stringstream filter_parameters;
        filter_parameters << "(on_change=" << std::boolalpha << _filter->on_change_ << ", interval=" << _filter->interval_
                          << ", on_change_resets_interval=" << std::boolalpha << _filter->on_change_resets_interval_ << ", ignore=[ ";
        for (auto i : _filter->ignore_) {
            filter_parameters << "(" << i.first << ", " << hex2(i.second) << ") ";
        }
        filter_parameters << "], send_current_value_after_=" << std::boolalpha << _filter->send_current_value_after_ << ")";
        VSOMEIP_INFO_P << "filter parameters: " << filter_parameters.str();

        if (_filter->send_current_value_after_) {
            VSOMEIP_WARNING_P << "filter uses unsupported parameter `send_current_value_after_`";
        }

        filters_[_client] = make_debounce_func(_filter);
    } else {
        filters_.erase(_client);
    }
}

void provider_event::adopt_subscribers(provider_event& _placeholder) {
    // Move the subscribers accumulated on the placeholder onto this event's structural groups.
    for (auto const& [group, clients] : _placeholder.subscriber_) {
        auto it = subscriber_.find(group);
        if (it == subscriber_.end()) {
            VSOMEIP_WARNING_P << "dropping " << clients.size() << " subscriber(s) for unknown eventgroup 0x" << hex4(group)
                              << " while adopting into event: [" << hex4(definition_.service_) << "." << hex4(definition_.instance_) << ":"
                              << hex4(definition_.event_) << "]";
            continue;
        }
        it->second.insert(clients.begin(), clients.end());
    }
    // Preserve each surviving client's compiled debounce filter. The closures are portable
    // (they fetch the clock lazily), so they can be moved across provider_event instances as-is.
    for (auto& [client, filter] : _placeholder.filters_) {
        if (is_subscribed(client)) {
            filters_[client] = std::move(filter);
        }
    }
}

void provider_event::remove_subscriber(eventgroup_t _group, client_t _client) {
    if (auto it = subscriber_.find(_group); it != subscriber_.end()) {
        it->second.erase(_client);
    }
}

std::set<client_t> const& provider_event::get_subscriber(eventgroup_t _group) const {
    static const std::set<client_t> empty_set{};
    if (auto const it = subscriber_.find(_group); it != subscriber_.end()) {
        return it->second;
    }
    return empty_set;
}

void provider_event::reset() {
    clear_subscriber();
    if (timer_) {
        timer_->stop();
    }
    current_ = nullptr;
    is_set_ = false;
}

std::pair<std::shared_ptr<message>, std::set<client_t>> provider_event::update(std::shared_ptr<payload> const& _payload) {
    create_event();

    // Field dedup (parity with event::prepare_update_payload_unlocked): a FIELD event without a cycle
    // that is already set and whose payload did not change is not re-sent (drops a redundant set of the
    // same value). The comparison uses the previously committed payload, captured before the commit below.
    std::shared_ptr<payload> const old_payload = current_->get_payload();
    bool const changed = has_changed(old_payload, _payload);
    bool const field_dedup = definition_.type_ == event_type_e::ET_FIELD && !timer_ && is_set_ && !changed;
    bool const was_set = is_set_;

    // Single-slot, compare-arg-then-commit: always commit the latest payload, even when the
    // notification is gated or deduplicated, so a later cyclic tick emits the latest value.
    current_->set_payload(_payload);
    is_set_ = true;

    // Cyclic-timer management. The cycle starts once a payload exists (independent of the on-change
    // gate, matching event). change_resets_cycle restarts the running cycle on a forwarded update.
    if (timer_) {
        if (!was_set) {
            timer_->start();
        } else if (options_.update_on_change_ && options_.update_resets_cycle_) {
            if (!changed) {
                // Even though we emit a warning, we are still required to send any _updated_ cyclic event (and update the timer)
                VSOMEIP_WARNING_P << "update_resets_cycle restarted the cycle on an unchanged value for event: ["
                                  << hex4(definition_.service_) << "." << hex4(definition_.instance_) << ":" << hex4(definition_.event_)
                                  << "]";
            }
            timer_->start();
        }
    }

    if (!options_.update_on_change_ || field_dedup) {
        return {current_, {}};
    }

    // Build the deduplicated subscriber union first, then evaluate each client exactly once (an
    // interval filter has side effects, so a multi-group client must not be evaluated twice).
    std::set<client_t> candidates;
    for (auto const& [_, clients] : subscriber_) {
        candidates.insert(clients.begin(), clients.end());
    }

    std::set<client_t> subscriber;
    for (client_t c : candidates) {
        bool forward;
        if (auto it = filters_.find(c); it != filters_.end()) {
            forward = it->second(old_payload, _payload);
        } else {
            // No per-client filter: the event-wide epsilon decides; a null (default) epsilon always forwards.
            forward = !options_.epsilon_change_func_ || options_.epsilon_change_func_(old_payload, _payload);
        }
        if (forward) {
            subscriber.insert(c);
        }
    }
    return {current_, std::move(subscriber)};
}

std::pair<std::shared_ptr<message>, std::set<client_t>> provider_event::cyclic() {
    // Cyclic re-notify (driven by the timer poke): resend the current value with no on-change gate and
    // no field dedup. Reads current_ directly (not the field-gated get_current), so non-field cyclic
    // events resend too. Runs while a payload is set, regardless of subscriber count.
    if (!is_set_ || !current_) {
        return {current_, {}};
    }

    // Force semantics (parity with event::notify(true) -> get_filtered_subscribers(true)): the event-wide
    // default epsilon is bypassed (no-filter subscribers always get it), but per-subscriber filters still
    // apply. On a cyclic tick the value is unchanged, so an on_change filter suppresses while an interval
    // filter forwards once elapsed.
    std::shared_ptr<payload> const payload = current_->get_payload();
    std::set<client_t> candidates;
    for (auto const& [_, clients] : subscriber_) {
        candidates.insert(clients.begin(), clients.end());
    }

    std::set<client_t> subscriber;
    for (client_t c : candidates) {
        bool forward;
        if (auto it = filters_.find(c); it != filters_.end()) {
            forward = it->second(payload, payload);
        } else {
            forward = true;
        }
        if (forward) {
            subscriber.insert(c);
        }
    }
    return {current_, std::move(subscriber)};
}

bool provider_event::has_changed(std::shared_ptr<payload> const& _lhs, std::shared_ptr<payload> const& _rhs) const {
    if (_lhs && _rhs) {
        return !((*_lhs) == (*_rhs));
    }
    // Exactly one side set => changed (value <-> nothing); both null => unchanged.
    // Note: deliberately deviates from event::has_changed, which inverts these degenerate cases.
    return static_cast<bool>(_lhs) != static_cast<bool>(_rhs);
}

std::shared_ptr<message> provider_event::update_without_subscriber(std::shared_ptr<payload> const& _payload, bool _force) {
    create_event();

    // notify_one gating parity with event's set_payload(payload, client, target, _force):
    //   - field dedup (has_changed byte-compare) suppresses an unchanged FIELD, bypassed by _force;
    //   - the update_on_change gate suppresses when disabled (NOT bypassed by force);
    //   - NO per-subscriber epsilon / event-wide epsilon (event::notify_one_unlocked sends directly).
    std::shared_ptr<payload> const old_payload = current_->get_payload();
    bool const changed = has_changed(old_payload, _payload);
    bool const field_dedup = !_force && definition_.type_ == event_type_e::ET_FIELD && !timer_ && is_set_ && !changed;
    bool const was_set = is_set_;

    // Single-slot, commit-always (same as update()), even when the send is gated/deduped.
    current_->set_payload(_payload);
    is_set_ = true;

    // First set starts the cycle (parity with the shared prepare_update_payload_unlocked start_cycle).
    // Unlike update(), the notify_one path never restarts a running cycle on later updates: event's
    // change_resets_cycle stop/start bracket lives only on the notify-ALL set_payload overload.
    if (timer_ && !was_set) {
        timer_->start();
    }

    if (!options_.update_on_change_ || field_dedup) {
        return nullptr;
    }
    return current_;
}

void provider_event::create_event() {
    if (!current_) {
        current_ = runtime::get()->create_notification();
        current_->set_service(definition_.service_);
        current_->set_instance(definition_.instance_);
        current_->set_interface_version(major_);
        current_->set_method(definition_.event_);
    }
}

std::shared_ptr<message> provider_event::get_current() const {
    if (definition_.type_ == event_type_e::ET_FIELD) {
        return current_;
    }
    return nullptr;
}
}
