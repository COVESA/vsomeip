// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>
#include "application_test_service.cpp"
#include "application_test_client.cpp"
#include "application_test_daemon.cpp"

TEST(someip_application_test_single_process, notify_increasing_counter)
{
    // start application acting as daemon (rm_stub)
    application_test_daemon its_daemon;

    // start receiver service (rm_proxy)
    application_test_service its_receiver(application_test::service);

    // stop the daemon (rm_stub goes away)
    its_daemon.stop();

    // restart client which tries to communicate with service multiple times
    // thus it will always be the new routing manager
    for (int var = 0; var < 10; ++var) {
        // every time the client is restarted it becomes the rm_stub again
        application_test_client its_client(application_test::service);
        if(var != 9) {
            its_client.stop(false);
        } else {
            // for the last iteration we sleep to make sure the communication
            // between the client and the service can be established
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            its_client.stop(true);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    its_receiver.on_shutdown_method_called(vsomeip::runtime::get()->create_message());
}


#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
