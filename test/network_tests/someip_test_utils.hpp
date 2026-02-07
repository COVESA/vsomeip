// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace vsomeip_test {

struct interprocess_utils {
    /**
     * @brief Waits on @param cv until either time @param timeout_s has expire or @param value_to_check matches to @param expected_value.
     *
     * @tparam T Specifies the type of @param value_to_check and @param expected_value
     * @param cv Condition variable.
     * @param its_lock Lock being held.
     * @param timeout_s Maximum wait time.
     * @param value_to_check Part of predicate, value to be changed by notifier.
     * @param expected_value Part of predicate, expected value to be set by notifier to positive predicate evaluation.
     */
    template<typename T>
    static void assert_wait_and_check_unlocked(boost::interprocess::interprocess_condition& cv,
                                               boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock,
                                               int timeout_s, T& value_to_check, T expected_value) {
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(timeout_s);

        cv.timed_wait(its_lock, timeout, [&] { return value_to_check == expected_value; });

        ASSERT_EQ(value_to_check, expected_value);
    }

    /**
     * @brief Waits on @param cv until either time @param timeout_s has expire or @param value_to_check matches to @param expected_value.
     *
     * @tparam T Specifies the type of @param value_to_check and @param expected_value
     * @param cv Condition variable.
     * @param its_lock Lock being held.
     * @param timeout_s Maximum wait time.
     * @param value_to_check Part of predicate, value to be changed by notifier.
     * @param expected_value Part of predicate, expected value to be set by notifier to positive predicate evaluation.
     */
    template<typename T>
    static bool wait_and_check_unlocked(boost::interprocess::interprocess_condition& cv,
                                        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock, int timeout_s,
                                        T& value_to_check, T expected_value) {
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(timeout_s);

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

    /**
     * To be used on components side
     *
     * @brief Performs a notify one on the parameterized condition variable and waits to be notified again.
     *
     * @param cv Condition variable to be notified.
     * @param its_lock Lock being held.
     */
    static void notify_and_wait_unlocked(boost::interprocess::interprocess_condition& cv,
                                         boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock) {

        cv.notify_one();

        cv.wait(its_lock);
    }
};

}
