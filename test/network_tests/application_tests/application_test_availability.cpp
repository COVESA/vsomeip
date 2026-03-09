// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include "application_test_client_availability.cpp"
#include "application_test_service.cpp"
#include "application_test_daemon.cpp"
#include "common/test_main.hpp"

TEST(someip_application_test_availability, register_availability_handlers) {
    // start application acting as daemon
    application_test_daemon its_daemon;

    // start receiver service
    application_test_service its_receiver(application_test::service);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // start client
    application_test_client_availability its_client(application_test::service);
    int counter(0);
    while (!its_client.all_availability_handlers_called() && counter < 500) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        counter++;
    }

    // shutdown
    its_receiver.stop();
    its_client.stop();
    its_daemon.stop();
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif
