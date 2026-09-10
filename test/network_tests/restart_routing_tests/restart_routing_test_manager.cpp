// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <unordered_set>
#include <thread>

#include <gtest/gtest.h>

#include "common/test_main.hpp"
#include "restart_routing_test_globals.hpp"
#include "restart_routing_test_client.hpp"
#include "restart_routing_test_service.hpp"

struct restart_routing_managerd {
    restart_routing_managerd() : app_(vsomeip::runtime::get()->create_application("restart_routing_managerd")) { }

    ~restart_routing_managerd() {
        app_->clear_all_handler();
        app_->stop();

        if (starter_.joinable()) {
            starter_.join();
        }
    }

    void init() {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        starter_ = std::thread([&]() { app_->start(); });
    }

    std::thread starter_;
    std::shared_ptr<vsomeip::application> app_;
};

class restart_routing_test_manager : public testing::Test {
protected:
    void SetUp() { std::cout << "Setting up restart_routing_test_manager" << std::endl; }

    void TearDown() { std::cout << "Tearing down restart_routing_test_manager" << std::endl; }
};

/**
 * Test that registered client using TCP endpoints for local comunication can properly recover after a host restart.
 */
TEST_F(restart_routing_test_manager, restart_routing_test_manager_restart_host) {
    setenv("VSOMEIP_CONFIGURATION", "restart_routing_test_without_id.json", 1);
    std::unique_ptr<restart_routing_managerd> host{std::make_unique<restart_routing_managerd>()};
    host->init();

    restart_routing_test_service service{};
    service.init();

    std::unordered_set<std::unique_ptr<restart_routing_test_client>> clients;
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        clients.emplace(std::make_unique<restart_routing_test_client>());
    }

    // Start service consumers processes.
    for (const auto& client : clients) {
        client->init();
    }

    // Wait for fist registration of all service consumers.
    for (const auto& client : clients) {
        ASSERT_TRUE(client->wait_for_registration());
    }

    // Restart host.
    host.reset();
    host = std::make_unique<restart_routing_managerd>();
    host->init();
    // Wait for restart to be completed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (const auto& client : clients) {
        client->send_messages();
    }
    // Wait for processes termination and test exit code.
    for (const auto& client : clients) {
        ASSERT_TRUE(client->wait_for_responses());
    }
    clients.clear();
    ASSERT_TRUE(service.wait_for_messages());
}

/**
 * Test that registered client using TCP endpoints for local comunication can properly recover after a host restart, even if the service is
 * provided by host.
 */
TEST_F(restart_routing_test_manager, restart_routing_test_manager_restart_service) {
    setenv("VSOMEIP_CONFIGURATION", "restart_routing_test_autoconfig_service_host.json", 1);
    std::unique_ptr<restart_routing_test_service> service{std::make_unique<restart_routing_test_service>()};
    service->init();

    std::unordered_set<std::unique_ptr<restart_routing_test_client>> clients;
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        clients.emplace(std::make_unique<restart_routing_test_client>());
    }

    // Start service consumers processes.
    for (const auto& client : clients) {
        client->init();
    }

    // Wait for fist registration of all service consumers.
    for (const auto& client : clients) {
        ASSERT_TRUE(client->wait_for_registration());
    }
    // Restart host.
    service.reset();
    service = std::make_unique<restart_routing_test_service>();
    service->init();
    // Wait for restart to be completed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (const auto& client : clients) {
        client->send_messages();
    }

    // Wait for processes termination and test exit code.
    for (const auto& client : clients) {
        ASSERT_TRUE(client->wait_for_responses());
    }
    clients.clear();

    ASSERT_TRUE(service->wait_for_messages());
}

/**
 * Test that registered client using UDS endpoints for local comunication can properly recover after a host restart.
 */
TEST_F(restart_routing_test_manager, restart_routing_test_manager_restart_host_with_dedicated_config_uds) {
    setenv("VSOMEIP_CONFIGURATION", "restart_routing_test_uds.json", 1);
    auto host{std::make_unique<restart_routing_managerd>()};
    host->init();

    restart_routing_test_service service{};
    service.init();

    std::unordered_set<std::unique_ptr<restart_routing_test_client>> clients;
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        clients.emplace(std::make_unique<restart_routing_test_client>());
    }

    // Start service consumers processes.
    for (const auto& client : clients) {
        client->init();
    }

    // Wait for fist registration of all service consumers.
    for (const auto& client : clients) {
        ASSERT_TRUE(client->wait_for_registration());
    }

    // Restart host.
    host.reset();
    host = std::make_unique<restart_routing_managerd>();
    host->init();
    // Wait for restart to be completed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (const auto& client : clients) {
        client->send_messages();
    }

    // Wait for processes termination and test exit code.
    for (const auto& client : clients) {
        ASSERT_TRUE(client->wait_for_responses());
    }
    clients.clear();
    ASSERT_TRUE(service.wait_for_messages());
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif
