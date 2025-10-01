// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <array>

// Number of service consumers to be created
#define NUM_SERVICE_CONSUMERS 4

namespace restart_routing {

/**
 * Interprocess synchronization data.
 */
struct restart_routing_test_interprocess_sync {
    enum registration_status { STATE_REGISTERED = 0x00, STATE_NOT_REGISTERED = 0x01, STATE_DEREGISTERED = 0x02 };
    enum sending_status { SEND_MESSAGES = 0x00, WAITING_TO_SEND_MESSAGES = 0x01 };

    boost::interprocess::interprocess_mutex client_mutex_;
    boost::interprocess::interprocess_condition client_cv_;

    std::array<registration_status, NUM_SERVICE_CONSUMERS> client_status_ = {
            registration_status::STATE_NOT_REGISTERED, registration_status::STATE_NOT_REGISTERED, registration_status::STATE_NOT_REGISTERED,
            registration_status::STATE_NOT_REGISTERED};
    sending_status sending_status_ = {sending_status::WAITING_TO_SEND_MESSAGES};
};

/**
 * Wrapper interprocess synchronization functions.
 */
struct restart_routing_test_interprocess_utils {
    /**
     * @brief Waits on @param cv until either time @param timeout_s has expire or @param value_to_check matches to @param expected_value.
     *
     * @tparam T Specifies the type of @param value_to_check and @param expected_value
     * @param cv Condition variable.
     * @param its_lock Locked being hold.
     * @param timeout_s Maximum wait time.
     * @param value_to_check Part of predicate, value to be changed by notifier.
     * @param expected_value Part of predicate, expected value to be set by notifier to positive predicate evaluation.
     */
    template<typename T>
    static void wait_and_check_unlocked(boost::interprocess::interprocess_condition& cv,
                                        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock, int timeout_s,
                                        T& value_to_check, T expected_value) {
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::local_time() + boost::posix_time::seconds(timeout_s);

        cv.timed_wait(its_lock, timeout, [&] { return value_to_check == expected_value; });

        ASSERT_EQ(value_to_check, expected_value);
    }

    /**
     * @brief Performs a notify one on the parameterized condition variable.
     *
     * @param cv Condition variable to be notified.
     */
    static void notify_component_unlocked(boost::interprocess::interprocess_condition& cv) { cv.notify_one(); }

    /**
     * @brief Performs a notify all on the parameterized condition variable.
     *
     * @param cv Condition variable to be notified.
     */
    static void notify_all_component_unlocked(boost::interprocess::interprocess_condition& cv) { cv.notify_all(); }
};

}
