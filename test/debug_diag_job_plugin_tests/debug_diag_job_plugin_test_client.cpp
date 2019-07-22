// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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
#include <cstring>

#include <gtest/gtest.h>

#ifndef _WIN32
#include <signal.h>
#endif

#include <vsomeip/vsomeip.hpp>
#include "../../implementation/logging/include/logger.hpp"

#include "../debug_diag_job_plugin_tests/debug_diag_job_plugin_test_globals.hpp"
class debug_diag_job_plugin_test_client;
static debug_diag_job_plugin_test_client* the_client;
extern "C" void signal_handler(int _signum);


enum class debug_diag_job_app_error_type_e : std::uint32_t {
    ET_UNKNOWN                         = 0,
    ET_OK                              = 256,
    ET_OUT_OF_RANGE                    = 257,
    ET_E_COMMUNICATION_ERROR           = 576,
};

class debug_diag_job_plugin_test_client {
public:
    debug_diag_job_plugin_test_client(
                              std::array<debug_diag_job_plugin_test::service_info, 3> _service_infos_local,
                              std::array<debug_diag_job_plugin_test::service_info, 3> _service_infos_remote) :
            service_infos_local_(_service_infos_local),
            service_infos_remote_(_service_infos_remote),
            app_(vsomeip::runtime::get()->create_application()),
            wait_until_registered_(true),
            wait_until_remote_services_available_(true),
            wait_until_debug_diag_job_service_available_(true),
            wait_until_local_services_available_(true),
            wait_until_local_services_unavailable_(true),
            wait_until_notifications_received_(true),
            wait_until_responses_received_(true),
            wait_until_debug_diag_job_response_received_(true),
            wait_for_stop_(true),
            last_response_(debug_diag_job_app_error_type_e::ET_UNKNOWN),
            stop_thread_(std::bind(&debug_diag_job_plugin_test_client::wait_for_stop, this)),
            run_thread_(std::bind(&debug_diag_job_plugin_test_client::run, this)) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }

        // register signal handler
        the_client = this;
        struct sigaction sa_new, sa_old;
        sa_new.sa_handler = signal_handler;
        sa_new.sa_flags = 0;
        sigemptyset(&sa_new.sa_mask);
        ::sigaction(SIGUSR1, &sa_new, &sa_old);
        ::sigaction(SIGINT, &sa_new, &sa_old);
        ::sigaction(SIGTERM, &sa_new, &sa_old);
        ::sigaction(SIGABRT, &sa_new, &sa_old);

        app_->register_state_handler(
                std::bind(&debug_diag_job_plugin_test_client::on_state, this,
                        std::placeholders::_1));

        app_->register_message_handler(vsomeip::ANY_SERVICE,
                vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
                std::bind(&debug_diag_job_plugin_test_client::on_message, this,
                        std::placeholders::_1));

        for (const auto& serviceinfos : {service_infos_remote_, service_infos_local_}) {
            for (const auto& serviceinfo : serviceinfos) {
                if (serviceinfo.service_id == 0xFFFF && serviceinfo.instance_id == 0xFFFF) {
                    continue;
                }
                service_infos_.push_back(serviceinfo);
            }
        }

        // register availability for all other services and request their event.
        for (const auto& i : service_infos_) {
            app_->register_availability_handler(i.service_id, i.instance_id,
                    std::bind(&debug_diag_job_plugin_test_client::on_availability, this,
                            std::placeholders::_1, std::placeholders::_2,
                            std::placeholders::_3));
            app_->request_service(i.service_id, i.instance_id);

            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(i.eventgroup_id);
            app_->request_event(i.service_id, i.instance_id,
                    static_cast<vsomeip::event_t>(i.event_id),
                    its_eventgroups, true);

            other_services_available_[std::make_pair(i.service_id, i.instance_id)] = false;

            app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id);
            other_services_received_notification_[std::make_pair(i.service_id, i.event_id)] = 0;
            other_services_received_response_[std::make_pair(i.service_id, i.instance_id)] = 0;
        }

        for (const auto& si : service_infos_local_) {
            if (si.service_id == 0xFFFF && si.instance_id == 0xFFFF) {
                continue;
            }
            other_local_services_available_[std::make_pair(si.service_id, si.instance_id)] = false;
            other_local_services_received_notification_[std::make_pair(si.service_id, si.event_id)] = 0;
        }

        for (const auto& si : service_infos_remote_) {
            if (si.service_id == 0xFFFF && si.instance_id == 0xFFFF) {
                continue;
            }
            other_remote_services_available_[std::make_pair(si.service_id, si.instance_id)] = false;
            other_remote_services_received_notification_[std::make_pair(si.service_id, si.event_id)] = 0;
        }
        app_->register_availability_handler(
                debug_diag_job_plugin_test::debug_diag_job_serviceinfo.service_id,
                debug_diag_job_plugin_test::debug_diag_job_serviceinfo.instance_id,
                std::bind(&debug_diag_job_plugin_test_client::on_availability, this,
                        std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3));
        app_->request_service(
                debug_diag_job_plugin_test::debug_diag_job_serviceinfo.service_id,
                debug_diag_job_plugin_test::debug_diag_job_serviceinfo.instance_id,
                0x1, vsomeip::ANY_MINOR);

        app_->start();
    }

    ~debug_diag_job_plugin_test_client() {
        run_thread_.join();
        stop_thread_.join();

    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
        << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                "registered." : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_registered_ = false;
            condition_.notify_one();
        }
    }

    void on_availability(vsomeip::service_t _service,
                         vsomeip::instance_t _instance, bool _is_available) {
        if(_is_available) {
            for (auto available_map : {&other_services_available_,
                                       &other_local_services_available_,
                                       &other_remote_services_available_}) {
                auto its_service = available_map->find(std::make_pair(_service, _instance));
                if (its_service != available_map->end()) {
                    if (its_service->second != _is_available) {
                        its_service->second = true;
                        VSOMEIP_DEBUG << "Service ["
                        << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                        << "] is available.";
                    }
                }
            }

            if (std::all_of(other_remote_services_available_.cbegin(),
                           other_remote_services_available_.cend(),
                           [](const std::map<std::pair<vsomeip::service_t,
                                   vsomeip::instance_t>, bool>::value_type& v) {
                                return v.second;})) {
                VSOMEIP_INFO << " all remote services are available.";
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_remote_services_available_ = false;
                condition_.notify_one();
            }

            if (_service == debug_diag_job_plugin_test::debug_diag_job_serviceinfo.service_id &&
                    _instance == debug_diag_job_plugin_test::debug_diag_job_serviceinfo.instance_id) {
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_debug_diag_job_service_available_ = false;
                condition_.notify_one();
            }

            if (std::all_of(other_local_services_available_.cbegin(),
                            other_local_services_available_.cend(),
                           [](const std::map<std::pair<vsomeip::service_t,
                                   vsomeip::instance_t>, bool>::value_type& v) {
                                return v.second;})) {
                VSOMEIP_INFO << "all local services are available.";
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_local_services_available_ = false;
                condition_.notify_one();
            }

            if (std::all_of(other_services_available_.cbegin(),
                            other_services_available_.cend(),
                           [](const std::map<std::pair<vsomeip::service_t,
                                   vsomeip::instance_t>, bool>::value_type& v) {
                                return v.second;})) {
                VSOMEIP_INFO << "all local and remote services are available.";
            }
        } else {
            bool was_available_before(false);
            auto its_service = other_local_services_available_.find(std::make_pair(_service, _instance));
            if (its_service != other_local_services_available_.end()) {
                if (its_service->second) {
                    its_service->second = false;
                    VSOMEIP_DEBUG << "Service ["
                    << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                    << "] is not available anymore.";
                    was_available_before = true;
                }
            }
            if (was_available_before) {
                if (std::all_of(other_local_services_available_.cbegin(),
                                other_local_services_available_.cend(),
                               [](const std::map<std::pair<vsomeip::service_t,
                                       vsomeip::instance_t>, bool>::value_type& v) {
                                    return !v.second;})) {
                    VSOMEIP_INFO << "all local services are not available anymore.";
                    std::lock_guard<std::mutex> its_lock(mutex_);
                    wait_until_local_services_unavailable_ = false;
                    condition_.notify_one();
                }
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &_message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {

            other_services_received_notification_[std::make_pair(_message->get_service(),
                                                             _message->get_method())]++;

            VSOMEIP_DEBUG
            << "Received a notification with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _message->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_session() << "] from Service/Method ["
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_service() << "/" << std::setw(4) << std::setfill('0')
            << std::hex << _message->get_method() <<"] (now have: "
            << std::dec << other_services_received_notification_[std::make_pair(_message->get_service(),
                                                                    _message->get_method())] << ")";

            std::shared_ptr<vsomeip::payload> its_payload(_message->get_payload());
            EXPECT_EQ(2u, its_payload->get_length());
            EXPECT_EQ((_message->get_service() & 0xFF00 ) >> 8, its_payload->get_data()[0]);
            EXPECT_EQ((_message->get_service() & 0xFF), its_payload->get_data()[1]);
            bool notify = all_notifications_received();

            if(notify) {
                for (auto &os : other_local_services_received_notification_) {
                    os.second = 0;
                }
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_notifications_received_ = false;
                condition_.notify_one();
            }
        } else if (_message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE &&
                _message->get_service() != debug_diag_job_plugin_test::debug_diag_job_serviceinfo.service_id) {
            other_services_received_response_[std::make_pair(_message->get_service(),
                                                             _message->get_instance())]++;
            VSOMEIP_INFO
            << "Received a response with Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _message->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_session() << "] from Service/Method ["
            << std::setw(4) << std::setfill('0') << std::hex
            << _message->get_service() << "/" << std::setw(4) << std::setfill('0')
            << std::hex << _message->get_method() <<"] (now have: "
            << std::dec << other_services_received_response_[std::make_pair(_message->get_service(),
                                                                    _message->get_instance())] << ")";
            if (std::all_of(other_services_received_response_.begin(),
                            other_services_received_response_.end(),
                            [&](const std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, std::uint32_t>::value_type& v) {
                return v.second > 0;
            })) {
                VSOMEIP_INFO << "received responses of all services!";
                std::lock_guard<std::mutex> its_lock(mutex_);
                wait_until_responses_received_ = false;
                condition_.notify_one();
            }
        } else if (_message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE &&
                _message->get_service() == debug_diag_job_plugin_test::debug_diag_job_serviceinfo.service_id) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            wait_until_debug_diag_job_response_received_ = false;
            last_response_ = debug_diag_job_app_error_type_e::ET_UNKNOWN;
            auto its_payload = _message->get_payload();
            if (its_payload && its_payload->get_length() > 3) {

                last_response_ = static_cast<debug_diag_job_app_error_type_e>(
                        (its_payload->get_data()[0] << 24) |
                        (its_payload->get_data()[1] << 16) |
                        (its_payload->get_data()[2] << 8) |
                        (its_payload->get_data()[3]));
            }
            condition_.notify_one();
        }
    }

    bool all_notifications_received() {
        return std::all_of(
                other_services_received_notification_.cbegin(),
                other_services_received_notification_.cend(),
                [&](const std::map<std::pair<vsomeip::service_t,
                        vsomeip::method_t>, std::uint32_t>::value_type& v)
                {
                    if (v.second == debug_diag_job_plugin_test::notifications_to_send) {
                        return true;
                    } else {
                        if (v.second >= debug_diag_job_plugin_test::notifications_to_send) {
                            VSOMEIP_WARNING
                                    << " Received multiple initial events from service/instance: "
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.first
                                    << "."
                                    << std::setw(4) << std::setfill('0') << std::hex << v.first.second
                                    << " number of received events: " << v.second
                                    << ". This is caused by StopSubscribe/Subscribe messages and/or"
                                    << " service offered via UDP and TCP";
                            return false;
                        } else {
                            return false;
                        }
                    }
                }
        );
    }

    void handle_signal(int _signum) {
        VSOMEIP_DEBUG << "Catched signal, going down ("
                << std::dec <<_signum << ")";
        std::lock_guard<std::mutex> its_lock(stop_mutex_);
        wait_for_stop_ = false;
        stop_condition_.notify_one();
    }

    void wait_for_stop() {
        {
            std::unique_lock<std::mutex> its_lock(stop_mutex_);
            while (wait_for_stop_) {
                stop_condition_.wait(its_lock);
            }
            VSOMEIP_INFO << "going down";
        }
        for (const auto& i : service_infos_) {
            if (i.service_id == 0xFFFF && i.instance_id == 0xFFFF) {
                continue;
            }
            app_->unsubscribe(i.service_id, i.instance_id, i.eventgroup_id);
            app_->release_event(i.service_id, i.instance_id, i.event_id);
            app_->release_service(i.service_id, i.instance_id);
        }
        app_->clear_all_handler();
        app_->stop();
    }

    void call_debug_diag_job(bool _offer) {
        std::shared_ptr<vsomeip::message> its_request = vsomeip::runtime::get()->create_request();
        its_request->set_service(debug_diag_job_plugin_test::debug_diag_job_serviceinfo.service_id);
        its_request->set_instance(debug_diag_job_plugin_test::debug_diag_job_serviceinfo.instance_id);
        if (_offer) {
            its_request->set_method(debug_diag_job_plugin_test::debug_diag_job_serviceinfo.method_id);
            std::vector<vsomeip::byte_t> its_payload;
            for (int i =0; i<8; i++) { // handletype and size placeholder
                its_payload.push_back(0x0);
            }
            // insert valid entries
            for (const auto& si : service_infos_local_) {
                if (si.service_id == 0xFFFF && si.instance_id == 0xFFFF) {
                    continue;
                }
                for (auto reliable : {false, true}) { // ensure to offer service reliable and unreliable
                    // service
                    its_payload.push_back(static_cast<vsomeip::byte_t>(si.service_id >> 8));
                    its_payload.push_back(static_cast<vsomeip::byte_t>(si.service_id & 0xFF));
                    // instance
                    its_payload.push_back(static_cast<vsomeip::byte_t>(si.instance_id >> 8));
                    its_payload.push_back(static_cast<vsomeip::byte_t>(si.instance_id & 0xFF));
                    // port (use event id as port)
                    its_payload.push_back(static_cast<vsomeip::byte_t>(si.event_id >> 8));
                    its_payload.push_back(static_cast<vsomeip::byte_t>(si.event_id & 0xFF));
                    // reliable
                    its_payload.push_back(reliable);
                    // magic_cookies_enabled
                    its_payload.push_back(reliable);
                }
            }

            std::size_t its_size = its_payload.size() - 8;
            its_payload[4] = vsomeip::byte_t(its_size >> 24 & 0xFF);
            its_payload[5] = vsomeip::byte_t(its_size >> 16 & 0xFF);
            its_payload[6] = vsomeip::byte_t(its_size >>  8 & 0xFF);
            its_payload[7] = vsomeip::byte_t(its_size       & 0xFF);

            its_request->set_payload(vsomeip::runtime::get()->create_payload(its_payload));
        } else {
            its_request->set_method(debug_diag_job_plugin_test::debug_diag_job_serviceinfo_reset.method_id);
        }
        std::unique_lock<std::mutex> its_lock(mutex_);
        app_->send(its_request);
        while(wait_until_debug_diag_job_response_received_) {
            condition_.wait(its_lock);
        }
        EXPECT_EQ(debug_diag_job_app_error_type_e::ET_OK, last_response_);
        wait_until_debug_diag_job_response_received_ = true;

    }

    void call_debug_diag_job_wrong_message(bool _offer) {
        std::shared_ptr<vsomeip::message> its_request = vsomeip::runtime::get()->create_request();
        its_request->set_service(debug_diag_job_plugin_test::debug_diag_job_serviceinfo.service_id);
        its_request->set_instance(debug_diag_job_plugin_test::debug_diag_job_serviceinfo.instance_id);
        if (_offer) {
            its_request->set_method(debug_diag_job_plugin_test::debug_diag_job_serviceinfo.method_id);
        } else {
            its_request->set_method(debug_diag_job_plugin_test::debug_diag_job_serviceinfo_reset.method_id);
        }
        std::vector<vsomeip::byte_t> its_payload;
        for (int i =0; i<8; i++) { // handletype and size placeholder
            its_payload.push_back(0x0);
        }
        // insert valid entries
        for (const auto& si : service_infos_local_) {
            if (si.service_id == 0xFFFF && si.instance_id == 0xFFFF) {
                continue;
            }
            for (auto reliable : {false, true}) { // ensure to offer service reliable and unreliable
                // service
                its_payload.push_back(static_cast<vsomeip::byte_t>(si.service_id >> 8));
                its_payload.push_back(static_cast<vsomeip::byte_t>(si.service_id & 0xFF));
                // instance
                its_payload.push_back(static_cast<vsomeip::byte_t>(si.instance_id >> 8));
                its_payload.push_back(static_cast<vsomeip::byte_t>(si.instance_id & 0xFF));
                // port (use event id as port)
                its_payload.push_back(static_cast<vsomeip::byte_t>(si.event_id >> 8));
                its_payload.push_back(static_cast<vsomeip::byte_t>(si.event_id & 0xFF));
                // reliable
                its_payload.push_back(reliable);
                // magic_cookies_enabled
                its_payload.push_back(reliable);
            }
        }

        // insert invalid entry
        // service
        its_payload.push_back(static_cast<vsomeip::byte_t>(0xAAAA >> 8));
        its_payload.push_back(static_cast<vsomeip::byte_t>(0xAAAA & 0xFF));
        // instance
        its_payload.push_back(static_cast<vsomeip::byte_t>(0xBBBB >> 8));
        its_payload.push_back(static_cast<vsomeip::byte_t>(0xBBBB & 0xFF));
        // port
        its_payload.push_back(static_cast<vsomeip::byte_t>(0xCCCC >> 8));
        its_payload.push_back(static_cast<vsomeip::byte_t>(0xCCCC & 0xFF));
        // reliable
        its_payload.push_back(0x1);
        // magic_cookies_enabled
        its_payload.push_back(0x1);

        for (int var = 0; var < 12; ++var) { // insert invalid data covered by size
            its_payload.push_back(0xdd);
        }

        std::size_t its_size = its_payload.size() - 8;
        its_payload[4] = vsomeip::byte_t(its_size >> 24 & 0xFF);
        its_payload[5] = vsomeip::byte_t(its_size >> 16 & 0xFF);
        its_payload[6] = vsomeip::byte_t(its_size >>  8 & 0xFF);
        its_payload[7] = vsomeip::byte_t(its_size       & 0xFF);
        its_request->set_payload(vsomeip::runtime::get()->create_payload(its_payload));

        auto send_request_and_wait_for_reply = [&](
                const std::shared_ptr<vsomeip::message>& _request,
                debug_diag_job_app_error_type_e _expected_response) {
            std::unique_lock<std::mutex> its_lock(mutex_);
            app_->send(_request);
            while(wait_until_debug_diag_job_response_received_) {
                condition_.wait(its_lock);
            }
            EXPECT_EQ(_expected_response, last_response_);
            wait_until_debug_diag_job_response_received_ = true;
        };

        send_request_and_wait_for_reply(its_request, debug_diag_job_app_error_type_e::ET_OUT_OF_RANGE);

        // send a request w/o payload
        its_payload.clear();
        its_request->set_payload(vsomeip::runtime::get()->create_payload(its_payload));
        send_request_and_wait_for_reply(its_request, debug_diag_job_app_error_type_e::ET_OUT_OF_RANGE);



        // send a request with to few data and a too big size field
        its_payload.clear();
        its_payload.push_back(0x0); // handle
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x11); // size
        its_payload.push_back(0x22);
        its_payload.push_back(0x33);
        its_payload.push_back(0x44);
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_request->set_payload(vsomeip::runtime::get()->create_payload(its_payload));
        send_request_and_wait_for_reply(its_request, debug_diag_job_app_error_type_e::ET_OUT_OF_RANGE);


        // send a request with to few data and a size of zero field
        its_payload.clear();
        its_payload.push_back(0x0); // handle
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x0); // size
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_request->set_payload(vsomeip::runtime::get()->create_payload(its_payload));
        send_request_and_wait_for_reply(its_request, debug_diag_job_app_error_type_e::ET_OUT_OF_RANGE);

        // send a request with valid size and data but additional data at the end
        its_payload.clear();
        its_payload.push_back(0x0); // handle
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x0); // size
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0x0);
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data
        its_payload.push_back(0xDD); // data

        its_size = its_payload.size() - 8;
        its_payload[4] = vsomeip::byte_t(its_size >> 24 & 0xFF);
        its_payload[5] = vsomeip::byte_t(its_size >> 16 & 0xFF);
        its_payload[6] = vsomeip::byte_t(its_size >>  8 & 0xFF);
        its_payload[7] = vsomeip::byte_t(its_size       & 0xFF);

        its_payload.push_back(0xDD); // data
        its_request->set_payload(vsomeip::runtime::get()->create_payload(its_payload));
        send_request_and_wait_for_reply(its_request, debug_diag_job_app_error_type_e::ET_OUT_OF_RANGE);
    }

    void call_shutdown_method() {
        std::shared_ptr<vsomeip::message> its_request = vsomeip::runtime::get()->create_request();

        auto send_request = [&](const debug_diag_job_plugin_test::service_info& _info) {
            if (_info.service_id == 0xFFFF && _info.instance_id == 0xFFFF) {
                return;
            }
            its_request->set_service(_info.service_id);
            its_request->set_instance(_info.instance_id);
            its_request->set_method(_info.method_id);
            app_->send(its_request);
        };
        // insert valid entries
        for (const auto& si : service_infos_local_) {
            send_request(si);
        }
        for (const auto& si : service_infos_remote_) {
            send_request(si);
        }
    }

    void run() {
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_registered_) {
                condition_.wait(its_lock);
            }
        }
        // wait until remote services are available
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_remote_services_available_) {
                condition_.wait(its_lock);
            }
            wait_until_remote_services_available_ = true;
            VSOMEIP_WARNING << "Remote services available";
        }
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_debug_diag_job_service_available_) {
                condition_.wait(its_lock);
            }
            wait_until_debug_diag_job_service_available_ = true;
            VSOMEIP_WARNING << "debug diag job service available";
        }
        // trigger offering of local services
        call_debug_diag_job(true);
        call_debug_diag_job(true);
        VSOMEIP_WARNING << "Calling debug_diag_job_service (offer)";
        // wait until local services are available as well
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_local_services_available_) {
                condition_.wait(its_lock);
            }
            wait_until_local_services_available_ = true;
            VSOMEIP_WARNING << "local services available";
        }

        // check that from all services events were received
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_notifications_received_) {
                condition_.wait(its_lock);
            }
            wait_until_notifications_received_ = true;
            VSOMEIP_WARNING << "notifications received";
        }

        // tigger stop offering of local services
        call_debug_diag_job(false);
        VSOMEIP_WARNING << "Calling debug_diag_job_service (stop offer)";
        // wait until local services are unavailable
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_local_services_unavailable_) {
                condition_.wait(its_lock);
            }
            wait_until_local_services_unavailable_ = true;
            VSOMEIP_WARNING << "local services unavailable";
        }

        // trigger offering of local services
        VSOMEIP_WARNING << "Calling debug_diag_job_service (offer)";
        call_debug_diag_job_wrong_message(true);
        // wait until local services are available as well again
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_local_services_available_) {
                condition_.wait(its_lock);
            }
            wait_until_local_services_available_ = true;
            VSOMEIP_WARNING << "local services available";
        }
        VSOMEIP_WARNING << "Calling shutdown method";
        call_shutdown_method();
        {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (wait_until_responses_received_) {
                if (std::cv_status::timeout  ==
                        condition_.wait_for(its_lock, std::chrono::seconds(10))) {
                    ADD_FAILURE() << "Didn't receive shutdown responses within time";
                    wait_until_responses_received_ = false;
                } else {
                    VSOMEIP_WARNING << "received shutdown method responses";
                }
            }
            wait_until_responses_received_ = true;
        }
        {
            std::lock_guard<std::mutex> its_lock(stop_mutex_);
            wait_for_stop_ = false;
            stop_condition_.notify_one();
        }
    }

private:
    std::array<debug_diag_job_plugin_test::service_info, 3> service_infos_local_;
    std::array<debug_diag_job_plugin_test::service_info, 3> service_infos_remote_;
    std::vector<debug_diag_job_plugin_test::service_info> service_infos_;
    std::shared_ptr<vsomeip::application> app_;
    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_services_available_;
    std::map<std::pair<vsomeip::service_t, vsomeip::event_t>, std::uint32_t> other_services_received_notification_;

    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_remote_services_available_;
    std::map<std::pair<vsomeip::service_t, vsomeip::event_t>, std::uint32_t> other_remote_services_received_notification_;

    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_local_services_available_;
    std::map<std::pair<vsomeip::service_t, vsomeip::event_t>, std::uint32_t> other_local_services_received_notification_;

    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, std::uint32_t> other_services_received_response_;

    bool wait_until_registered_;
    bool wait_until_remote_services_available_;
    bool wait_until_debug_diag_job_service_available_;
    bool wait_until_local_services_available_;
    bool wait_until_local_services_unavailable_;
    bool wait_until_notifications_received_;
    bool wait_until_responses_received_;
    bool wait_until_debug_diag_job_response_received_;
    std::mutex mutex_;
    std::condition_variable condition_;

    bool wait_for_stop_;

    debug_diag_job_app_error_type_e last_response_;
    std::mutex stop_mutex_;
    std::condition_variable stop_condition_;
    std::thread stop_thread_;

    std::thread run_thread_;
};

extern "C" void signal_handler(int signum) {
    the_client->handle_signal(signum);
}

TEST(someip_debug_diag_job_plugin_test, remotely_enable_offering_of_local_services)
{
        debug_diag_job_plugin_test_client its_sample(
                debug_diag_job_plugin_test::service_infos_local,
                debug_diag_job_plugin_test::service_infos_remote);
}

#ifndef _WIN32
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
