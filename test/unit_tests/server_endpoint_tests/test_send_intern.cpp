// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include "../../../implementation/endpoints/include/server_endpoint_impl.hpp"
#include "../../../implementation/endpoints/include/endpoint_definition.hpp"
#include "../../../implementation/service_discovery/include/defines.hpp"

using namespace vsomeip_v3;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::SetArgPointee;

namespace {

using target_data_iterator_type = server_endpoint_impl<boost::asio::ip::udp>::target_data_iterator_type;

// Mimics the production send_queued() behavior closely enough for the tests:
// drains whatever ended up in the target's queue (optionally handing the
// single buffer it contained back to the caller) and resets queue_size_.
bool drain_queue(const target_data_iterator_type _it, message_buffer_ptr_t* _captured_buffer = nullptr) {
    auto& its_queue = _it->second.queue_;
    if (_captured_buffer != nullptr && !its_queue.empty()) {
        *_captured_buffer = its_queue.front().first;
    }
    while (!its_queue.empty()) {
        its_queue.pop_front();
    }
    _it->second.queue_size_ = 0;
    return false;
}

class fake_server_endpoint : public server_endpoint_impl<boost::asio::ip::udp> {
public:
    explicit fake_server_endpoint(boost::asio::io_context& _io) :
        server_endpoint_impl<boost::asio::ip::udp>(nullptr, nullptr, _io, nullptr) {
        queue_limit_ = QUEUE_SIZE_UNLIMITED;
        max_message_size_ = VSOMEIP_MAX_UDP_MESSAGE_SIZE;

        // Defaults so that tests which don't care about a particular
        // (service, method) pair - or about send_queued() at all - still get
        // sane behavior without having to set up their own actions.
        ON_CALL(*this, get_configured_times_from_endpoint(_, _, _, _))
                .WillByDefault(
                        DoAll(SetArgPointee<2>(std::chrono::nanoseconds::zero()), SetArgPointee<3>(std::chrono::nanoseconds::zero())));
        ON_CALL(*this, send_queued(_)).WillByDefault(Invoke([](const target_data_iterator_type _it) { return drain_queue(_it); }));
    }

    using server_endpoint_impl<boost::asio::ip::udp>::send_intern;
    using server_endpoint_impl<boost::asio::ip::udp>::targets_;

    // Configures the debounce/retention values get_configured_times_from_endpoint()
    // reports for a given (service, method), the same way the real
    // configuration lookup would. Kept as a thin helper over ON_CALL so that
    // test bodies don't need to know about gmock actions/matchers.
    void set_configured_times(service_t _service, method_t _method, std::chrono::nanoseconds _debouncing,
                              std::chrono::nanoseconds _retention) {
        ON_CALL(*this, get_configured_times_from_endpoint(_service, _method, _, _))
                .WillByDefault(DoAll(SetArgPointee<2>(_debouncing), SetArgPointee<3>(_retention)));
    }

    void set_max_message_size(uint32_t _size) { max_message_size_ = _size; }

    void set_queue_limit(configuration::endpoint_queue_limit_t _limit) { queue_limit_ = _limit; }

    void init(const endpoint_type&, boost::system::error_code&) override { }

    MOCK_METHOD(bool, send_queued, (const target_data_iterator_type), (override));
    MOCK_METHOD(void, get_configured_times_from_endpoint, (service_t, method_t, std::chrono::nanoseconds*, std::chrono::nanoseconds*),
                (const, override));

    bool get_default_target(service_t, endpoint_type&) const override { return false; }
    void print_status() override { }
    bool is_reliable() const override { return false; }
    uint16_t get_local_port() const override { return 30509; }

    void start() override { }
    void receive() override { }
    bool is_local() const override { return false; }
    bool send_to(const std::shared_ptr<endpoint_definition>, const byte_t*, uint32_t) override { return true; }
    bool send_error(const std::shared_ptr<endpoint_definition>, const byte_t*, uint32_t) override { return true; }

private:
    std::string get_remote_information(const target_data_iterator_type) const override { return "test"; }
    std::string get_remote_information(const endpoint_type&) const override { return "test"; }
    bool tp_segmentation_enabled(service_instance_t, method_t) const override { return false; }
};

message_buffer_t make_message(service_t _service, method_t _method, session_t _session = 0x0001) {
    message_buffer_t its_message{
            static_cast<byte_t>((_service >> 8) & 0xFF),
            static_cast<byte_t>(_service & 0xFF),
            static_cast<byte_t>((_method >> 8) & 0xFF),
            static_cast<byte_t>(_method & 0xFF),
            0x00,
            0x00,
            0x00,
            0x08,
            0x00,
            0x00,
            static_cast<byte_t>((_session >> 8) & 0xFF),
            static_cast<byte_t>(_session & 0xFF),
            0x01,
            0x01,
            0x00,
            0x00,
    };
    return its_message;
}

struct send_intern_test : public ::testing::Test {
    boost::asio::io_context io_;
    std::shared_ptr<fake_server_endpoint> endpoint_ = std::make_shared<NiceMock<fake_server_endpoint>>(io_);
    boost::asio::ip::udp::endpoint target_{boost::asio::ip::make_address("127.0.0.1"), 30510};
};

} // namespace

TEST_F(send_intern_test, zero_debounce_and_retention_takes_fast_path) {
    endpoint_->set_configured_times(0x1234, 0x5678, std::chrono::nanoseconds::zero(), std::chrono::nanoseconds::zero());

    // The message must depart immediately: exactly one send_queued call,
    // carrying it.
    message_buffer_ptr_t its_sent_buffer;
    EXPECT_CALL(*endpoint_, send_queued(_)).Times(1).WillOnce(Invoke([&its_sent_buffer](const target_data_iterator_type _it) {
        return drain_queue(_it, &its_sent_buffer);
    }));

    const auto its_message = make_message(0x1234, 0x5678);
    ASSERT_TRUE(endpoint_->send_intern(target_, its_message.data(), static_cast<uint32_t>(its_message.size())));

    ASSERT_NE(its_sent_buffer, nullptr);
    EXPECT_EQ(its_message, *its_sent_buffer);

    // The per-target pending train was left completely untouched: no passenger
    // was ever recorded in it and it is still empty.
    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());
    EXPECT_TRUE(it->second.train_->passengers_.empty());
    EXPECT_TRUE(it->second.train_->buffer_->empty());
    EXPECT_TRUE(it->second.dispatched_trains_.empty());
}

TEST_F(send_intern_test, sd_message_takes_fast_path_and_skips_config_lookup) {
    // Even if debounce/retention is configured for the SD identifier,
    // it must never be looked up nor honored for SD messages.
    endpoint_->set_configured_times(VSOMEIP_SD_SERVICE, VSOMEIP_SD_METHOD, std::chrono::seconds(5), std::chrono::seconds(5));
    EXPECT_CALL(*endpoint_, get_configured_times_from_endpoint(_, _, _, _)).Times(0);

    message_buffer_ptr_t its_sent_buffer;
    EXPECT_CALL(*endpoint_, send_queued(_)).Times(1).WillOnce(Invoke([&its_sent_buffer](const target_data_iterator_type _it) {
        return drain_queue(_it, &its_sent_buffer);
    }));

    const auto its_message = make_message(VSOMEIP_SD_SERVICE, VSOMEIP_SD_METHOD);
    ASSERT_TRUE(endpoint_->send_intern(target_, its_message.data(), static_cast<uint32_t>(its_message.size())));

    ASSERT_NE(its_sent_buffer, nullptr);
    EXPECT_EQ(its_message, *its_sent_buffer);

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());
    EXPECT_TRUE(it->second.train_->passengers_.empty());
    EXPECT_TRUE(it->second.train_->buffer_->empty());
}

TEST_F(send_intern_test, non_zero_debounce_joins_train_instead_of_departing_immediately) {
    using namespace std::chrono_literals;
    endpoint_->set_configured_times(0x1234, 0x5678, 10ms, 100ms);

    // Old batching path: the message must be held in the pending train, never
    // handed off to send_queued.
    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_message = make_message(0x1234, 0x5678);
    ASSERT_TRUE(endpoint_->send_intern(target_, its_message.data(), static_cast<uint32_t>(its_message.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());
    EXPECT_FALSE(it->second.train_->passengers_.empty());
    EXPECT_EQ(its_message.size(), it->second.train_->buffer_->size());

    const auto its_departure = it->second.train_->departure_;
    const auto its_expiry = it->second.dispatch_timer_->expiry();
    EXPECT_GE(its_expiry, its_departure);
    EXPECT_LE(its_expiry - its_departure, std::chrono::milliseconds(20));
}

TEST_F(send_intern_test, two_distinct_identifiers_coalesce_into_the_same_train) {
    using namespace std::chrono_literals;
    endpoint_->set_configured_times(0x1111, 0x0001, 0ns, 100ms);
    endpoint_->set_configured_times(0x2222, 0x0002, 0ns, 100ms);

    // Nothing must be flushed: both messages get batched together.
    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_first = make_message(0x1111, 0x0001);
    const auto its_second = make_message(0x2222, 0x0002);

    ASSERT_TRUE(endpoint_->send_intern(target_, its_first.data(), static_cast<uint32_t>(its_first.size())));
    ASSERT_TRUE(endpoint_->send_intern(target_, its_second.data(), static_cast<uint32_t>(its_second.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());
    EXPECT_EQ(2u, it->second.train_->passengers_.size());
    EXPECT_EQ(its_first.size() + its_second.size(), it->second.train_->buffer_->size());
}

TEST_F(send_intern_test, repeating_same_identifier_forces_previous_train_to_depart) {
    using namespace std::chrono_literals;
    endpoint_->set_configured_times(0x1111, 0x0001, 0ns, 100ms);

    // The second message for the very same (service, method) forces the train
    // holding the first message to be scheduled for departure (moved to
    // dispatched_trains_, to be flushed once the dispatch timer fires) before
    // the new one starts a fresh, still-pending train. Neither ever reaches
    // send_queued() synchronously.
    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_first = make_message(0x1111, 0x0001, 0x0001);
    const auto its_second = make_message(0x1111, 0x0001, 0x0002);

    ASSERT_TRUE(endpoint_->send_intern(target_, its_first.data(), static_cast<uint32_t>(its_first.size())));
    ASSERT_TRUE(endpoint_->send_intern(target_, its_second.data(), static_cast<uint32_t>(its_second.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());
    ASSERT_EQ(1u, it->second.dispatched_trains_.size());
    ASSERT_EQ(1u, it->second.dispatched_trains_.begin()->second.size());
    EXPECT_EQ(its_first.size(), it->second.dispatched_trains_.begin()->second.front()->buffer_->size());

    EXPECT_EQ(1u, it->second.train_->passengers_.size());
    EXPECT_EQ(its_second.size(), it->second.train_->buffer_->size());
}

TEST_F(send_intern_test, full_train_buffer_forces_departure_of_distinct_identifier) {
    using namespace std::chrono_literals;
    // Both identifiers get a non-zero debounce/retention so the old batching
    // path (and thus the must_depart lambda) is taken at all.
    endpoint_->set_configured_times(0x1111, 0x0001, 10ms, 100ms);
    endpoint_->set_configured_times(0x2222, 0x0002, 10ms, 100ms);
    // Each message is 16 bytes (see make_message); shrink max_message_size_
    // so that a second, distinct-identifier message no longer fits into the
    // train that already holds the first one (16 + 16 > 20), forcing STEP 5
    // to fire before STEP 6/7 are ever reached.
    endpoint_->set_max_message_size(20);

    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_first = make_message(0x1111, 0x0001);
    const auto its_second = make_message(0x2222, 0x0002);

    ASSERT_TRUE(endpoint_->send_intern(target_, its_first.data(), static_cast<uint32_t>(its_first.size())));
    ASSERT_TRUE(endpoint_->send_intern(target_, its_second.data(), static_cast<uint32_t>(its_second.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());

    // The first (now full) train was scheduled for departure...
    ASSERT_EQ(1u, it->second.dispatched_trains_.size());
    ASSERT_EQ(1u, it->second.dispatched_trains_.begin()->second.size());
    const auto& its_departed_train = it->second.dispatched_trains_.begin()->second.front();
    EXPECT_EQ(its_first.size(), its_departed_train->buffer_->size());
    EXPECT_EQ(1u, its_departed_train->passengers_.size());

    // ...and the second, distinct-identifier message started a fresh train,
    // even though its own debounce/retention would otherwise have allowed it
    // to join the existing one.
    EXPECT_EQ(1u, it->second.train_->passengers_.size());
    EXPECT_EQ(its_second.size(), it->second.train_->buffer_->size());
}

TEST_F(send_intern_test, debounce_stricter_than_trains_minimal_max_retention_forces_departure) {
    using namespace std::chrono_literals;
    // First passenger commits the train to a 50ms minimal_max_retention_time_.
    endpoint_->set_configured_times(0x1111, 0x0001, 1ms, 50ms);
    // Second, distinct passenger demands a debounce (60ms) stricter than that
    // 50ms ceiling: the train's committed departure could already violate the
    // new passenger's debounce requirement, so it must depart now (STEP 6,
    // first check: its_debouncing > its_data.train_->minimal_max_retention_time_).
    endpoint_->set_configured_times(0x2222, 0x0002, 60ms, 10ms);

    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_first = make_message(0x1111, 0x0001);
    const auto its_second = make_message(0x2222, 0x0002);

    ASSERT_TRUE(endpoint_->send_intern(target_, its_first.data(), static_cast<uint32_t>(its_first.size())));
    ASSERT_TRUE(endpoint_->send_intern(target_, its_second.data(), static_cast<uint32_t>(its_second.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());

    ASSERT_EQ(1u, it->second.dispatched_trains_.size());
    ASSERT_EQ(1u, it->second.dispatched_trains_.begin()->second.size());
    EXPECT_EQ(its_first.size(), it->second.dispatched_trains_.begin()->second.front()->buffer_->size());

    EXPECT_EQ(1u, it->second.train_->passengers_.size());
    EXPECT_EQ(its_second.size(), it->second.train_->buffer_->size());
}

TEST_F(send_intern_test, debounce_would_undershoot_trains_committed_departure_forces_departure) {
    using namespace std::chrono_literals;
    // First passenger: debounce 5ms, retention 20ms. This commits the train
    // to minimal_max_retention_time_ = 20ms and departure_ = t1 + 20ms.
    endpoint_->set_configured_times(0x1111, 0x0001, 5ms, 20ms);
    // Second, distinct passenger: debounce == 20ms, i.e. NOT strictly greater
    // than minimal_max_retention_time_ (so STEP 6's first check is false),
    // but because this call happens strictly later than t1, t2 + 20ms is
    // already past the train's committed departure_ = t1 + 20ms (STEP 6,
    // second check: its_now + its_debouncing > its_data.train_->departure_).
    endpoint_->set_configured_times(0x2222, 0x0002, 20ms, 1ms);

    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_first = make_message(0x1111, 0x0001);
    const auto its_second = make_message(0x2222, 0x0002);

    ASSERT_TRUE(endpoint_->send_intern(target_, its_first.data(), static_cast<uint32_t>(its_first.size())));
    // Guarantee t2 is measurably later than t1 (steady_clock is monotonic, but
    // we want a comfortable margin rather than relying on sub-microsecond
    // scheduling luck).
    std::this_thread::sleep_for(2ms);
    ASSERT_TRUE(endpoint_->send_intern(target_, its_second.data(), static_cast<uint32_t>(its_second.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());

    ASSERT_EQ(1u, it->second.dispatched_trains_.size());
    ASSERT_EQ(1u, it->second.dispatched_trains_.begin()->second.size());
    EXPECT_EQ(its_first.size(), it->second.dispatched_trains_.begin()->second.front()->buffer_->size());

    EXPECT_EQ(1u, it->second.train_->passengers_.size());
    EXPECT_EQ(its_second.size(), it->second.train_->buffer_->size());
}

TEST_F(send_intern_test, retention_stricter_than_trains_minimal_debounce_forces_departure) {
    using namespace std::chrono_literals;
    // First passenger commits the train to minimal_debounce_time_ = 50ms and
    // departure_ = t1 + 200ms.
    endpoint_->set_configured_times(0x1111, 0x0001, 50ms, 200ms);
    // Second, distinct passenger: debounce (5ms) is well under the train's
    // minimal_max_retention_time_ (200ms) and t2 + 5ms is nowhere near the
    // train's departure_ (t1 + 200ms), so both STEP 6 checks are false.
    // But its retention (10ms) is stricter than the train's committed
    // minimal_debounce_time_ (50ms): the train's earliest possible departure
    // would already exceed this passenger's retention requirement, forcing
    // departure now (STEP 7, first check: its_retention <
    // its_data.train_->minimal_debounce_time_).
    endpoint_->set_configured_times(0x2222, 0x0002, 5ms, 10ms);

    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_first = make_message(0x1111, 0x0001);
    const auto its_second = make_message(0x2222, 0x0002);

    ASSERT_TRUE(endpoint_->send_intern(target_, its_first.data(), static_cast<uint32_t>(its_first.size())));
    ASSERT_TRUE(endpoint_->send_intern(target_, its_second.data(), static_cast<uint32_t>(its_second.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());

    ASSERT_EQ(1u, it->second.dispatched_trains_.size());
    ASSERT_EQ(1u, it->second.dispatched_trains_.begin()->second.size());
    EXPECT_EQ(its_first.size(), it->second.dispatched_trains_.begin()->second.front()->buffer_->size());

    EXPECT_EQ(1u, it->second.train_->passengers_.size());
    EXPECT_EQ(its_second.size(), it->second.train_->buffer_->size());
}

TEST_F(send_intern_test, tighter_retention_pulls_departure_earlier_without_forcing_departure) {
    using namespace std::chrono_literals;
    // First passenger commits the train to minimal_debounce_time_ = 50ms and
    // departure_ = t1 + 200ms.
    endpoint_->set_configured_times(0x1111, 0x0001, 50ms, 200ms);
    // Second, distinct passenger: debounce (5ms) keeps both STEP 6 checks
    // false as above. Its retention (100ms) is NOT stricter than the train's
    // minimal_debounce_time_ (50ms), so STEP 7's first check is false too.
    // But t2 + 100ms is earlier than the train's current departure_
    // (t1 + 200ms), so the else-branch fires: departure_ gets pulled forward
    // to t2 + 100ms, must_depart stays false, and the message joins the SAME
    // train instead of starting a new one.
    endpoint_->set_configured_times(0x2222, 0x0002, 5ms, 100ms);

    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    const auto its_first = make_message(0x1111, 0x0001);
    const auto its_second = make_message(0x2222, 0x0002);

    ASSERT_TRUE(endpoint_->send_intern(target_, its_first.data(), static_cast<uint32_t>(its_first.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());
    const auto its_departure_after_first = it->second.train_->departure_;
    const auto its_train_after_first = it->second.train_;

    const auto its_before_second = std::chrono::steady_clock::now();
    ASSERT_TRUE(endpoint_->send_intern(target_, its_second.data(), static_cast<uint32_t>(its_second.size())));

    // No departure happened: still no dispatched trains, and it is literally
    // the same train object as before (not a freshly created one).
    EXPECT_TRUE(it->second.dispatched_trains_.empty());
    EXPECT_EQ(its_train_after_first, it->second.train_);

    // Both passengers ended up in the same train.
    EXPECT_EQ(2u, it->second.train_->passengers_.size());
    EXPECT_EQ(its_first.size() + its_second.size(), it->second.train_->buffer_->size());

    // The departure was pulled earlier, to (approximately) t2 + 100ms, well
    // before the original t1 + 200ms commitment.
    const auto its_departure_after_second = it->second.train_->departure_;
    EXPECT_LT(its_departure_after_second, its_departure_after_first);
    EXPECT_GE(its_departure_after_second, its_before_second + 100ms);
    EXPECT_LE(its_departure_after_second, its_before_second + 100ms + std::chrono::milliseconds(20));
}

TEST_F(send_intern_test, queue_limit_reached_drops_message_without_touching_the_train) {
    using namespace std::chrono_literals;

    // Exactly one send_queued call is expected across this whole test: the
    // priming message (below) departs via the fast path, but the later,
    // queue-limit-rejected message must never reach send_queued() at all.
    EXPECT_CALL(*endpoint_, send_queued(_)).Times(1);

    // Prime the target's entry in targets_ via a harmless fast-path send (zero
    // debounce/retention, since 0x9999/0x0009 is not configured), so we can
    // then reach in and simulate an endpoint that already has bytes queued.
    const auto its_priming_message = make_message(0x9999, 0x0009);
    ASSERT_TRUE(endpoint_->send_intern(target_, its_priming_message.data(), static_cast<uint32_t>(its_priming_message.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());

    // Simulate 15 bytes already sitting in the (not-yet-flushed) output queue,
    // and cap the endpoint's queue limit at 20 bytes.
    it->second.is_sending_ = true;
    it->second.queue_size_ = 15;
    endpoint_->set_queue_limit(20);

    endpoint_->set_configured_times(0x1234, 0x5678, 0ns, 0ns);
    const auto its_message = make_message(0x1234, 0x5678);
    // 15 (already queued) + 16 (new message) = 31 > 20: check_queue_limit
    // must reject it before any train/queue logic runs.
    EXPECT_FALSE(endpoint_->send_intern(target_, its_message.data(), static_cast<uint32_t>(its_message.size())));

    // The simulated queue/train state is untouched: the message was dropped
    // outright (send_queued's single call, verified above, was for the
    // priming message only).
    EXPECT_EQ(15u, it->second.queue_size_);
    EXPECT_TRUE(it->second.train_->passengers_.empty());
    EXPECT_TRUE(it->second.train_->buffer_->empty());
    EXPECT_TRUE(it->second.dispatched_trains_.empty());
}

TEST_F(send_intern_test, oversized_message_takes_segment_message_path_and_is_dropped) {
    endpoint_->set_max_message_size(8);

    const auto its_message = make_message(0x1234, 0x5678);
    ASSERT_GT(its_message.size(), 8u);

    EXPECT_CALL(*endpoint_, send_queued(_)).Times(0);

    EXPECT_FALSE(endpoint_->send_intern(target_, its_message.data(), static_cast<uint32_t>(its_message.size())));

    auto it = endpoint_->targets_.find(target_);
    ASSERT_NE(it, endpoint_->targets_.end());
    EXPECT_TRUE(it->second.train_->passengers_.empty());
    EXPECT_TRUE(it->second.train_->buffer_->empty());
    EXPECT_TRUE(it->second.dispatched_trains_.empty());
}
