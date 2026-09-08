// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_gate.hpp"
#include "helpers/command_message.hpp"
#include "helpers/service_state.hpp"
#include "helpers/sockets/fake_tcp_socket_handle.hpp"

#include "sample_interfaces.hpp"

#include "common/timeout_scale.hpp"

#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>

namespace vsomeip_v3::testing {
static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};

/**
 * Holds the thread that flushes the pending commands, just before the batch they belong
 * to becomes visible to the routing manager.
 *
 * A custom command handler is invoked inline on the thread that writes into the peer
 * socket, and fake_tcp_socket_handle::consume only hands the parsed batch over to the
 * pipe once its whole loop is done. Blocking here therefore keeps routing_manager_client
 * stalled inside send_pending_commands() with none of the pending commands forwarded.
 */
class flush_stall {
public:
    explicit flush_stall(protocol::id_e _id) : id_{_id} { }

    /// Never drops a command, it only delays the batch that carries it.
    bool operator()(command_message const& _command) {
        if (_command.id_ != id_) {
            return false;
        }
        std::unique_lock lock{mtx_};
        if (released_) {
            return false;
        }
        reached_ = true;
        cv_.notify_all();
        // bounded, so that a failing expectation cannot wedge the io thread for good
        (void)cv_.wait_for(lock, common::scaled_timeout(std::chrono::seconds(10)), [this] { return released_; });
        return false;
    }

    [[nodiscard]] bool wait_until_reached(std::chrono::milliseconds _timeout = common::scaled_timeout(std::chrono::seconds(3))) {
        std::unique_lock lock{mtx_};
        return cv_.wait_for(lock, _timeout, [this] { return reached_; });
    }

    void release() {
        std::scoped_lock lock{mtx_};
        released_ = true;
        cv_.notify_all();
    }

private:
    protocol::id_e const id_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool reached_{false};
    bool released_{false};
};

struct test_registration_ordering : public base_fake_socket_fixture {
    test_registration_ordering() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(server_name_);
    }

    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));
    }

    app* routingmanagerd_{};
    app* server_{};
};

/**
 * An application that offers a service becomes reachable the very moment its offer
 * arrives at the routing manager: a peer that already requested the service is told about
 * the availability and may answer with a request straight away. State and message
 * handlers share one dispatch queue, so the application has to observe ST_REGISTERED
 * before that can happen - otherwise it answers a request while it still considers itself
 * unregistered, and a binding that gates sending on the registration silently drops the
 * response.
 */
TEST_F(test_registration_ordering, host_is_notified_before_pending_offers_are_flushed) {
    start_router();

    // Hold back the registration ack, so that the offer below is still a pending one by
    // the time the registration is allowed to complete.
    auto gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_name_, routingmanager_name_, socket_role::client, gate->get_data_pipe()));
    gate->block_at(protocol::id_e::ASSIGN_CLIENT_ACK_ID);

    flush_stall stall{protocol::id_e::OFFER_SERVICE_ID};
    set_custom_command_handler(server_name_, routingmanager_name_, std::ref(stall), socket_role::client);

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(gate->wait_for_blocked());
    ASSERT_FALSE(server_->app_state_record_.wait_for_any(vsomeip::state_type_e::ST_REGISTERED, std::chrono::milliseconds(10)))
            << "the registration ack was supposed to be held back";

    // Nothing is sent while deregistered, so this offer is left for send_pending_commands.
    server_->offer(interfaces::cafe.instance_);
    gate->block(false);

    ASSERT_TRUE(stall.wait_until_reached()) << "the pending offer was never flushed";
    EXPECT_TRUE(server_->app_state_record_.wait_for_any(vsomeip::state_type_e::ST_REGISTERED))
            << "the pending offer became visible to the routing manager before the application "
               "was notified about its own registration";

    stall.release();

    EXPECT_TRUE(wait_for_command(server_name_, routingmanager_name_, protocol::id_e::OFFER_SERVICE_ID, socket_role::server));
}

TEST_F(test_registration_ordering, offers_arriving_during_the_registration_are_sent_once) {
    constexpr size_t its_offers{128};

    start_router();

    auto gate = command_gate::create();
    ASSERT_TRUE(setup_data_pipe(server_name_, routingmanager_name_, socket_role::client, gate->get_data_pipe()));
    gate->block_at(protocol::id_e::ASSIGN_CLIENT_ACK_ID);

    std::atomic<size_t> its_offer_count{0};
    set_custom_command_handler(
            server_name_, routingmanager_name_,
            [&its_offer_count](command_message const& _command) {
                if (_command.id_ == protocol::id_e::OFFER_SERVICE_ID) {
                    ++its_offer_count;
                }
                return false;
            },
            socket_role::client);

    server_ = start_client(server_name_);
    ASSERT_NE(server_, nullptr);
    ASSERT_TRUE(gate->wait_for_blocked());

    // Nothing is sent while deregistered, so this offer is left for send_pending_commands.
    server_->offer(interfaces::cafe.instance_);

    // From here the registration and the offers overlap.
    gate->block(false);

    // Each service is offered once, so each must produce exactly one command.
    for (size_t i = 0; i < its_offers; ++i) {
        server_->offer(service_instance{static_cast<service_t>(0x1000 + i), 0x1});
    }

    // Registered means the stop offer below is actually sent, so it can serve as the marker.
    ASSERT_TRUE(server_->app_state_record_.wait_for_any(vsomeip::state_type_e::ST_REGISTERED));

    // The local connection is FIFO: once the stop offer arrives, every offer written before it
    // has been counted.
    server_->stop_offer(interfaces::cafe.instance_);
    ASSERT_TRUE(wait_for_command(server_name_, routingmanager_name_, protocol::id_e::STOP_OFFER_SERVICE_ID, socket_role::server));
    EXPECT_EQ(its_offers + 1, its_offer_count.load()) << "an offer reached the routing manager more than once";
}
} // vsomeip_v3::testing
