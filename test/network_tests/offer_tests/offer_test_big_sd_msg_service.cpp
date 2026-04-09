// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <algorithm>
#include <atomic>
#include <algorithm>

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "offer_test_globals.hpp"
#include "../someip_test_globals.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "common/test_main.hpp"

class offer_test_big_sd_msg_service {
public:
    offer_test_big_sd_msg_service(struct offer_test::service_info _service_info) :
        service_info_(_service_info),
        // service with number 1 uses "routingmanagerd" as application name
        // this way the same json file can be reused for all local tests
        // including the ones with routingmanagerd
        app_(vsomeip::runtime::get()->create_application("offer_test_big_sd_msg_service")) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }

        app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, service_info_.shutdown_method_id,
                                       std::bind(&offer_test_big_sd_msg_service::on_shutdown_method_called, this, std::placeholders::_1));

        for (std::uint16_t s = 1; s <= offer_test::big_msg_number_services; s++) {
            app_->offer_event(s, 0x1, offer_test::big_msg_event_id, {offer_test::big_msg_eventgroup_id}, vsomeip::event_type_e::ET_EVENT,
                              std::chrono::milliseconds::zero(), false, true, nullptr, vsomeip::reliability_type_e::RT_UNKNOWN);
            app_->offer_service(s, 0x1, 0x1, 0x1);
        }

        app_->start();
    }

    void on_shutdown_method_called(const std::shared_ptr<vsomeip::message>& _message) {
        app_->send(vsomeip::runtime::get()->create_response(_message));

        // magic sleep to give time for the last message to be sent and processed by the client before STOP_OFFER is sent, otherwise the
        // client may drop the message and the test will fail
        // TODO: FIXME! REMOVE THIS!
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        app_->stop();
    }

private:
    struct offer_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;
};

TEST(someip_offer_test_big_sd_msg, notify_increasing_counter) {
    offer_test_big_sd_msg_service its_sample(offer_test::service);
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif
