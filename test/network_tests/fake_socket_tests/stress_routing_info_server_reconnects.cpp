// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "helpers/app.hpp"
#include "helpers/attribute_recorder.hpp"
#include "helpers/base_fake_socket_fixture.hpp"
#include "helpers/command_record.hpp"
#include "helpers/fake_socket_factory.hpp"
#include "helpers/message_checker.hpp"
#include "helpers/sockets/fake_tcp_socket_handle.hpp"
#include "helpers/service_state.hpp"
#include "helpers/availability_checker.hpp"

#include "sample_interfaces.hpp"

#include <boost/asio/error.hpp>
#include <vsomeip/vsomeip.hpp>
#include <gtest/gtest.h>

#include <cstdlib>

namespace vsomeip_v3::testing {
static std::string const routingmanager_name_{"routingmanagerd"};
static std::string const server_name_{"server"};
static std::string const client_name_{"client"};

struct stress_server_reconnects : public base_fake_socket_fixture {
    stress_server_reconnects() {
        use_configuration("multiple_client_one_process.json");
        create_app(routingmanager_name_);
        create_app(server_name_);
        create_app(client_name_);
    }
    void start_router() {
        routingmanagerd_ = start_client(routingmanager_name_);
        ASSERT_NE(routingmanagerd_, nullptr);
        ASSERT_TRUE(await_connectable(routingmanager_name_));
    }

    void start_server() {
        server_ = start_client(server_name_);
        ASSERT_NE(server_, nullptr);
        ASSERT_TRUE(server_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
        server_->offer(service_instance_);
    }

    void start_client_app() {
        client_ = start_client(client_name_);
        ASSERT_NE(client_, nullptr);
        ASSERT_TRUE(client_->app_state_record_.wait_for_last(vsomeip::state_type_e::ST_REGISTERED));
    }

    void start_apps() {
        start_router();
        start_server();
        start_client_app();
    }

    void request_service() { client_->request_service(service_instance_); }
    [[nodiscard]] bool await_service() {
        return client_->availability_record_.wait_for_last(service_availability::available(service_instance_));
    }
    void answer_requests_with(std::vector<unsigned char> _payload) {
        server_->answer_request(request_, [payload = _payload] { return payload; });
    }

    interface interface_{0x3344};
    service_instance service_instance_{interface_.instance_};

    vsomeip_v3::method_t method_{0x1111};
    request request_{service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message_checker request_checker_{std::nullopt, service_instance_, method_, vsomeip::message_type_e::MT_REQUEST, {}};
    message_checker response_checker_{std::nullopt, service_instance_, method_, vsomeip::message_type_e::MT_RESPONSE, {}};

    app* routingmanagerd_{};
    app* client_{};
    app* server_{};
};

TEST_F(stress_server_reconnects, request_reply_works_after_server_reconnects) {
    // Regression test for the guest-map race:
    // 1. Server reconnects
    // 2. router adds the guest routing_info (formerly on_registration)
    // 3. router executes the error handler removing the routing_info
    // 4. router accepts offer service, but has no routing_info available any longer
    start_apps();
    request_service();
    ASSERT_TRUE(await_service());
    set_ignore_nothing_to_read_from(server_name_, routingmanager_name_, socket_role::server, true);
    std::vector<unsigned char> payload = {};
    // because there used to be a very tiny race window, some iterations were required
    // to reproduce the issue, but due to the restart sender debounce one iteration
    // needs at least 100 ms
    for (unsigned char i = 0; i < 50; ++i) {
        TEST_LOG << " ##### Start Iteration #####";
        // better clear the record
        client_->availability_record_.clear();
        ASSERT_TRUE(disconnect(server_name_, boost::asio::error::timed_out, routingmanager_name_, std::nullopt));

        // Wait for the service to cycle: unavailable then available again (routing info re-sent), but don't enforce with last,
        // as the re-availability can be very fast
        ASSERT_TRUE(client_->availability_record_.wait_for_any(service_availability::unavailable(service_instance_)));
        ASSERT_TRUE(client_->availability_record_.wait_for_any(service_availability::available(service_instance_)));

        // Send a request — requires the client to physically connect to the server.
        // Without the fix the routing info carries 0.0.0.0:0 and the request never arrives.
        payload.push_back(i);
        answer_requests_with(payload);
        request_.payload_ = payload;
        request_checker_.payload_ = payload;
        response_checker_.payload_ = payload;
        client_->send_request(request_);
        EXPECT_TRUE(server_->message_record_.wait_for(request_checker_));
        EXPECT_TRUE(client_->message_record_.wait_for(response_checker_));
    }
}
}
