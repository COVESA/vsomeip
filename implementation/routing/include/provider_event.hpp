// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../endpoints/include/timer.hpp"

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/function_types.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/message.hpp>

#include <memory>
#include <map>
#include <set>

namespace vsomeip_v3 {

struct debounce_filter_impl_t;

/**
 * @class provider_event
 * @brief Provider-side (proxy) representation of a single offered event or field.
 *
 * Two properties set this apart from the `event` and define its role:
 *
 * 1. Pull model. It never pushes: a provider_event holds the state of one offered
 *    event (subscribers, the last field value, gating/debounce config) and, when
 *    asked, computes *what* to notify and *to whom* — returning a ready-to-send
 *    message plus the target client set. It primarily acts as storage; the
 *    owner (its caller) pulls that result and performs the actual send.
 *
 * 2. No synchronization primitives. This class holds no mutex of its own. Because
 *    it only stores state and returns decisions (never acting on them), the owner
 *    can own the locking and synchronize across *many* events under a single,
 *    simpler locking strategy instead of every event guarding itself.
 *
 * Callback guarantee: like local_endpoint, a public member function never
 * synchronously calls back into any other component. It only reads/updates its own
 * state and returns; the caller decides what to do with the result. This is what
 * lets the owner call it while holding its lock without risking re-entrancy.
 *
 * Stored state / responsibilities:
 * - Subscriber book-keeping: which clients are subscribed, grouped per eventgroup.
 * - Notification gating: field-value dedup and the update_on_change flag decide
 *   whether a change is published at all.
 * - Debounce: optional per-subscriber epsilon/interval filtering of the target set.
 * - Field value: caches the last payload (current_) so late subscribers and cyclic
 *   pokes can be served the current value.
 * - Cyclic notification: OWNS the periodic timer; on each tick it invokes the
 *   captured poke, which asks the owner to re-notify. Reschedule decisions stay here.
 *
 * Placeholder vs. real:
 * - A placeholder accumulates subscribers for arbitrary groups
 *   *before* the real event is offered; the real event and the
 *   caller transfers the cached subscribers and their filters (adopt_subscribers()).
 * - Re-offering an already-registered (non-placeholder) event is rejected by the
 *   owner (error, keep existing); provider_event itself has no replace path.
 */
class provider_event {
public:
    /// Gating/debounce behaviour for an offered event.
    struct options {
        bool update_on_change_; ///< If false, value changes are not auto-published.
        bool update_resets_cycle_; ///< If true, a published change restarts the cyclic timer.
        epsilon_change_func_t epsilon_change_func_; ///< Event-wide value-change predicate (may be null).
    };
    /// Immutable identity of an offered event: type, address triplet and its eventgroups.
    struct definition {
        event_type_e type_;
        service_t service_;
        instance_t instance_;
        event_t event_;
        std::set<eventgroup_t> groups_;
    };

    /**
     * @brief Creates a real (offered) event.
     * @param _cycle Cyclic notification interval; zero disables the timer.
     * @param _periodic_poke Invoked on each timer tick (fire-and-forget) to trigger re-notify.
     */
    provider_event(definition const& _definition, options const& _options, std::chrono::milliseconds _cycle, boost::asio::io_context& _io,
                   std::function<void()> _periodic_poke);
    /**
     * @brief Creates a placeholder that only accumulates subscribers until the real event is offered.
     */
    provider_event();

    [[nodiscard]] bool is_placeholder() const;
    [[nodiscard]] event_t get_event() const;
    [[nodiscard]] bool is_subscribed(client_t _client) const;
    [[nodiscard]] bool is_part_of(eventgroup_t _group) const;
    std::set<eventgroup_t> const& groups() const;
    std::set<client_t> const& get_subscriber(eventgroup_t _group) const;
    /// @brief Last cached field message (nullptr for a plain event or a never-set field).
    std::shared_ptr<message> get_current() const;

    void set_version(major_version_t _major);

    /**
     * @brief Detaches all subscribers, returning them grouped per eventgroup.
     * @note Re-seeds the definition's structural groups so the event can be re-subscribed.
     *       Used to transfer subscribers from a placeholder to the real event.
     */
    std::map<eventgroup_t, std::set<client_t>> clear_subscriber();
    std::set<eventgroup_t> unsubscribe(client_t _client);

    /**
     * @brief Adopts all subscribers (and their per-client debounce filters) from a placeholder.
     * @note Used when the real event is offered: subscribers accumulated on the placeholder are
     *       moved onto this event's valid groups, preserving each client's compiled filter.
     *       Subscribers for groups this event does not define are dropped (with a warning).
     */
    void adopt_subscribers(provider_event& _placeholder);

    /**
     * @brief Registers a subscriber for an eventgroup, with an optional debounce filter.
     */
    void add_subscriber(eventgroup_t _group, client_t _client, const std::shared_ptr<debounce_filter_impl_t>& _filter);
    void remove_subscriber(eventgroup_t _group, client_t _client);

    void reset();

    /**
     * @brief Applies a new payload and computes the notify-all result.
     * @return {message, targets}: the message to send and the (debounce-filtered) subscriber set.
     *         An empty target set means the change was suppressed (dedup / !update_on_change).
     */
    std::pair<std::shared_ptr<message>, std::set<client_t>> update(std::shared_ptr<payload> const& _payload);
    /**
     * @brief Produces the periodic re-notification of the current value (timer tick path).
     * @return {message, targets} for the cyclic subscriber set; empty if nothing to send.
     */
    std::pair<std::shared_ptr<message>, std::set<client_t>> cyclic();
    /**
     * @brief notify_one variant: gates and caches a payload without a subscriber set.
     * @param _force Bypasses the field-value dedup only (not the update_on_change gate).
     * @return The message to send, or nullptr if the change was suppressed.
     */
    std::shared_ptr<message> update_without_subscriber(std::shared_ptr<payload> const& _payload, bool _force = false);

private:
    void create_event();
    bool has_changed(std::shared_ptr<payload> const& _lhs, std::shared_ptr<payload> const& _rhs) const;

private:
    bool is_placeholder_{false};
    bool is_set_{false};
    major_version_t major_{ANY_MAJOR};

    std::shared_ptr<message> current_;

    definition const definition_{};
    options const options_{};

    // Cyclic-timer plumbing. provider_event OWNS scheduling: it starts/stops timer_, which owns
    // both the cycle interval and the task. On each tick the task invokes the captured periodic_poke
    // (fire-and-forget: it has the owner perform the re-notification) and returns true to reschedule.
    // The reschedule decision stays inside provider_event (via start()/stop()), not the owner.
    // Null when the cycle is zero (no cyclic notification) or on placeholders.
    std::shared_ptr<timer> timer_;
    std::map<eventgroup_t, std::set<client_t>> subscriber_;

    std::map<client_t, epsilon_change_func_t> filters_;
};

} // namespace vsomeip_v3
