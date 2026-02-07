// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <array>
#include "../someip_test_utils.hpp"

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

}
