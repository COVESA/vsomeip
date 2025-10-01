// Copyright (C) 2014-2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef AVAILABILITY_HANDLER_TEST_GLOBALS_HPP_
#define AVAILABILITY_HANDLER_TEST_GLOBALS_HPP_

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace availability_handler {

vsomeip::service_t Service_ID = 0x1111;
vsomeip::instance_t Instance_ID = 0x1;

struct availability_handler_test_steps {
    enum registration_status { STATE_REGISTERED = 0x00, STATE_NOT_REGISTERED = 0x01, STATE_DEREGISTERED = 0x02 };
    enum offering_status { OFFERS_SENT = 0x00, OFFERS_NOT_SENT = 0x01, STOP_OFFERS_SENT = 0x02, STOP_OFFERS_NOT_SENT = 0x03 };
    enum reregistering_status { REREGISTER_DONE = 0x00, REREGISTER_NOT_DONE = 0x01 };
    enum requesting_status { REQUESTS_SENT = 0x00, REQUESTS_NOT_SENT = 0x01 };
    enum releasing_status { SERVICES_RELEASED = 0x00, SERVICES_NOT_RELEASED = 0x01 };
    enum availability_received_status { AVAILABILITY_RECEIVED = 0x00, UNAVAILABILITY_RECEIVED = 0x01, AVAILABILITY_NOT_RECEIVED = 0x02 };

    boost::interprocess::interprocess_mutex service_mutex_;
    boost::interprocess::interprocess_mutex client_mutex_;
    boost::interprocess::interprocess_condition service_cv_;
    boost::interprocess::interprocess_condition client_cv_;

    registration_status service_status_ = STATE_NOT_REGISTERED;
    registration_status client_status_ = STATE_NOT_REGISTERED;
    offering_status offer_status_ = OFFERS_NOT_SENT;
    requesting_status request_status_ = REQUESTS_NOT_SENT;
    releasing_status release_status_ = SERVICES_NOT_RELEASED;
    availability_received_status availability_status_ = AVAILABILITY_NOT_RECEIVED;
    reregistering_status reregister_status_ = REREGISTER_NOT_DONE;
};

class availability_handler_utils {
public:
    // To be used on test_manager side
    template<typename T>
    static void wait_and_check_unlocked(boost::interprocess::interprocess_condition& cv,
                                        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock, int timeout_s,
                                        T& value_to_check, T expected_value) {
        boost::posix_time::ptime timeout = boost::posix_time::second_clock::universal_time() + boost::posix_time::seconds(timeout_s);

        cv.timed_wait(its_lock, timeout, [&] { return value_to_check == expected_value; });

        ASSERT_EQ(value_to_check, expected_value);
    }

    static void notify_component_unlocked(boost::interprocess::interprocess_condition& cv) { cv.notify_one(); }

    // To be used on components side
    static void notify_and_wait_unlocked(boost::interprocess::interprocess_condition& cv,
                                         boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex>& its_lock) {

        cv.notify_one();

        cv.wait(its_lock);
    }
};

}

#endif /* AVAILABILITY_HANDLER_TEST_GLOBALS_HPP_ */
