// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include "../../../implementation/endpoints/include/auxiliary_context.hpp"

using namespace vsomeip_v3;

TEST(test_auxiliary_context, start_and_stop) {
    /// basic sanity check

    auxiliary_context context(0);
    EXPECT_NO_THROW(context.start());
    EXPECT_NO_THROW(context.stop());
}

TEST(test_auxiliary_context, use_context) {
    /// whether we can use the underlying context while it's running

    auxiliary_context context(0);
    EXPECT_NO_THROW(context.start());

    std::promise<void> event_promise;
    boost::asio::post(context.get_context(), [&event_promise]() { event_promise.set_value(); });

    // wait for the event to be executed
    EXPECT_EQ(event_promise.get_future().wait_for(std::chrono::seconds(1)), std::future_status::ready);

    EXPECT_NO_THROW(context.stop());
}

TEST(test_auxiliary_context, use_context_after_restart) {
    /// whether we can use the underlying context after a restart

    auxiliary_context context(0);
    EXPECT_NO_THROW(context.start());
    EXPECT_NO_THROW(context.stop());
    EXPECT_NO_THROW(context.start());

    std::promise<void> event_promise;
    boost::asio::post(context.get_context(), [&event_promise]() { event_promise.set_value(); });

    // wait for the event to be executed
    EXPECT_EQ(event_promise.get_future().wait_for(std::chrono::seconds(1)), std::future_status::ready);

    EXPECT_NO_THROW(context.stop());
}
