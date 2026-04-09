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

#include <gtest/gtest.h>

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>

#include "offer_test_globals.hpp"
#include "../someip_test_globals.hpp"
#include "../implementation/utility/include/utility.hpp"
#include <common/vsomeip_app_utilities.hpp>
#include "common/test_main.hpp"

class offer_test_big_sd_msg_client {
public:
    offer_test_big_sd_msg_client(struct offer_test::service_info _service_info) :
        service_info_(_service_info), app_(vsomeip::runtime::get()->create_application("offer_test_big_sd_msg_client")),
        wait_until_subscribed_(true), wait_for_stop_(true),
        shutdown_thread_(std::bind(&offer_test_big_sd_msg_client::send_shutdown, this)) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
                                       std::bind(&offer_test_big_sd_msg_client::on_response, this, std::placeholders::_1));

        // register availability for all other services and request their event.
        app_->register_availability_handler(vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE,
                                            std::bind(&offer_test_big_sd_msg_client::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3),
                                            0x1, 0x1);
        std::set<vsomeip::eventgroup_t> its_eventgroups;
        its_eventgroups.insert(offer_test::big_msg_eventgroup_id);
        for (std::uint16_t s = 1; s <= offer_test::big_msg_number_services; s++) {
            app_->request_service(s, 0x1, 0x1, 0x1);
            app_->request_event(s, 0x1, offer_test::big_msg_event_id, its_eventgroups, vsomeip::event_type_e::ET_EVENT,
                                vsomeip::reliability_type_e::RT_UNKNOWN);
            app_->subscribe(s, 0x1, offer_test::big_msg_eventgroup_id, 0x1, offer_test::big_msg_event_id);
            services_available_subscribed_[s] = std::make_pair(false, 0);
            app_->register_subscription_status_handler(s, 0x1, offer_test::big_msg_eventgroup_id, offer_test::big_msg_event_id,
                                                       std::bind(&offer_test_big_sd_msg_client::subscription_status_changed, this,
                                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                                                 std::placeholders::_4, std::placeholders::_5));
        }
        app_->start();
    }

    ~offer_test_big_sd_msg_client() { shutdown_thread_.join(); }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        VSOMEIP_DEBUG << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << _instance << "] is "
                      << (_is_available ? "available" : "not available") << ".";

        std::scoped_lock its_lock(mutex_);
        if (_is_available) {
            auto found_service = services_available_subscribed_.find(_service);
            if (found_service != services_available_subscribed_.end()) {
                found_service->second.first = true;
                if (std::all_of(services_available_subscribed_.cbegin(), services_available_subscribed_.cend(),
                                [](const services_available_subscribed_t::value_type& v) { return v.second.first; })) {
                    VSOMEIP_WARNING << "************************************************************";
                    VSOMEIP_WARNING << "All services available!";
                    VSOMEIP_WARNING << "************************************************************";
                }
            }
        }
    }

    void subscription_status_changed(const vsomeip::service_t _service, const vsomeip::instance_t _instance, const vsomeip::eventgroup_t,
                                     const vsomeip::event_t, const uint16_t _error) {
        VSOMEIP_DEBUG << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << _instance << "] has sent "
                      << (!_error ? "subscribe ack" : "subscribe nack") << ".";
        if (_error == 0x0 /*OK*/) {
            std::scoped_lock its_lock(mutex_);
            auto found_service = services_available_subscribed_.find(_service);
            if (found_service != services_available_subscribed_.end()) {
                found_service->second.second++;
                ASSERT_EQ(found_service->second.second, 1)
                        << "Registered subscription status handler was called more than once for service:"
                        << vsomeip_v3::hex4(found_service->first);
                if (std::all_of(services_available_subscribed_.cbegin(), services_available_subscribed_.cend(),
                                [](const services_available_subscribed_t::value_type& v) { return v.second.second == 1; })) {
                    VSOMEIP_WARNING << "************************************************************";
                    VSOMEIP_WARNING << "All subscription were acknowledged!";
                    VSOMEIP_WARNING << "************************************************************";
                    wait_until_subscribed_ = false;
                    condition_.notify_all();
                }
            }
        }
    };

    void on_response(const std::shared_ptr<vsomeip::message>&) {
        std::unique_lock its_lock(mutex_);
        wait_for_stop_ = false;
        condition_.notify_one();
    }

    void send_shutdown() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        condition_.wait(its_lock, [this] { return !wait_until_subscribed_; });

        std::shared_ptr<vsomeip::message> its_req = vsomeip::runtime::get()->create_request();
        its_req->set_service(1);
        its_req->set_instance(1);
        its_req->set_interface_version(0x1);
        its_req->set_method(service_info_.shutdown_method_id);
        app_->send(its_req);

        condition_.wait(its_lock, [this] { return !wait_for_stop_; });
        app_->stop();
    }

private:
    struct offer_test::service_info service_info_;
    std::shared_ptr<vsomeip::application> app_;

    bool wait_until_subscribed_;
    bool wait_for_stop_;
    std::mutex mutex_;
    std::condition_variable condition_;

    typedef std::map<vsomeip::service_t, std::pair<bool, std::uint32_t>> services_available_subscribed_t;
    services_available_subscribed_t services_available_subscribed_;
    std::thread shutdown_thread_;
};

TEST(someip_offer_test_big_sd_msg, subscribe_or_call_method_at_service) {
    offer_test_big_sd_msg_client its_sample(offer_test::service);
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    return test_main(argc, argv);
}
#endif
