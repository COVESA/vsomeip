// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include "common/test_main.hpp"
#include "reuse_client_id_test_globals.hpp"

// 20 is intentionally generous, execution times (especially with valgrind) can be quite long
#define REGISTRATION_TIMEOUT_S 20
#define CLIENT_1_IDX 0
#define CLIENT_2_IDX 1
#define CLIENT_3_IDX 2
#define CLIENT_4_IDX 3
#define CLIENT_5_IDX 4

namespace bpi = boost::interprocess;
namespace vt = vsomeip_test;

const std::string host_executor("../../../examples/routingmanagerd/routingmanagerd");

struct shared_memory_data {
    shared_memory_data() {};
    bpi::mapped_region region_;
    std::unique_ptr<reuse_client_id::reuse_client_id_test_interprocess_sync,
                    void (*)(reuse_client_id::reuse_client_id_test_interprocess_sync*)>
            sync_ptr{nullptr, nullptr};
};

class reuse_client_id_test_manager : public testing::Test {
protected:
    void SetUp() { bpi::shared_memory_object::remove("ReuseClientIdSync"); }

    void TearDown() { bpi::shared_memory_object::remove("ReuseClientIdSync"); }

    static void remove(reuse_client_id::reuse_client_id_test_interprocess_sync* ptr) { ptr->~reuse_client_id_test_interprocess_sync(); }

    void init(shared_memory_data& _shm_data) {
        // Create a shared memory object.
        bpi::shared_memory_object shm(bpi::open_or_create, // only create
                                      "ReuseClientIdSync", // name
                                      bpi::read_write // read-write mode
        );
        // Set size
        shm.truncate(sizeof(reuse_client_id::reuse_client_id_test_interprocess_sync));
        // Mapping region
        _shm_data.region_ = bpi::mapped_region(shm, bpi::read_write);
        // Allocating
        _shm_data.sync_ptr = {new (_shm_data.region_.get_address()) reuse_client_id::reuse_client_id_test_interprocess_sync, &remove};
    }

    bool wait_for_registration(shared_memory_data& _shm_data, int _app_id, int _timeout) {
        bool result;
        {
            bpi::scoped_lock<bpi::interprocess_mutex> its_lock(_shm_data.sync_ptr->client_mutex_);
            result = vt::interprocess_utils::wait_and_check_unlocked(
                    _shm_data.sync_ptr->client_cv_, its_lock, _timeout, _shm_data.sync_ptr->client_status_[_app_id],
                    reuse_client_id::reuse_client_id_test_interprocess_sync::registration_status::STATE_REGISTERED);
        }
        return result;
    }
};

auto custom_env = [](const std::string config, const std::string app_name, const std::string app_id = "") {
    std::map<std::string, std::string> app_env{{"VSOMEIP_CONFIGURATION", config}, {"VSOMEIP_APPLICATION_NAME", app_name}};
    if (!app_id.empty()) {
        app_env["VSOMEIP_APPLICATION_ID"] = app_id;
    }
    return app_env;
};

TEST_F(reuse_client_id_test_manager, reuse_client_id_test) {
    shared_memory_data shm_data;
    init(shm_data);

    process_manager host{host_executor, custom_env("reuse_client_id_test.json", "routingmanagerd")};
    host.run();

    // start client1
    auto client1 = std::make_unique<process_manager>("./reuse_client_id_test_client 6301 1",
                                                     custom_env("reuse_client_id_test.json", "client1", "0"));
    client1->run();
    EXPECT_TRUE(wait_for_registration(shm_data, CLIENT_1_IDX, REGISTRATION_TIMEOUT_S));

    // start client2
    auto client2 = std::make_unique<process_manager>("./reuse_client_id_test_client 6302 1",
                                                     custom_env("reuse_client_id_test.json", "client2", "1"));
    client2->run();
    EXPECT_TRUE(wait_for_registration(shm_data, CLIENT_2_IDX, REGISTRATION_TIMEOUT_S));

    // start client3
    auto client3 = std::make_unique<process_manager>("./reuse_client_id_test_client 6303 1",
                                                     custom_env("reuse_client_id_test.json", "client3", "2"));
    client3->run();
    EXPECT_TRUE(wait_for_registration(shm_data, CLIENT_3_IDX, REGISTRATION_TIMEOUT_S));

    // start client4
    // this client won't register due to no available clientIDs (diagnosis_mask limit)
    auto client4 = std::make_unique<process_manager>("./reuse_client_id_test_client 6304 0",
                                                     custom_env("reuse_client_id_test.json", "client4", "3"));
    client4->run();
    // since this app is expected to not register, do not wait as long, unecessary
    EXPECT_FALSE(wait_for_registration(shm_data, CLIENT_4_IDX, 3));

    // stop client4
    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->stop_clients_[CLIENT_4_IDX] = true;
    }
    vt::interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    EXPECT_EQ(client4->wait(), 0);

    // restart client1
    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->restart_clients_[CLIENT_1_IDX] = true;
    }
    vt::interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    // start client5
    auto client5 = std::make_unique<process_manager>("./reuse_client_id_test_client 6301 1",
                                                     custom_env("reuse_client_id_test.json", "client5", "4"));
    client5->run();
    wait_for_registration(shm_data, CLIENT_5_IDX, REGISTRATION_TIMEOUT_S);

    // stop client2
    // make clientID available
    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->stop_clients_[CLIENT_2_IDX] = true;
    }
    vt::interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);
    EXPECT_EQ(client2->wait(), 0);

    // restart client1
    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->restart_clients_[CLIENT_1_IDX] = false;
    }
    vt::interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    // allow client1 to get clientID (6302)
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    // stop remaining clients
    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(shm_data.sync_ptr->client_mutex_);
        shm_data.sync_ptr->stop_clients_[CLIENT_1_IDX] = true;
        shm_data.sync_ptr->stop_clients_[CLIENT_3_IDX] = true;
        shm_data.sync_ptr->stop_clients_[CLIENT_5_IDX] = true;
    }
    vt::interprocess_utils::notify_all_component_unlocked(shm_data.sync_ptr->client_cv_);

    EXPECT_EQ(client1->wait(), 0);
    EXPECT_EQ(client3->wait(), 0);
    EXPECT_EQ(client5->wait(), 0);

    host.send_signal(SIGINT);
    EXPECT_EQ(host.wait(), 0);
}

#if defined(__linux__) || defined(ANDROID) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif
