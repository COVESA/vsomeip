// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef VSOMEIP_V3_TESTING_TEST_LOGGING_HPP_
#define VSOMEIP_V3_TESTING_TEST_LOGGING_HPP_

#include <vsomeip/internal/logger.hpp>

#define TEST_LOG   vsomeip_v3::logger::message(vsomeip_v3::logger::level_e::LL_DEBUG) << "[TEST_LOG] "

#endif
