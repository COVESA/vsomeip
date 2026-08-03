// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Unit-test matrix for provider_event.
//
// Reference behaviour is `event` (implementation/routing/src/event.cpp); the intent is
// parity for the provider-side notification features:
//   - update_on_change gate + field dedup (unchanged-value skip)
//   - per-subscriber epsilon/debounce filtering
//   - cyclic re-notify timer (+ update_resets_cycle)
//   - notify_one gating parity
// plus the re-offer (placeholder -> real) subscriber transfer.

#include <gtest/gtest.h>

#include "../../../implementation/routing/include/provider_event.hpp"
#include "../../../implementation/configuration/include/debounce_filter_impl.hpp"
#include "../../../implementation/endpoints/include/abstract_socket_factory.hpp"
#include "../../../implementation/endpoints/include/abstract_timer.hpp"
#include "../endpoint_tests/delegating_socket_factory.hpp"

#include "common/fake_clock.hpp"

#include <vsomeip/runtime.hpp>

#include <boost/asio/io_context.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace vsomeip_v3::testing {

using namespace std::chrono_literals;

// Controllable fake timer: records start/cancel and the pending handler so a test can drive ticks
// deterministically (used from the cyclic tests; a zero cycle never constructs one).
struct fake_timer_state {
    uint32_t start_count_{0};
    uint32_t cancel_count_{0};
    std::optional<std::chrono::milliseconds> interval_;
    abstract_timer::handler_t handler_;
};

class fake_timer final : public abstract_timer {
public:
    explicit fake_timer(std::shared_ptr<fake_timer_state> _state) : state_(std::move(_state)) { }
    void cancel() override { ++state_->cancel_count_; }
    void expires_after(std::chrono::milliseconds _timeout) override { state_->interval_ = _timeout; }
    void async_wait(handler_t _handler) override {
        state_->handler_ = std::move(_handler);
        ++state_->start_count_;
    }

private:
    std::shared_ptr<fake_timer_state> state_;
};

// Per-test factory: hands out the fixture's fake_clock and controllable fake timers.
class test_factory final : public abstract_socket_factory {
public:
    std::shared_ptr<abstract_netlink_connector> create_netlink_connector(boost::asio::io_context&, const boost::asio::ip::address&,
                                                                         const boost::asio::ip::address&, bool) override {
        return nullptr;
    }
    std::unique_ptr<tcp_socket> create_tcp_socket(boost::asio::io_context&) override { return nullptr; }
    std::unique_ptr<tcp_acceptor> create_tcp_acceptor(boost::asio::io_context&) override { return nullptr; }
    std::unique_ptr<udp_socket> create_udp_socket(boost::asio::io_context&) override { return nullptr; }
#if defined(__linux__) || defined(__QNX__)
    std::unique_ptr<uds_socket> create_uds_socket(boost::asio::io_context&) override { return nullptr; }
    std::unique_ptr<uds_acceptor> create_uds_acceptor(boost::asio::io_context&) override { return nullptr; }
#endif
    std::unique_ptr<abstract_timer> create_timer(boost::asio::io_context&) override {
        auto state = std::make_shared<fake_timer_state>();
        last_timer_state_ = state;
        return std::make_unique<fake_timer>(state);
    }
    std::shared_ptr<abstract_clock> get_clock() override { return clock_; }

    std::shared_ptr<abstract_clock> clock_{};
    std::shared_ptr<fake_timer_state> last_timer_state_;
};

// Common base fixture. Installs a delegating factory once (get() caches the first factory globally),
// then swaps in a fresh per-test factory bound to this test's fake_clock. Grows as slices land.
struct provider_event_test : ::testing::Test {
    static std::shared_ptr<delegating_socket_factory> delegate_;
    static void SetUpTestSuite() { set_abstract_factory(delegate_); }

    void SetUp() override {
        factory_ = std::make_shared<test_factory>();
        factory_->clock_ = clock_;
        delegate_->impl_ = factory_;
    }

    std::shared_ptr<payload> make_payload(std::vector<byte_t> _data) const { return runtime::get()->create_payload(std::move(_data)); }

    static std::shared_ptr<debounce_filter_impl_t> make_filter(bool _on_change, int64_t _interval_ms = -1,
                                                               bool _on_change_resets_interval = false,
                                                               std::map<size_t, byte_t> _ignore = {}) {
        auto filter = std::make_shared<debounce_filter_impl_t>();
        filter->on_change_ = _on_change;
        filter->interval_ = _interval_ms;
        filter->on_change_resets_interval_ = _on_change_resets_interval;
        filter->ignore_ = std::move(_ignore);
        return filter;
    }

    std::shared_ptr<provider_event> make_event(event_type_e _type, std::set<eventgroup_t> _groups, std::chrono::milliseconds _cycle = 0ms,
                                               bool _update_on_change = true, bool _update_resets_cycle = false,
                                               epsilon_change_func_t _epsilon = nullptr) {
        provider_event::definition def{_type, service_, instance_, event_, std::move(_groups)};
        provider_event::options opt{_update_on_change, _update_resets_cycle, std::move(_epsilon)};
        return std::make_shared<provider_event>(def, opt, _cycle, io_, [this]() { ++poke_count_; });
    }

    std::shared_ptr<fake_clock> clock_{std::make_shared<fake_clock>()};
    boost::asio::io_context io_;
    std::shared_ptr<test_factory> factory_;
    unsigned poke_count_{0};

    static constexpr service_t service_{0x1111};
    static constexpr instance_t instance_{0x2222};
    static constexpr event_t event_{0x3333};
    static constexpr eventgroup_t group1_{0x4444};
    static constexpr eventgroup_t group2_{0x5555};
    static constexpr client_t client1_{0x0001};
    static constexpr client_t client2_{0x0002};
};

std::shared_ptr<delegating_socket_factory> provider_event_test::delegate_ = std::make_shared<delegating_socket_factory>();

// ---------------------------------------------------------------------------
// Construction & basics
// ---------------------------------------------------------------------------
struct provider_event_basics : provider_event_test { };

TEST_F(provider_event_basics, custom_ctor_non_placeholder) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    EXPECT_FALSE(ev->is_placeholder());
}

TEST_F(provider_event_basics, default_ctor_yields_placeholder) {
    EXPECT_TRUE(provider_event().is_placeholder());
}

TEST_F(provider_event_basics, groups_seeded_from_definition) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    EXPECT_TRUE(ev->is_part_of(group1_));
    EXPECT_TRUE(ev->is_part_of(group2_));
    EXPECT_EQ(ev->groups(), (std::set<eventgroup_t>{group1_, group2_}));
}

TEST_F(provider_event_basics, get_event_returns_definition_event) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    EXPECT_EQ(ev->get_event(), event_);
}

TEST_F(provider_event_basics, set_version_applied_to_notification) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->set_version(0x07);

    auto [msg, subscribers] = ev->update(make_payload({1}));

    EXPECT_EQ(msg->get_interface_version(), 0x07);
}

TEST_F(provider_event_basics, zero_cycle_constructs_no_timer) {
    (void)make_event(event_type_e::ET_FIELD, {group1_}, 0ms);
    EXPECT_EQ(factory_->last_timer_state_, nullptr);
}

TEST_F(provider_event_basics, nonzero_cycle_constructs_timer) {
    (void)make_event(event_type_e::ET_FIELD, {group1_}, 100ms);
    EXPECT_NE(factory_->last_timer_state_, nullptr);
}

// ---------------------------------------------------------------------------
// Subscriber management
// ---------------------------------------------------------------------------
struct provider_event_subscribers : provider_event_test { };

TEST_F(provider_event_subscribers, add_subscriber_known_group_adds_client) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);
    EXPECT_EQ(ev->get_subscriber(group1_), std::set<client_t>{client1_});
}

TEST_F(provider_event_subscribers, add_subscriber_unknown_group_on_real_event_rejected) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group2_, client1_, nullptr); // group2_ is not part of the definition

    EXPECT_FALSE(ev->is_part_of(group2_));
    EXPECT_FALSE(ev->is_subscribed(client1_));
}

TEST_F(provider_event_subscribers, add_subscriber_unknown_group_on_placeholder_creates_group) {
    auto ev = provider_event();
    ev.add_subscriber(group1_, client1_, nullptr);

    EXPECT_TRUE(ev.is_part_of(group1_));
    EXPECT_EQ(ev.get_subscriber(group1_), std::set<client_t>{client1_});
}

TEST_F(provider_event_subscribers, remove_subscriber_removes_client) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);
    ev->remove_subscriber(group1_, client1_);
    EXPECT_TRUE(ev->get_subscriber(group1_).empty());
}

TEST_F(provider_event_subscribers, remove_subscriber_unknown_group_is_noop) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);

    ev->remove_subscriber(group2_, client1_); // unknown group: no effect, no crash

    EXPECT_EQ(ev->get_subscriber(group1_), std::set<client_t>{client1_});
}

TEST_F(provider_event_subscribers, unsubscribe_removes_from_all_groups_and_returns_them) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    ev->add_subscriber(group1_, client1_, nullptr);
    ev->add_subscriber(group2_, client1_, nullptr);

    auto groups = ev->unsubscribe(client1_);

    EXPECT_EQ(groups, (std::set<eventgroup_t>{group1_, group2_}));
    EXPECT_FALSE(ev->is_subscribed(client1_));
}

TEST_F(provider_event_subscribers, unsubscribe_unknown_client_returns_empty) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    EXPECT_TRUE(ev->unsubscribe(client1_).empty());
}

TEST_F(provider_event_subscribers, clear_subscriber_returns_and_empties_all) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    ev->add_subscriber(group1_, client1_, nullptr);
    ev->add_subscriber(group2_, client2_, nullptr);

    auto previous = ev->clear_subscriber();

    EXPECT_EQ(previous[group1_], std::set<client_t>{client1_});
    EXPECT_EQ(previous[group2_], std::set<client_t>{client2_});
    EXPECT_FALSE(ev->is_subscribed(client1_));
    EXPECT_FALSE(ev->is_subscribed(client2_));
}

TEST_F(provider_event_subscribers, clear_subscriber_preserves_structural_groups) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    ev->add_subscriber(group1_, client1_, nullptr);

    ev->clear_subscriber();

    // The structural groups from the definition survive, so subscribers can be added again
    // (e.g. after a host reconnect).
    EXPECT_TRUE(ev->is_part_of(group1_));
    EXPECT_TRUE(ev->is_part_of(group2_));
    ev->add_subscriber(group1_, client1_, nullptr);
    EXPECT_EQ(ev->get_subscriber(group1_), std::set<client_t>{client1_});
}

TEST_F(provider_event_subscribers, is_subscribed_true_when_in_any_group) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    ev->add_subscriber(group2_, client1_, nullptr);
    EXPECT_TRUE(ev->is_subscribed(client1_));
}

TEST_F(provider_event_subscribers, is_subscribed_false_when_absent) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    EXPECT_FALSE(ev->is_subscribed(client1_));
}

TEST_F(provider_event_subscribers, is_part_of_reflects_group_membership) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    EXPECT_TRUE(ev->is_part_of(group1_));
    EXPECT_FALSE(ev->is_part_of(group2_));
}

TEST_F(provider_event_subscribers, get_subscriber_returns_members_or_empty) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);
    EXPECT_EQ(ev->get_subscriber(group1_), std::set<client_t>{client1_});
    EXPECT_TRUE(ev->get_subscriber(group2_).empty()); // unknown group
}

TEST_F(provider_event_subscribers, reset_clears_subscribers_and_current) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);
    (void)ev->update(make_payload({1, 2, 3}));

    ev->reset();

    EXPECT_FALSE(ev->is_subscribed(client1_));
    EXPECT_EQ(ev->get_current(), nullptr);
}

// ---------------------------------------------------------------------------
// adopt_subscribers (placeholder -> real transfer)
// ---------------------------------------------------------------------------
struct provider_event_adopt : provider_event_test { };

TEST_F(provider_event_adopt, transfers_subscribers_onto_valid_groups) {
    provider_event placeholder;
    placeholder.add_subscriber(group1_, client1_, nullptr);
    placeholder.add_subscriber(group2_, client2_, nullptr);

    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    ev->adopt_subscribers(placeholder);

    EXPECT_EQ(ev->get_subscriber(group1_), std::set<client_t>{client1_});
    EXPECT_EQ(ev->get_subscriber(group2_), std::set<client_t>{client2_});
}

TEST_F(provider_event_adopt, drops_subscribers_for_groups_the_event_does_not_define) {
    provider_event placeholder;
    placeholder.add_subscriber(group2_, client1_, nullptr); // group2 not offered by the real event below

    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->adopt_subscribers(placeholder);

    EXPECT_FALSE(ev->is_subscribed(client1_));
    EXPECT_FALSE(ev->is_part_of(group2_));
}

TEST_F(provider_event_adopt, preserves_per_client_debounce_filter) {
    // A client subscribes on the placeholder with an on_change filter. After the real event
    // adopts the placeholder's subscribers, that filter must still gate the client's forwards
    // (regression guard: the transfer previously re-added subscribers without their filter).
    provider_event placeholder;
    placeholder.add_subscriber(group1_, client1_, make_filter(true)); // on_change

    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->adopt_subscribers(placeholder);

    ASSERT_TRUE(ev->is_subscribed(client1_));
    EXPECT_EQ(ev->update(make_payload({1, 2, 3})).second, std::set<client_t>{client1_}); // changed -> forward
    EXPECT_TRUE(ev->update(make_payload({1, 2, 3})).second.empty()); // unchanged -> on_change filter suppresses
}

// ---------------------------------------------------------------------------
// update() / payload
// ---------------------------------------------------------------------------
struct provider_event_update : provider_event_test { };

TEST_F(provider_event_update, update_builds_notification_with_definition_fields) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    auto [msg, subscribers] = ev->update(make_payload({1}));

    EXPECT_EQ(msg->get_service(), service_);
    EXPECT_EQ(msg->get_instance(), instance_);
    EXPECT_EQ(msg->get_method(), event_);
}

TEST_F(provider_event_update, update_sets_payload_on_current) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    auto payload = make_payload({9, 8, 7});

    auto [msg, subscribers] = ev->update(payload);

    ASSERT_NE(msg->get_payload(), nullptr);
    EXPECT_TRUE(*msg->get_payload() == *payload);
}

TEST_F(provider_event_update, update_returns_all_subscribers_across_groups) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    ev->add_subscriber(group1_, client1_, nullptr);
    ev->add_subscriber(group2_, client2_, nullptr);

    auto [msg, subscribers] = ev->update(make_payload({1}));

    EXPECT_EQ(subscribers, (std::set<client_t>{client1_, client2_}));
}

TEST_F(provider_event_update, update_deduplicates_multi_group_client) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_, group2_});
    ev->add_subscriber(group1_, client1_, nullptr);
    ev->add_subscriber(group2_, client1_, nullptr);

    auto [msg, subscribers] = ev->update(make_payload({1}));

    EXPECT_EQ(subscribers, std::set<client_t>{client1_});
}

TEST_F(provider_event_update, update_reuses_single_current_slot) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});

    auto [msg1, s1] = ev->update(make_payload({1}));
    auto [msg2, s2] = ev->update(make_payload({2}));

    EXPECT_EQ(msg1, msg2); // same message object reused
}

TEST_F(provider_event_update, update_without_subscriber_sets_payload_returns_message) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    auto payload = make_payload({5, 5});

    auto msg = ev->update_without_subscriber(payload);

    ASSERT_NE(msg, nullptr);
    EXPECT_TRUE(*msg->get_payload() == *payload);
}

TEST_F(provider_event_update, get_current_returns_message_for_field) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    (void)ev->update(make_payload({1}));
    EXPECT_NE(ev->get_current(), nullptr);
}

TEST_F(provider_event_update, get_current_returns_null_for_non_field) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    (void)ev->update(make_payload({1}));
    EXPECT_EQ(ev->get_current(), nullptr);
}

// ---------------------------------------------------------------------------
// update_on_change gate + field dedup (unchanged-value skip)
// ---------------------------------------------------------------------------
struct provider_event_gate : provider_event_test { };

TEST_F(provider_event_gate, update_on_change_false_suppresses_notification) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_}, 0ms, /*update_on_change=*/false);
    ev->add_subscriber(group1_, client1_, nullptr);

    auto [msg, subscribers] = ev->update(make_payload({1, 2, 3}));

    // Payload is still committed (single-slot), but no subscriber is notified.
    EXPECT_NE(msg, nullptr);
    EXPECT_TRUE(subscribers.empty());
}

TEST_F(provider_event_gate, update_on_change_true_forwards) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);

    auto [msg, subscribers] = ev->update(make_payload({1, 2, 3}));

    EXPECT_NE(msg, nullptr);
    EXPECT_EQ(subscribers, std::set<client_t>{client1_});
}

TEST_F(provider_event_gate, field_dedup_first_empty_payload_forwards) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);

    // The fresh current_ starts as a non-null empty payload, so an incoming empty payload is
    // byte-equal to it; only the is_set_ guard (first set) makes this forward.
    auto [msg, subscribers] = ev->update(make_payload({}));

    EXPECT_FALSE(subscribers.empty());
}

TEST_F(provider_event_gate, field_dedup_unchanged_payload_suppresses) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);

    (void)ev->update(make_payload({1, 2, 3}));
    auto [msg, subscribers] = ev->update(make_payload({1, 2, 3}));

    EXPECT_TRUE(subscribers.empty());
}

TEST_F(provider_event_gate, field_dedup_changed_payload_forwards) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);

    (void)ev->update(make_payload({1, 2, 3}));
    auto [msg, subscribers] = ev->update(make_payload({1, 2, 4}));

    EXPECT_EQ(subscribers, std::set<client_t>{client1_});
}

TEST_F(provider_event_gate, field_dedup_not_applied_to_non_field) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, nullptr);

    (void)ev->update(make_payload({1, 2, 3}));
    auto [msg, subscribers] = ev->update(make_payload({1, 2, 3}));

    // A plain event is not deduplicated: identical payload still forwards.
    EXPECT_FALSE(subscribers.empty());
}

TEST_F(provider_event_gate, field_dedup_not_applied_when_cyclic) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_}, 100ms);
    ev->add_subscriber(group1_, client1_, nullptr);

    (void)ev->update(make_payload({1, 2, 3}));
    auto [msg, subscribers] = ev->update(make_payload({1, 2, 3}));

    // cycle > 0 disables field dedup (matches event: guard requires cycle == 0).
    EXPECT_FALSE(subscribers.empty());
}

// ---------------------------------------------------------------------------
// per-subscriber epsilon / debounce filtering
// ---------------------------------------------------------------------------
struct provider_event_filters : provider_event_test { };

TEST_F(provider_event_filters, no_filter_uses_default_epsilon_change_func) {
    bool consulted = false;
    auto epsilon = [&consulted](std::shared_ptr<payload> const&, std::shared_ptr<payload> const&) {
        consulted = true;
        return true;
    };
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 0ms, true, false, epsilon);
    ev->add_subscriber(group1_, client1_, nullptr);

    auto [msg, subscribers] = ev->update(make_payload({1}));

    EXPECT_TRUE(consulted);
    EXPECT_EQ(subscribers, std::set<client_t>{client1_});
}

TEST_F(provider_event_filters, default_epsilon_change_func_can_suppress) {
    auto epsilon = [](std::shared_ptr<payload> const&, std::shared_ptr<payload> const&) { return false; };
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 0ms, true, false, epsilon);
    ev->add_subscriber(group1_, client1_, nullptr);

    auto [msg, subscribers] = ev->update(make_payload({1}));

    EXPECT_TRUE(subscribers.empty());
}

TEST_F(provider_event_filters, filter_on_change_forwards_only_on_byte_change) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true));

    EXPECT_EQ(ev->update(make_payload({1, 2, 3})).second, std::set<client_t>{client1_}); // changed
    EXPECT_TRUE(ev->update(make_payload({1, 2, 3})).second.empty()); // unchanged
    EXPECT_EQ(ev->update(make_payload({1, 2, 4})).second, std::set<client_t>{client1_}); // changed
}

TEST_F(provider_event_filters, filter_on_change_ignore_mask_full_byte) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true, -1, false, {{1, 0xFF}}));

    (void)ev->update(make_payload({1, 2, 3})); // first forward
    EXPECT_TRUE(ev->update(make_payload({1, 0x99, 3})).second.empty()); // only fully-ignored byte changed
}

TEST_F(provider_event_filters, filter_on_change_ignore_mask_partial_bits) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true, -1, false, {{0, 0x0F}})); // ignore low nibble

    (void)ev->update(make_payload({0x10})); // first forward
    EXPECT_TRUE(ev->update(make_payload({0x1F})).second.empty()); // only ignored low nibble changed
    EXPECT_EQ(ev->update(make_payload({0x2F})).second, std::set<client_t>{client1_}); // high nibble changed
}

TEST_F(provider_event_filters, filter_on_change_extra_bytes_ignored_when_masked) {
    auto masked = make_event(event_type_e::ET_EVENT, {group1_});
    masked->add_subscriber(group1_, client1_, make_filter(true, -1, false, {{3, 0xFF}}));
    (void)masked->update(make_payload({1, 2, 3}));
    EXPECT_TRUE(masked->update(make_payload({1, 2, 3, 9})).second.empty()); // extra byte fully masked

    auto unmasked = make_event(event_type_e::ET_EVENT, {group1_});
    unmasked->add_subscriber(group1_, client1_, make_filter(true));
    (void)unmasked->update(make_payload({1, 2, 3}));
    EXPECT_EQ(unmasked->update(make_payload({1, 2, 3, 9})).second, std::set<client_t>{client1_}); // extra byte counts
}

TEST_F(provider_event_filters, filter_interval_forwards_after_elapsed) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(false, 100));

    EXPECT_EQ(ev->update(make_payload({1})).second, std::set<client_t>{client1_}); // first forward
    EXPECT_TRUE(ev->update(make_payload({2})).second.empty()); // not elapsed
    clock_->advance(150ms);
    EXPECT_EQ(ev->update(make_payload({3})).second, std::set<client_t>{client1_}); // elapsed
}

TEST_F(provider_event_filters, filter_interval_suppresses_before_elapsed) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(false, 100));

    (void)ev->update(make_payload({1})); // first forward stores now
    clock_->advance(50ms);
    EXPECT_TRUE(ev->update(make_payload({2})).second.empty()); // 50 < 100
}

TEST_F(provider_event_filters, filter_interval_first_forward_always_allowed) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(false, 1000000));

    EXPECT_EQ(ev->update(make_payload({1})).second, std::set<client_t>{client1_}); // last_forwarded == max
}

TEST_F(provider_event_filters, filter_on_change_resets_interval_true_resets_on_change) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true, 100, true));

    (void)ev->update(make_payload({1})); // t=0   forward, last=0
    clock_->advance(80ms);
    (void)ev->update(make_payload({2})); // t=80  changed forward, resets last=80
    clock_->advance(40ms);
    EXPECT_TRUE(ev->update(make_payload({2})).second.empty()); // t=120 unchanged; elapsed 40 < 100 -> suppress
}

TEST_F(provider_event_filters, filter_on_change_resets_interval_false_keeps_interval) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true, 100, false));

    (void)ev->update(make_payload({1})); // t=0   forward, last=0
    clock_->advance(80ms);
    (void)ev->update(make_payload({2})); // t=80  changed forward, last stays 0 (not elapsed, no reset)
    clock_->advance(40ms);
    EXPECT_EQ(ev->update(make_payload({2})).second, std::set<client_t>{client1_}); // t=120 elapsed 120 >= 100
}

TEST_F(provider_event_filters, filter_forwards_on_change_or_elapsed) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true, 100, false));

    (void)ev->update(make_payload({1})); // first forward
    EXPECT_EQ(ev->update(make_payload({2})).second, std::set<client_t>{client1_}); // changed (not elapsed)
    EXPECT_TRUE(ev->update(make_payload({2})).second.empty()); // unchanged and not elapsed
    clock_->advance(150ms);
    EXPECT_EQ(ev->update(make_payload({2})).second, std::set<client_t>{client1_}); // unchanged but elapsed
}

TEST_F(provider_event_filters, mixed_filtered_and_default_subscribers_each_evaluated) {
    auto suppressing = [](std::shared_ptr<payload> const&, std::shared_ptr<payload> const&) { return false; };
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 0ms, true, false, suppressing);
    ev->add_subscriber(group1_, client1_, make_filter(true)); // on_change -> forwards on change
    ev->add_subscriber(group1_, client2_, nullptr); // default epsilon -> suppresses

    auto [msg, subscribers] = ev->update(make_payload({1}));

    EXPECT_EQ(subscribers, std::set<client_t>{client1_});
}

TEST_F(provider_event_filters, add_subscriber_null_filter_removes_existing_filter) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true)); // install on_change filter
    ev->add_subscriber(group1_, client1_, nullptr); // null filter removes it

    (void)ev->update(make_payload({1}));
    // With the filter removed and the default (null) epsilon, an unchanged repeat still forwards.
    EXPECT_EQ(ev->update(make_payload({1})).second, std::set<client_t>{client1_});
}

TEST_F(provider_event_filters, clear_subscriber_clears_filters) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(true));
    ev->clear_subscriber();

    ev->add_subscriber(group1_, client1_, nullptr); // re-subscribe without a filter
    (void)ev->update(make_payload({1}));
    EXPECT_EQ(ev->update(make_payload({1})).second, std::set<client_t>{client1_}); // no stale filter
}

TEST_F(provider_event_filters, filter_uses_injected_clock_not_wall_clock) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_});
    ev->add_subscriber(group1_, client1_, make_filter(false, 100));

    (void)ev->update(make_payload({1})); // first forward stores fake now (0)
    EXPECT_TRUE(ev->update(make_payload({2})).second.empty()); // fake clock not advanced -> not elapsed
    clock_->advance(150ms);
    EXPECT_EQ(ev->update(make_payload({3})).second, std::set<client_t>{client1_}); // advancing fake clock forwards
}

// ---------------------------------------------------------------------------
// cyclic timer (+ change_resets_cycle) and rmc poke
// ---------------------------------------------------------------------------
struct provider_event_cyclic : provider_event_test { };

TEST_F(provider_event_cyclic, tick_invokes_periodic_poke) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);

    (void)ev->update(make_payload({1})); // first payload -> cycle starts
    ASSERT_EQ(state->start_count_, 1u);

    state->handler_(boost::system::error_code{}); // one cycle elapses
    EXPECT_EQ(poke_count_, 1u);
}

TEST_F(provider_event_cyclic, reschedules_while_alive) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1}));

    state->handler_(boost::system::error_code{});
    state->handler_(boost::system::error_code{});

    EXPECT_EQ(poke_count_, 2u); // keeps rescheduling
}

TEST_F(provider_event_cyclic, runs_regardless_of_subscriber_count) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms); // no subscribers
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1}));

    state->handler_(boost::system::error_code{});

    EXPECT_EQ(poke_count_, 1u); // pokes even with zero subscribers
}

TEST_F(provider_event_cyclic, stopped_on_reset) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1}));

    ev->reset();
    state->handler_(boost::system::error_code{}); // the pending wait must not run the task

    EXPECT_EQ(poke_count_, 0u);
}

TEST_F(provider_event_cyclic, add_subscriber_does_not_touch_timer) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1}));
    auto const starts = state->start_count_;
    auto const cancels = state->cancel_count_;

    ev->add_subscriber(group1_, client1_, nullptr);

    EXPECT_EQ(state->start_count_, starts);
    EXPECT_EQ(state->cancel_count_, cancels);
}

TEST_F(provider_event_cyclic, unsubscribe_does_not_touch_timer) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    ev->add_subscriber(group1_, client1_, nullptr);
    (void)ev->update(make_payload({1}));
    auto const starts = state->start_count_;
    auto const cancels = state->cancel_count_;

    ev->remove_subscriber(group1_, client1_);
    (void)ev->unsubscribe(client1_);

    EXPECT_EQ(state->start_count_, starts);
    EXPECT_EQ(state->cancel_count_, cancels);
}

TEST_F(provider_event_cyclic, cyclic_send_reads_current_directly) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms); // non-field
    ev->add_subscriber(group1_, client1_, nullptr);
    auto payload = make_payload({7, 7});
    (void)ev->update(payload);

    auto [msg, subscribers] = ev->cyclic();

    ASSERT_NE(msg, nullptr);
    EXPECT_TRUE(*msg->get_payload() == *payload);
    EXPECT_EQ(subscribers, std::set<client_t>{client1_});
    EXPECT_EQ(ev->get_current(), nullptr); // get_current is field-gated; cyclic bypasses it
}

TEST_F(provider_event_cyclic, update_resets_cycle_true_resets_interval_on_accepted_change) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms, /*update_on_change*/ true, /*update_resets_cycle*/ true);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1})); // first set -> start
    auto const starts = state->start_count_;

    (void)ev->update(make_payload({2})); // accepted change -> restart

    EXPECT_GT(state->start_count_, starts);
}

TEST_F(provider_event_cyclic, update_resets_cycle_false_does_not_reset) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms, /*update_on_change*/ true, /*update_resets_cycle*/ false);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1}));
    auto const starts = state->start_count_;

    (void)ev->update(make_payload({2})); // change, but update_resets_cycle == false

    EXPECT_EQ(state->start_count_, starts);
}

TEST_F(provider_event_cyclic, update_resets_cycle_resets_on_unchanged_cyclic_update) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_}, 100ms, /*update_on_change*/ true, /*update_resets_cycle*/ true);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1}));
    auto const starts = state->start_count_;

    (void)ev->update(make_payload({1})); // unchanged, but cyclic -> still restarts

    EXPECT_GT(state->start_count_, starts);
}

TEST_F(provider_event_cyclic, update_resets_cycle_not_reset_when_gate_closed) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 100ms, /*update_on_change*/ false, /*update_resets_cycle*/ true);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);
    (void)ev->update(make_payload({1})); // first set still starts the cycle
    auto const starts = state->start_count_;

    (void)ev->update(make_payload({2})); // gate closed -> not forwarded -> no restart

    EXPECT_EQ(state->start_count_, starts);
}

TEST_F(provider_event_cyclic, zero_cycle_never_pokes) {
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 0ms); // no timer
    EXPECT_EQ(factory_->last_timer_state_, nullptr);

    (void)ev->update(make_payload({1}));

    EXPECT_EQ(factory_->last_timer_state_, nullptr);
    EXPECT_EQ(poke_count_, 0u);
}

// ---------------------------------------------------------------------------
// notify_one gating parity
// ---------------------------------------------------------------------------
struct provider_event_notify_one : provider_event_test { };

TEST_F(provider_event_notify_one, honors_update_on_change_gate) {
    // update_on_change=false: the notify_one payload update is committed but not forwarded (null).
    auto ev = make_event(event_type_e::ET_FIELD, {group1_}, 0ms, /*update_on_change*/ false);

    auto msg = ev->update_without_subscriber(make_payload({1}));

    EXPECT_EQ(msg, nullptr);
    EXPECT_NE(ev->get_current(), nullptr); // still committed (single-slot commit-always)
}

TEST_F(provider_event_notify_one, honors_field_dedup) {
    // A set FIELD re-notified with the identical value is suppressed (field dedup), parity with event.
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});

    ASSERT_NE(ev->update_without_subscriber(make_payload({1, 2})), nullptr); // first set forwards
    EXPECT_EQ(ev->update_without_subscriber(make_payload({1, 2})), nullptr); // unchanged -> deduped
}

TEST_F(provider_event_notify_one, ignores_per_subscriber_epsilon) {
    // notify_one sends directly (event::notify_one_unlocked): neither the event-wide epsilon nor a
    // per-subscriber debounce filter is consulted. Use ET_EVENT to avoid field dedup masking the point.
    auto ev = make_event(event_type_e::ET_EVENT, {group1_}, 0ms, /*update_on_change*/ true,
                         /*update_resets_cycle*/ false,
                         /*epsilon*/ [](auto const&, auto const&) { return false; }); // would suppress everything
    ev->add_subscriber(group1_, client1_, make_filter(/*on_change*/ true)); // per-client filter that would suppress

    auto msg = ev->update_without_subscriber(make_payload({9}));

    EXPECT_NE(msg, nullptr); // forwarded despite both would-be suppressing filters
}

TEST_F(provider_event_notify_one, update_without_subscriber_gates_and_may_return_null) {
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});

    EXPECT_NE(ev->update_without_subscriber(make_payload({1})), nullptr); // first set forwards
    EXPECT_EQ(ev->update_without_subscriber(make_payload({1})), nullptr); // unchanged -> null
    EXPECT_NE(ev->update_without_subscriber(make_payload({2})), nullptr); // changed -> forwards again
}

TEST_F(provider_event_notify_one, no_send_before_payload_set) {
    // A never-set field yields no initial value: the initial-value push path (get_current) returns null,
    // so a subscribing client is not sent a bogus notification. Parity with event's is_set_ guard.
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});

    EXPECT_EQ(ev->get_current(), nullptr);
}

TEST_F(provider_event_notify_one, field_dedup_bypassed_by_force) {
    // force bypasses field dedup (parity with event::prepare_update_payload_unlocked's !_force).
    auto ev = make_event(event_type_e::ET_FIELD, {group1_});

    ASSERT_NE(ev->update_without_subscriber(make_payload({1, 2}), /*force*/ false), nullptr);
    EXPECT_NE(ev->update_without_subscriber(make_payload({1, 2}), /*force*/ true), nullptr); // unchanged, but forced
}

TEST_F(provider_event_notify_one, starts_cycle_on_first_set_but_never_resets) {
    // Q3: the notify_one path starts the cycle on first set (shared prepare_update_payload_unlocked
    // start_cycle) but NEVER restarts it on later updates -- even with update_resets_cycle enabled,
    // because event's notify_one set_payload overloads omit the stop/start bracket.
    auto ev = make_event(event_type_e::ET_FIELD, {group1_}, 100ms, /*update_on_change*/ true,
                         /*update_resets_cycle*/ true);
    auto state = factory_->last_timer_state_;
    ASSERT_NE(state, nullptr);

    (void)ev->update_without_subscriber(make_payload({1})); // first set -> cycle starts
    EXPECT_EQ(state->start_count_, 1u);

    (void)ev->update_without_subscriber(make_payload({2})); // changed value, but notify_one must not reset
    EXPECT_EQ(state->start_count_, 1u);
}

} // namespace vsomeip_v3::testing
