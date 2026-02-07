// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <array>
#include "../someip_test_utils.hpp"

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

}
