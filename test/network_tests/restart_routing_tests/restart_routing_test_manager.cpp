// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <unordered_set>
#include <thread>
#include <iostream>

#include <gtest/gtest.h>

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/mapped_region.hpp>

#include <common/process_manager.hpp>
#include "restart_routing_test_globals.hpp"

namespace bpi = boost::interprocess;

const std::string service_executor("./restart_routing_test_service");
const std::string client_executor("./restart_routing_test_client");
const std::string host_executor("../../../examples/routingmanagerd/routingmanagerd");

struct shared_memory_data {
    shared_memory_data() {};
    bpi::mapped_region region_;
    std::unique_ptr<restart_routing::restart_routing_test_interprocess_sync,
                    void (*)(restart_routing::restart_routing_test_interprocess_sync*)>
            sync_ptr{nullptr, nullptr};
};

class restart_routing_test_manager : public testing::Test {
protected:
    void SetUp() { std::cout << "Setting up restart_routing_test_manager" << std::endl; }

    void TearDown() {
        std::cout << "Tearing down restart_routing_test_manager" << std::endl;
        bpi::shared_memory_object::remove("RestartRoutingSync");
    }

    static void remover(restart_routing::restart_routing_test_interprocess_sync* ptr) { ptr->~restart_routing_test_interprocess_sync(); }

    void init(shared_memory_data& _shm_data) {
        // Create a shared memory object.
        bpi::shared_memory_object shm(bpi::open_or_create, // only create
                                      "RestartRoutingSync", // name
                                      bpi::read_write // read-write mode
        );
        // Set size
        shm.truncate(sizeof(restart_routing::restart_routing_test_interprocess_sync));
        // Mapping region
        _shm_data.region_ = bpi::mapped_region(shm, bpi::read_write);
        // Allocating
        _shm_data.sync_ptr = {new (_shm_data.region_.get_address()) restart_routing::restart_routing_test_interprocess_sync, &remover};
    }
};

auto custom_env = [](const std::string config, const std::string app_name, const std::string app_id = "") {
    std::map<std::string, std::string> app_env{{"VSOMEIP_CONFIGURATION", config}, {"VSOMEIP_APPLICATION_NAME", app_name}};
    if (!app_id.empty()) {
        app_env["VSOMEIP_APPLICATION_ID"] = app_id;
    }
    return app_env;
};

/**
 * Test that registered client using TCP endpoints for local comunication can properly recover after a host restart.
 */
TEST_F(restart_routing_test_manager, restart_routing_test_manager_restart_host) {
    shared_memory_data shm_data;
    init(shm_data);

    process_manager host{host_executor, custom_env("restart_routing_test_autoconfig.json", "routingmanagerd")};
    host.run();
    host.wait_for_start();

    process_manager service{service_executor, custom_env("restart_routing_test_autoconfig.json", "restart_routing_test_service")};
    service.run();

    std::unordered_set<std::unique_ptr<process_manager>> clients;
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        clients.emplace(std::make_unique<process_manager>(client_executor,
                                                          custom_env("restart_routing_test_autoconfig.json",
                                                                     "restart_routing_test_client" + std::to_string(app_counter + 1),
                                                                     std::to_string(app_counter))));
    }

    // Start service consumers processes.
    for (const auto& client : clients) {
        client->run();
    }

    // Wait for fist registration of all service consumers.
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        restart_routing::restart_routing_test_interprocess_utils::wait_and_check_unlocked(
                shm_data.sync_ptr->client_cv_, its_lock, 10, shm_data.sync_ptr->client_status_[app_counter],
                restart_routing::restart_routing_test_interprocess_sync::registration_status::STATE_REGISTERED);
    }

    // Restart host.
    host.reset();
    // Wait for restart to be completed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        // Inform service consumers that they can start sending requests
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->sending_status_ = restart_routing::restart_routing_test_interprocess_sync::sending_status::SEND_MESSAGES;
    }
    restart_routing::restart_routing_test_interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    // Wait for processes termination and test exit code.
    service.join();
    for (const auto& client : clients) {
        client->join();
        EXPECT_EQ(client->exit_code_, 0);
    }

    // Terminate host, do not assert exit code.
    host.terminate();
}

/**
 * Test that registered client using TCP endpoints for local comunication can properly recover after a host restart, even if the service is
 * provided by host.
 */
TEST_F(restart_routing_test_manager, restart_routing_test_manager_restart_service) {
    shared_memory_data shm_data;
    init(shm_data);

    process_manager service{service_executor,
                            custom_env("restart_routing_test_autoconfig_service_host.json", "restart_routing_test_service")};
    service.run();

    std::unordered_set<std::unique_ptr<process_manager>> clients;
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        clients.emplace(std::make_unique<process_manager>(client_executor,
                                                          custom_env("restart_routing_test_autoconfig_service_host.json",
                                                                     "restart_routing_test_client" + std::to_string(app_counter + 1),
                                                                     std::to_string(app_counter))));
    }

    // Start service consumers processes.
    for (const auto& client : clients) {
        client->run();
    }

    // Wait for fist registration of all service consumers.
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        restart_routing::restart_routing_test_interprocess_utils::wait_and_check_unlocked(
                shm_data.sync_ptr->client_cv_, its_lock, 10, shm_data.sync_ptr->client_status_[app_counter],
                restart_routing::restart_routing_test_interprocess_sync::registration_status::STATE_REGISTERED);
    }

    // Restart host.
    service.reset();
    // Wait for restart to be completed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        // Inform service consumers that they can start sending requests
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->sending_status_ = restart_routing::restart_routing_test_interprocess_sync::sending_status::SEND_MESSAGES;
    }
    restart_routing::restart_routing_test_interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    // Wait for processes termination and test exit code.
    for (const auto& client : clients) {
        client->join();
        EXPECT_EQ(client->exit_code_, 0);
    }
    service.join();
}

/**
 * Test that registered client using UDS endpoints for local comunication can properly recover after a host restart.
 */
TEST_F(restart_routing_test_manager, restart_routing_test_manager_restart_host_with_dedicated_config_uds) {
    shared_memory_data shm_data;
    init(shm_data);

    process_manager host{host_executor, custom_env("restart_routing_test_service.json", "routingmanagerd")};
    host.run();
    host.wait_for_start();

    process_manager service{service_executor, custom_env("restart_routing_test_service.json", "restart_routing_test_service")};
    service.run();

    std::unordered_set<std::unique_ptr<process_manager>> clients;
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        clients.emplace(std::make_unique<process_manager>(client_executor,
                                                          custom_env("restart_routing_test_client.json",
                                                                     "restart_routing_test_client" + std::to_string(app_counter + 1),
                                                                     std::to_string(app_counter))));
    }

    // Start service consumers processes.
    for (const auto& client : clients) {
        client->run();
    }

    // Wait for fist registration of all service consumers.
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        restart_routing::restart_routing_test_interprocess_utils::wait_and_check_unlocked(
                shm_data.sync_ptr->client_cv_, its_lock, 10, shm_data.sync_ptr->client_status_[app_counter],
                restart_routing::restart_routing_test_interprocess_sync::registration_status::STATE_REGISTERED);
    }

    // Restart host.
    host.reset();
    // Wait for restart to be completed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        // Inform service consumers that they can start sending requests
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->sending_status_ = restart_routing::restart_routing_test_interprocess_sync::sending_status::SEND_MESSAGES;
    }
    restart_routing::restart_routing_test_interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    // Wait for processes termination and test exit code.
    service.join();
    for (const auto& client : clients) {
        client->join();
        EXPECT_EQ(client->exit_code_, 0);
    }

    // Terminate host, do not assert exit code.
    host.terminate();
}

/**
 * Client id race condition.
 */
TEST_F(restart_routing_test_manager, client_id_race_condition) {
    shared_memory_data shm_data;
    init(shm_data);

    process_manager service{service_executor, custom_env("restart_routing_test_without_id.json", "restart_routing_test_service")};
    service.run();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    process_manager host{host_executor, custom_env("restart_routing_test_without_id.json", "routingmanagerd")};
    host.run();
    host.wait_for_start();

    std::unordered_set<std::unique_ptr<process_manager>> clients;
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        clients.emplace(std::make_unique<process_manager>(client_executor,
                                                          custom_env("restart_routing_test_without_id.json",
                                                                     "restart_routing_test_client" + std::to_string(app_counter + 1),
                                                                     std::to_string(app_counter))));
    }

    // Start service consumers processes.
    for (const auto& client : clients) {
        client->run();
    }

    // Wait for fist registration of all service consumers.
    for (uint32_t app_counter = 0; app_counter < NUM_SERVICE_CONSUMERS; ++app_counter) {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        restart_routing::restart_routing_test_interprocess_utils::wait_and_check_unlocked(
                shm_data.sync_ptr->client_cv_, its_lock, 10, shm_data.sync_ptr->client_status_[app_counter],
                restart_routing::restart_routing_test_interprocess_sync::registration_status::STATE_REGISTERED);
    }

    // Restart host.
    host.reset();
    // Wait for restart to be completed.
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        // Inform service consumers that they can start sending requests
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->sending_status_ = restart_routing::restart_routing_test_interprocess_sync::sending_status::SEND_MESSAGES;
    }
    restart_routing::restart_routing_test_interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    // Wait for processes termination and test exit code.
    service.join();
    for (const auto& client : clients) {
        client->join();
        EXPECT_EQ(client->exit_code_, 0);
    }

    // Terminate host, do not assert exit code.
    host.terminate();
}

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
