// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include "../someip_test_utils.hpp"

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

}
