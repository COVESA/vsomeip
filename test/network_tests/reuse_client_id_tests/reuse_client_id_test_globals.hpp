// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#define NUM_CLIENTS 5

namespace reuse_client_id {

/**
 * Interprocess synchronization data.
 */
struct reuse_client_id_test_interprocess_sync {
    enum registration_status { STATE_REGISTERED = 0x00, STATE_NOT_REGISTERED = 0x01, STATE_DEREGISTERED = 0x02 };

    boost::interprocess::interprocess_mutex client_mutex_;
    boost::interprocess::interprocess_condition client_cv_;

    std::array<registration_status, NUM_CLIENTS> client_status_ = {
            registration_status::STATE_NOT_REGISTERED, registration_status::STATE_NOT_REGISTERED, registration_status::STATE_NOT_REGISTERED,
            registration_status::STATE_NOT_REGISTERED, registration_status::STATE_NOT_REGISTERED};

    std::array<bool, NUM_CLIENTS> stop_clients_ = {0, 0, 0, 0, 0};
    std::array<bool, NUM_CLIENTS> restart_clients_ = {0, 0, 0, 0, 0};
};

/**
 * Wrapper interprocess synchronization functions.
 */
struct reuse_client_id_test_interprocess_utils {
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
    static bool wait_and_check_unlocked(boost::interprocess::interprocess_condition& cv,
                                        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock, int timeout_s,
                                        T& value_to_check, T expected_value) {
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::local_time() + boost::posix_time::seconds(timeout_s);

        cv.timed_wait(its_lock, timeout, [&] { return value_to_check == expected_value; });

        return value_to_check == expected_value;
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
