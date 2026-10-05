// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/service_state.hpp"

#include <boost/asio/ip/udp.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace vsomeip_v3::testing {

namespace {
std::string const router_one_name_{"router_one"};
std::string const ecu_one_client_name_{"ecu_one_client"};
std::string const router_two_name_{"router_two"};
std::string const ecu_two_server_name_{"ecu_two_server"};

// Burst parameters. The total (kBurstCount * kPayloadSize) must exceed the configured
// "endpoint-queue-limit-external" (4096 in ecu_two_queue_limited.json) so that, once
// sending is stalled, the limit is reached and later notifications are dropped while
// still pending in the batching stage.
constexpr size_t kBurstCount{100};
constexpr size_t kPayloadSize{512};
} // namespace

struct test_queue_limit_helper : public base_fake_socket_fixture {
    ~test_queue_limit_helper() {
        for (auto& [name, vsip_app] : apps_) {
            stop_client(name);
        }
        for (auto const& env : env_vars_) {
            ::unsetenv(env.c_str());
        }
    }

    app* start_application(std::string const& _app_name, std::string const& _config) {
        auto env{"VSOMEIP_CONFIGURATION_" + _app_name};
        if (!env_vars_.count(env)) {
            ::setenv(env.c_str(), _config.c_str(), 1);
            env_vars_.insert(env);
        }
        create_app(_app_name);
        auto* vsip_app = start_client(_app_name);
        apps_[_app_name] = vsip_app;
        return vsip_app;
    }

    [[nodiscard]] bool registered(app* _app) { return _app && _app->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED); }

    // Starts both ECUs. The sender ECU (ecu_two) uses the config passed in, so a
    // test can select the queue-limited or the default (unlimited) variant.
    void start_all_apps(std::string const& _ecu_two_config) {
        router_one_ = start_application(router_one_name_, "ecu_one.json");
        ecu_one_client_ = start_application(ecu_one_client_name_, "ecu_one.json");
        router_two_ = start_application(router_two_name_, _ecu_two_config);
        ecu_two_server_ = start_application(ecu_two_server_name_, _ecu_two_config);
        ASSERT_TRUE(registered(router_one_));
        ASSERT_TRUE(registered(ecu_one_client_));
        ASSERT_TRUE(registered(router_two_));
        ASSERT_TRUE(registered(ecu_two_server_));
    }

    void offer_and_subscribe() {
        ecu_two_server_->offer(service_instance_);
        ecu_two_server_->offer_event(offered_event_.si_, offered_event_.to_event_spec());

        ecu_one_client_->request_service(service_instance_);
        ASSERT_TRUE(ecu_one_client_->availability_record_.wait_for_last(service_availability::available(service_instance_)));
        ecu_one_client_->subscribe_event(offered_event_);
        ASSERT_TRUE(ecu_one_client_->subscription_record_.wait_for_last(event_subscription::successfully_subscribed_to(offered_event_)));
    }

    // Sends kBurstCount distinct notifications while the sender is stalled, then
    // releases the socket. Returns how many the client ultimately received.
    size_t stalled_burst_and_count() {
        // Stall only the data endpoint (subscription over SD has already completed).
        EXPECT_TRUE(delay_boardnet_sending(ecu_two_data_ep_, true));

        for (size_t i = 0; i < kBurstCount; ++i) {
            std::vector<unsigned char> payload(kPayloadSize, static_cast<unsigned char>(i));
            ecu_two_server_->send_event(offered_event_, payload);
        }

        // Release the socket; whatever survived the limit drains to the client.
        EXPECT_TRUE(delay_boardnet_sending(ecu_two_data_ep_, false));

        // Let the surviving notifications arrive, then read the recorded count.
        size_t received{0};
        (void)ecu_one_client_->message_record_.wait_for(
                [&](auto const& record) {
                    received = record.size();
                    return record.size() >= kBurstCount; // times out unless everything arrived
                },
                std::chrono::milliseconds(500));
        return received;
    }

    interface boardnet_interface_{0x3344};
    service_instance service_instance_{boardnet_interface_.instance_};
    event_ids offered_event_{service_instance_, boardnet_interface_.events_[0]};

    // Local bind endpoint of ecu_two's UDP server endpoint (the service's
    // unreliable port). Stalling its outgoing processing keeps the batched
    // notifications from draining.
    boost::asio::ip::udp::endpoint const ecu_two_data_ep_{boost::asio::ip::make_address("160.48.199.99"), 30501};

    std::map<std::string, app*> apps_;
    std::set<std::string> env_vars_;

    app* router_one_{};
    app* router_two_{};
    app* ecu_one_client_{};
    app* ecu_two_server_{};
};

// With a small external queue limit, a stalled burst must be partially dropped:
// the client cannot receive the whole burst because the batching stage is now
// counted against the limit.
TEST_F(test_queue_limit_helper, pending_trains_over_limit_cause_drops) {
    start_all_apps("ecu_two_queue_limited.json");
    offer_and_subscribe();

    const size_t received = stalled_burst_and_count();

    // Some notifications get through, but not the full burst: the queue limit
    // dropped the ones that would have exceeded queue_size_ + pending_train_size_.
    EXPECT_GT(received, 0u) << "expected at least some notifications to be delivered";
    EXPECT_LT(received, kBurstCount) << "expected the queue limit to drop part of the burst, but all " << kBurstCount << " were delivered";
}

} // namespace vsomeip_v3::testing
