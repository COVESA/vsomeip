// Copyright(C) 2015 - 2025 Bayerische Motoren Werke Aktiengesellschaft(BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v.2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http: // mozilla.org/MPL/2.0/.
#ifndef REGISTRATION_CHECK_TEST_SERVICE_HPP
#define REGISTRATION_CHECK_TEST_SERVICE_HPP

#include <gtest/gtest.h>

#include <iostream>
#include <thread>
#include <mutex>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include <common/test_timer.hpp>

class service {
public:
    /// @brief Constructs the service and creates the vsomeip application instance.
    /// @note The application is not initialized until init() is called.
    service();

    /// @brief Stops vsomeip application
    ~service();

    /// @brief Initializes the vsomeip application and registers the message.
    ///
    /// @return true if vsomeip->init() was successful
    bool init();
    
    /// @brief Starts vsomeip application on a background thread. Non blocking call.
    void start();

    /// @brief Stops vsomeip application
    void stop();

    /// @brief Handler for application registration state change.
    ///
    /// @param state Current registration state
    void on_state(vsomeip::state_type_e _state);

    /// @brief Getter for application registration state
    ///
    /// @return true if registered
    bool is_registered();


private:
    /// @brief vsomeip app interface
    std::shared_ptr<vsomeip::application> vsomeip_app;

    std::mutex service_mutex;

    /// @brief background thread that will serve as context for the vsomeip application
    std::thread app_thread;

    std::atomic<vsomeip::state_type_e> registration_state;
};

/// @brief Timeout test
constexpr auto TEST_TIMEOUT = std::chrono::seconds(20);

/// @brief Time to wait to check if the service is registered
constexpr auto SLEEP_TIME = std::chrono::milliseconds(10);

#endif // REGISTRATION_CHECK_SERVICE_HPP
