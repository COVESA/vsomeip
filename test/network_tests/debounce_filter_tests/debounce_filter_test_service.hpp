// Copyright (C) 2023 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef DEBOUNCE_TEST_SERVICE_HPP_
#define DEBOUNCE_TEST_SERVICE_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>

#include "debounce_filter_test_common.hpp"

class debounce_test_service {
public:
    debounce_test_service();

    bool init();
    void start();
    void stop();

    void run();
    void wait();

private:
    void on_start(const std::shared_ptr<vsomeip::message> &);
    void on_stop(const std::shared_ptr<vsomeip::message> &);

    void start_test();

    std::mutex run_mutex_;
    std::condition_variable run_condition_;

    std::atomic<bool> is_running_;
    std::thread runner_;
    std::shared_ptr<vsomeip::application> app_;
};

#endif // DEBOUNCE_TEST_SERVICE_HPP_
