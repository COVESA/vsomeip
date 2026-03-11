// Copyright (C) 2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <thread>

#include <gtest/gtest.h>

#include <common/utility.hpp>

#include "mocks/mock_routing_manager_base.hpp"
#include "mocks/mock_routing_manager_host.hpp"
#include "../../../implementation/configuration/include/configuration_impl.hpp"

using namespace vsomeip_v3;

class rmb_fixture : public ::testing::Test {
protected:
    boost::asio::io_context io_;
    ::testing::NiceMock<mock_routing_manager_host> host_;

    std::shared_ptr<vsomeip_v3::configuration> cfg_ = std::make_shared<vsomeip_v3::cfg::configuration_impl>("");
    std::string name_{"unit_tests_routing_manager_tests"};

    void SetUp() override {
        ON_CALL(host_, get_io()).WillByDefault(::testing::ReturnRef(io_));
        ON_CALL(host_, get_name()).WillByDefault(::testing::ReturnRef(name_));
        ON_CALL(host_, get_configuration()).WillByDefault(::testing::Return(cfg_));
    }

public:
    service_t its_service = 0x1234;
    instance_t its_instance = 0x5678;
    eventgroup_t its_eventgroup = 0x9999;
    event_t its_event = 0x3333;
    std::shared_ptr<debounce_filter_impl_t> its_filter = std::make_shared<debounce_filter_impl_t>();
    client_t client_one = 0x1111;
    client_t client_two = 0x2222;
};

// Ensures that concurrent calls of insert_subscription does not result in multiple calls to create_placeholder_event_and_subscribe, as this
// will lead to clients failing to subscribe
TEST_F(rmb_fixture, double_insert_subscription) {

    vsomeip_v3::mock_rmb rmb(&host_);
    std::mutex subscription_mutex; // same as rmb::subscription_mutex

    EXPECT_CALL(rmb,
                create_placeholder_event_and_subscribe(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .Times(1);

    // Used to add concurrency and check if create_placeholder_event_and_subscribe() will be called twice
    std::thread its_thread([&]() {
        std::scoped_lock its_lock{subscription_mutex};
        (void)rmb.insert_subscription(its_service, its_instance, its_eventgroup, its_event, its_filter, client_one);
    });

    {
        std::scoped_lock its_lock{subscription_mutex};
        (void)rmb.insert_subscription(its_service, its_instance, its_eventgroup, its_event, its_filter, client_two);
    }

    its_thread.join();
}
