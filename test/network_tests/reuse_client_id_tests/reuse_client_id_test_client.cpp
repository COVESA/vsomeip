// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <iomanip>
#include <cstring>

#include "reuse_client_id_test_client.hpp"

#include <boost/interprocess/managed_shared_memory.hpp>
#include "common/test_main.hpp"

namespace bpi = boost::interprocess;
namespace vt = vsomeip_test;

uint16_t expected_clientid;
bool expected_to_register;

reuse_client_id_test_client::reuse_client_id_test_client(const char* app_name_, uint32_t _app_id) :
    app_(vsomeip::runtime::get()->create_application(app_name_)), app_id_{_app_id}, is_registered(false) {
    app_->register_state_handler(std::bind(&reuse_client_id_test_client::on_state, this, std::placeholders::_1));
}

void reuse_client_id_test_client::on_state(vsomeip::state_type_e _state) {
    is_registered = _state == vsomeip::state_type_e::ST_REGISTERED;
    if (is_registered) {
        {
            bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
            ip_sync->client_status_[app_id_] =
                    reuse_client_id::reuse_client_id_test_interprocess_sync::registration_status::STATE_REGISTERED;
        }
        vt::interprocess_utils::notify_all_component_unlocked(ip_sync->client_cv_);
    }
}

void reuse_client_id_test_client::run() {
    // Create a shared memory object.
    bpi::shared_memory_object shm(bpi::open_only, // only open
                                  "ReuseClientIdSync", // name
                                  bpi::read_write // read-write mode
    );
    // Map the whole shared memory in this process
    bpi::mapped_region region(shm, // What to map
                              bpi::read_write // Map it as read-write
    );
    void* addr = region.get_address();
    ip_sync = static_cast<reuse_client_id::reuse_client_id_test_interprocess_sync*>(addr);

    if (!app_->init()) {
        ADD_FAILURE() << "Couldn't initialize application";
        exit(1);
    }

    auto starter_ = std::thread([&]() { app_->start(); });

    auto restart = std::thread([&] {
        {
            bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
            ASSERT_TRUE(vt::interprocess_utils::wait_and_check_unlocked(ip_sync->client_cv_, its_lock, 20,
                                                                        ip_sync->restart_clients_[app_id_], true));
        }

        if (expected_to_register) {
            // check expected clientID before restart as next clientID might be different
            EXPECT_EQ(app_->get_client(), expected_clientid);
        }

        bool restarted{false};
        {
            bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
            restarted = ip_sync->restart_clients_[app_id_];
        }

        if (restarted && !stopping) {
            app_->stop();
            // wait for other client to register
            {
                bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
                ASSERT_TRUE(vt::interprocess_utils::wait_and_check_unlocked(ip_sync->client_cv_, its_lock, 10,
                                                                            ip_sync->restart_clients_[app_id_], false));
            }
            restarted = true;
            app_->start();
        }
    });

    // wait for notify from test manager
    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
        ASSERT_TRUE(
                vt::interprocess_utils::wait_and_check_unlocked(ip_sync->client_cv_, its_lock, 20, ip_sync->stop_clients_[app_id_], true));
    }

    // prevent restart
    stopping = true;
    // exit from restart thread
    {
        bpi::scoped_lock<bpi::interprocess_mutex> its_lock(ip_sync->client_mutex_);
        ip_sync->restart_clients_[app_id_] = true;
    }
    vt::interprocess_utils::notify_all_component_unlocked(ip_sync->client_cv_);

    EXPECT_EQ(is_registered, expected_to_register);

    // if restarted check that it received different clientID
    if (restarted) {
        EXPECT_NE(app_->get_client(), expected_clientid);
    }

    app_->stop();

    if (restart.joinable()) {
        restart.join();
    }

    if (starter_.joinable()) {
        starter_.join();
    }
}

TEST(reuse_client_id_test, start_app) {
    reuse_client_id_test_client reuse_client_id_test_client(
            "reuse_client_id_test_client", static_cast<std::uint32_t>(std::stoul(getenv("VSOMEIP_APPLICATION_ID"), NULL, 10)));
    reuse_client_id_test_client.run();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Not enough arguments provided" << std::endl;
        return 0;
    }

    expected_clientid = static_cast<uint16_t>(std::stoi(argv[1], nullptr, 16));
    expected_to_register = std::stoi(argv[2]);

    return test_main(argc, argv);
}
