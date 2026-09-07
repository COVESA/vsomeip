// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>

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
    static bool wait_and_check_unlocked(boost::interprocess::interprocess_condition& cv,
                                        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock, int timeout_s,
                                        T& value_to_check, T expected_value) {
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(timeout_s);

        cv.timed_wait(its_lock, timeout, [&] { return value_to_check == expected_value; });

        return value_to_check == expected_value;
    }

    /**
     * @brief Releases the component that is blocked in notify_and_wait_unlocked.
     *
     * The acknowledgement counter, and not the notification itself, is what the component waits
     * for: a bare notification is lost when the component has not reached its wait yet, and it
     * cannot be told apart from a notification another waiter on the same cv is interested in.
     *
     * @param cv Condition variable to be notified.
     * @param ack Handshake counter shared with the component.
     */
    static void notify_component_unlocked(boost::interprocess::interprocess_condition& cv, std::uint32_t& ack) {
        ++ack;
        cv.notify_all();
    }

    /**
     * @brief Performs a notify all on the parameterized condition variable.
     *
     * @param cv Condition variable to be notified.
     */
    static void notify_all_component_unlocked(boost::interprocess::interprocess_condition& cv) { cv.notify_all(); }

    /**
     * To be used on components side
     *
     * @brief Publishes the state the component has just reached and blocks until the manager
     *        acknowledges it by advancing @param ack.
     *
     * @param cv Condition variable to be notified.
     * @param its_lock Lock being held.
     * @param ack Handshake counter shared with the manager.
     */
    static void notify_and_wait_unlocked(boost::interprocess::interprocess_condition& cv,
                                         boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock,
                                         std::uint32_t& ack) {

        const std::uint32_t its_ack = ack;

        cv.notify_all();

        cv.wait(its_lock, [&] { return ack != its_ack; });
    }
};

}
