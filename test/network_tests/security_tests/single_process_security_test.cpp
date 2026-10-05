// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include <common/vsomeip_app_utilities.hpp>

#include "common/test_main.hpp"

#include "security_service.hpp"
#include "security_client.hpp"

TEST(single_process_security_test, basic_subscribe_request_response) {
    security_client client(false, true, false, false);
    security_service service(false, true);

    auto router = vsomeip::runtime::get()->create_application("routingmanagerd");

    std::thread router_thread([&router]() {
        router->init();
        router->start();
    });

    std::thread client_thread([&client]() {
        if (client.init()) {
            client.start();
            client.join_sender_thread();
        }
    });

    std::thread service_thread([&service]() {
        if (service.init()) {
            service.start();
            service.join_offer_thread();
        }
    });

    if (client_thread.joinable()) {
        client_thread.join();
    }

    if (service_thread.joinable()) {
        service_thread.join();
    }

    router->stop();

    if (router_thread.joinable()) {
        router_thread.join();
    }
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif
