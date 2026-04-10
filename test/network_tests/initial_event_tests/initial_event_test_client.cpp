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
#include <mutex>
#include <gtest/gtest.h>

#if defined(__linux__)
#include <signal.h>
#endif

#include <vsomeip/vsomeip.hpp>
#include <vsomeip/internal/logger.hpp>
#include "common/test_main.hpp"

#include "initial_event_test_globals.hpp"

class initial_event_test_client {
public:
    initial_event_test_client(int _client_number, std::array<initial_event_test::service_info, 7> _service_infos,
                              bool _subscribe_on_available, std::uint32_t _events_to_subscribe, bool _dont_exit, bool _subscribe_only_one,
                              vsomeip::reliability_type_e _reliability_type, bool _client_subscribes_twice) :
        client_number_(_client_number), service_infos_(_service_infos), app_(vsomeip::runtime::get()->create_application()),
        wait_for_stop_(true), second_subscribe_started_(false), second_subscribe_completed_(false), all_services_are_available_(false),
        subscribed_(false), subscribe_on_available_(_subscribe_on_available), events_to_subscribe_(_events_to_subscribe),
        dont_exit_(_dont_exit), subscribe_only_one_(_subscribe_only_one), reliability_type_(_reliability_type),
        client_subscribes_twice_(_client_subscribes_twice) {
        if (!app_->init()) {
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }

        app_->register_state_handler(std::bind(&initial_event_test_client::on_state, this, std::placeholders::_1));

        app_->register_message_handler(vsomeip::ANY_SERVICE, vsomeip::ANY_INSTANCE, vsomeip::ANY_METHOD,
                                       std::bind(&initial_event_test_client::on_message, this, std::placeholders::_1));

        // register availability for all other services and request their event.
        for (const auto& i : service_infos_) {
            if (i.service_id == 0xFFFF && i.instance_id == 0xFFFF) {
                continue;
            }
            app_->register_availability_handler(i.service_id, i.instance_id,
                                                std::bind(&initial_event_test_client::on_availability, this, std::placeholders::_1,
                                                          std::placeholders::_2, std::placeholders::_3));
            app_->request_service(i.service_id, i.instance_id);

            std::set<vsomeip::eventgroup_t> its_eventgroups;
            its_eventgroups.insert(i.eventgroup_id);
            for (std::uint32_t j = 0; j < events_to_subscribe_; j++) {
                app_->request_event(i.service_id, i.instance_id, static_cast<vsomeip::event_t>(i.event_id + j), its_eventgroups,
                                    vsomeip::event_type_e::ET_FIELD, reliability_type_);
            }

            other_services_available_[std::make_pair(i.service_id, i.instance_id)] = false;

            if (!subscribe_on_available_) {
                if (events_to_subscribe_ == 1) {
                    app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR);

                    other_services_received_notification_[std::make_pair(i.service_id, i.event_id)] = 0;
                } else if (events_to_subscribe_ > 1) {
                    if (!subscribe_only_one_) {
                        for (std::uint32_t j = 0; j < events_to_subscribe_; j++) {
                            app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR,
                                            static_cast<vsomeip::event_t>(i.event_id + j));
                            other_services_received_notification_[std::make_pair(i.service_id, i.event_id + j)] = 0;
                        }
                    } else {
                        app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR,
                                        static_cast<vsomeip::event_t>(i.event_id));
                        other_services_received_notification_[std::make_pair(i.service_id, i.event_id)] = 0;
                    }
                }
            } else {
                for (std::uint32_t j = 0; j < events_to_subscribe_; j++) {
                    other_services_received_notification_[std::make_pair(i.service_id, i.event_id + j)] = 0;
                }
            }
        }

        stop_thread_ = std::thread(&initial_event_test_client::wait_for_stop, this);

        // start thread which handles all of the signals
        signal_thread_ = std::thread(&initial_event_test_client::wait_for_signal, this);

        app_->start();
    }

    ~initial_event_test_client() {
        if (stop_thread_.joinable()) {
            stop_thread_.join();
        }

        if (signal_thread_.joinable()) {
            // in case `signal_thread_` did not receive a signal yet
            VSOMEIP_INFO << "Waiting for signal thread";
            pthread_kill(signal_thread_.native_handle(), SIGTERM);
            signal_thread_.join();
        }
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        if (!_is_available) {
            return;
        }

        bool its_service_became_available(false);
        bool all_services_became_available(false);
        bool should_subscribe(false);
        {
            std::scoped_lock its_lock(state_mutex_);
            auto its_service = other_services_available_.find(std::make_pair(_service, _instance));
            if (its_service != other_services_available_.end()) {
                if (!its_service->second) {
                    its_service->second = true;
                    its_service_became_available = true;
                }
            }

            if (!all_services_are_available_
                && std::all_of(
                        other_services_available_.cbegin(), other_services_available_.cend(),
                        [](const std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool>::value_type& v) { return v.second; })) {
                all_services_are_available_ = true;
                all_services_became_available = true;
                if (subscribe_on_available_ && !subscribed_) {
                    should_subscribe = true;
                    subscribed_ = true;
                }
            }
        }

        if (its_service_became_available) {
            VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << client_number_ << "] Service [" << std::setw(4)
                          << _service << "." << _instance << "] is available.";
        }

        if (all_services_became_available) {
            VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << client_number_ << "] all services are available.";
            if (should_subscribe) {
                for (const auto& i : service_infos_) {
                    if (i.service_id == 0xFFFF && i.instance_id == 0xFFFF) {
                        continue;
                    }
                    if (events_to_subscribe_ == 1) {
                        app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR);
                    } else if (events_to_subscribe_ > 1) {
                        for (std::uint32_t j = 0; j < events_to_subscribe_; j++) {
                            app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR,
                                            static_cast<vsomeip::event_t>(i.event_id + j));
                        }
                    }
                }
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message>& _message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_NOTIFICATION) {
            std::uint32_t its_notification_count(0);

            {
                std::scoped_lock its_lock(state_mutex_);
                auto& its_counter = other_services_received_notification_[std::make_pair(_message->get_service(), _message->get_method())];
                its_counter++;
                its_notification_count = its_counter;
            }

            VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << client_number_ << "] "
                          << "Received a notification with Client/Session [" << std::setw(4) << _message->get_client() << "/"
                          << std::setw(4) << _message->get_session() << "] from Service/Method [" << std::setw(4) << _message->get_service()
                          << "/" << std::setw(4) << _message->get_method() << "] (now have: " << std::dec << its_notification_count << ")";

            std::shared_ptr<vsomeip::payload> its_payload(_message->get_payload());
            EXPECT_EQ(2u, its_payload->get_length());
            EXPECT_EQ((_message->get_service() & 0xFF00) >> 8, its_payload->get_data()[0]);
            EXPECT_EQ((_message->get_service() & 0xFF), its_payload->get_data()[1]);
            bool should_subscribe_again(false);
            bool should_stop(false);
            if (client_subscribes_twice_) {
                // only relevant for testcase:
                // initial_event_test_diff_client_ids_same_ports_udp_client_subscribes_twice
                // check that a second subscribe triggers another initial event
                // expect notifications_to_send_after_double_subscribe == 2;
                {
                    std::scoped_lock its_lock(state_mutex_);
                    if (!second_subscribe_started_) {
                        second_subscribe_started_ = true;
                        should_subscribe_again = true;
                    } else if (second_subscribe_completed_
                               && all_notifications_received_locked(initial_event_test::notifications_to_send * 2)) {
                        should_stop = true;
                    }
                }

                if (should_subscribe_again) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    for (const auto& i : service_infos_) {
                        // subscribe again and expect initial events cached at rm::proxy to be
                        // received as configured routing manager only fires the event once after
                        // first susbcribe.
                        if (i.service_id == 0xFFFF && i.instance_id == 0xFFFF) {
                            continue;
                        }
                        if (!subscribe_on_available_) {
                            if (events_to_subscribe_ == 1) {
                                app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR);
                            } else if (events_to_subscribe_ > 1) {
                                if (!subscribe_only_one_) {
                                    for (std::uint32_t j = 0; j < events_to_subscribe_; j++) {
                                        app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR,
                                                        static_cast<vsomeip::event_t>(i.event_id + j));
                                    }
                                } else {
                                    app_->subscribe(i.service_id, i.instance_id, i.eventgroup_id, vsomeip::DEFAULT_MAJOR,
                                                    static_cast<vsomeip::event_t>(i.event_id));
                                }
                            }
                        }
                    }
                    std::scoped_lock its_lock(state_mutex_);
                    second_subscribe_completed_ = true;
                    if (all_notifications_received_locked(initial_event_test::notifications_to_send * 2)) {
                        should_stop = true;
                    }
                }
            } else {
                std::scoped_lock its_lock(state_mutex_);
                if (all_notifications_received_locked(initial_event_test::notifications_to_send)) {
                    should_stop = true;
                }
            }

            if (should_stop && !dont_exit_) {
                std::scoped_lock its_lock(state_mutex_);
                wait_for_stop_ = false;
                stop_condition_.notify_one();
            }
        }
    }

    bool all_notifications_received_locked(std::uint32_t _required_notifications) {
        return std::all_of(other_services_received_notification_.cbegin(), other_services_received_notification_.cend(),
                           [&](const std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t>::value_type& v) {
                               bool result;
                               // NOTE: can receive multiple initial notifications; see ff0d2ae2
                               if (v.second >= _required_notifications) {
                                   result = true;
                               } else {
                                   VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << client_number_ << "] "
                                                << "Still missing initial events from service/instance: " << std::setw(4) << v.first.first
                                                << "." << std::setw(4) << v.first.second << " number of received events: " << v.second;
                                   result = false;
                               }

                               return result;
                           });
    }

    void wait_for_signal() {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);
        sigaddset(&set, SIGTERM);
        sigaddset(&set, SIGINT);

        // Wait until a new signal is received.
        for (;;) {
            auto signal = 0;
            auto result = sigwait(&set, &signal);
            if (result == 0) {
                if (signal == SIGUSR1 || signal == SIGINT || signal == SIGTERM) {
                    VSOMEIP_INFO << "Received signal " << signal;
                    break;
                }
            }
        }

        {
            std::scoped_lock its_lock(state_mutex_);
            wait_for_stop_ = false;
        }
        stop_condition_.notify_one();
    }

    void wait_for_stop() {
        static int its_call_number(0);
        its_call_number++;

        {
            std::unique_lock<std::mutex> its_lock(state_mutex_);
            stop_condition_.wait(its_lock, [this] { return !wait_for_stop_; });
        }
        VSOMEIP_ERROR << "(" << std::dec << its_call_number << ") [" << std::hex << std::setfill('0') << std::setw(4) << client_number_
                      << "] Received notifications from all services, going down";
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

private:
    int client_number_;
    std::array<initial_event_test::service_info, 7> service_infos_;
    std::shared_ptr<vsomeip::application> app_;
    std::mutex state_mutex_;
    std::map<std::pair<vsomeip::service_t, vsomeip::instance_t>, bool> other_services_available_;
    std::map<std::pair<vsomeip::service_t, vsomeip::method_t>, std::uint32_t> other_services_received_notification_;

    bool wait_for_stop_;
    bool second_subscribe_started_;
    bool second_subscribe_completed_;
    bool all_services_are_available_;
    bool subscribed_;

    bool subscribe_on_available_;
    std::uint32_t events_to_subscribe_;
    bool dont_exit_;
    bool subscribe_only_one_;

    std::condition_variable stop_condition_;
    std::thread stop_thread_;
    std::thread signal_thread_;
    vsomeip::reliability_type_e reliability_type_;
    bool client_subscribes_twice_;
};

static int client_number;
static bool use_same_service_id;
static bool subscribe_on_available;
static std::uint32_t subscribe_multiple_events;
static bool dont_exit;
static bool subscribe_only_one;
static bool client_subscribes_twice;

vsomeip::reliability_type_e reliability_type = vsomeip::reliability_type_e::RT_UNKNOWN;

TEST(someip_initial_event_test, wait_for_initial_events_of_all_services) {
    if (use_same_service_id) {
        initial_event_test_client its_sample(client_number, initial_event_test::service_infos_same_service_id, subscribe_on_available,
                                             subscribe_multiple_events, dont_exit, subscribe_only_one, reliability_type,
                                             client_subscribes_twice);
    } else {
        initial_event_test_client its_sample(client_number, initial_event_test::service_infos, subscribe_on_available,
                                             subscribe_multiple_events, dont_exit, subscribe_only_one, reliability_type,
                                             client_subscribes_twice);
    }
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    // block signals as soon as possible; `sigwait` is used later
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    if (argc < 2) {
        std::cerr << "Please specify a client number, like: " << argv[0] << " 2 SUBSCRIBE_BEFORE_START SAME_SERVICE_ID" << std::endl;
        std::cerr << "Valid client numbers are from 0 to 0xFFFF" << std::endl;
        std::cerr << "After client number one/multiple of these flags can be specified:";
        std::cerr << " - Time of subscription, valid values: [SUBSCRIBE_ON_AVAILABILITY, "
                     "SUBSCRIBE_BEFORE_START], default SUBSCRIBE_BEFORE_START"
                  << std::endl;
        std::cerr << " - SAME_SERVICE_ID flag. If set the test is run w/ multiple instances of the "
                     "same service, default false"
                  << std::endl;
        std::cerr << " - MULTIPLE_EVENTS flag. If set the test will subscribe to multiple events "
                     "in the eventgroup, default false"
                  << std::endl;
        std::cerr << " - DONT_EXIT flag. If set the test will not exit if all notifications have "
                     "been received. Default false"
                  << std::endl;
        std::cerr << " - SUBSCRIBE_ONLY_ONE flag. If set the test will only subscribe to one event "
                     "even if MULTIPLE_EVENTS is set. Default false"
                  << std::endl;
        return 1;
    }

    client_number = std::stoi(std::string(argv[1]), nullptr);

    subscribe_on_available = false;
    use_same_service_id = false;
    subscribe_multiple_events = 1;
    dont_exit = false;
    subscribe_only_one = false;
    client_subscribes_twice = false;
    if (argc > 2) {
        for (int i = 2; i < argc; i++) {
            if (std::string("SUBSCRIBE_ON_AVAILABILITY") == std::string(argv[i])) {
                subscribe_on_available = true;
                std::cout << "Subscribing on availability" << std::endl;
            } else if (std::string("SUBSCRIBE_BEFORE_START") == std::string(argv[i])) {
                subscribe_on_available = false;
                std::cout << "Subscribing on start" << std::endl;
            } else if (std::string("SAME_SERVICE_ID") == std::string(argv[i])) {
                use_same_service_id = true;
                std::cout << "Using same service ID" << std::endl;
            } else if (std::string("MULTIPLE_EVENTS") == std::string(argv[i])) {
                subscribe_multiple_events = 5;
                std::cout << "Subscribing to multiple events" << std::endl;
            } else if (std::string("DONT_EXIT") == std::string(argv[i])) {
                dont_exit = true;
                std::cout << "Waiting for all notifications to be received" << std::endl;
            } else if (std::string("SUBSCRIBE_ONLY_ONE") == std::string(argv[i])) {
                subscribe_only_one = true;
                std::cout << "Subscribing to only one event" << std::endl;
            } else if (std::string("TCP") == std::string(argv[i])) {
                reliability_type = vsomeip::reliability_type_e::RT_RELIABLE;
                std::cout << "Using reliability type RT_RELIABLE" << std::endl;
            } else if (std::string("UDP") == std::string(argv[i])) {
                reliability_type = vsomeip::reliability_type_e::RT_UNRELIABLE;
                std::cout << "Using reliability type RT_UNRELIABLE" << std::endl;
            } else if (std::string("TCP_AND_UDP") == std::string(argv[i])) {
                reliability_type = vsomeip::reliability_type_e::RT_BOTH;
                std::cout << "Using reliability type RT_BOTH" << std::endl;
            } else if (std::string("CLIENT_SUBSCRIBES_TWICE") == std::string(argv[i])) {
                client_subscribes_twice = true;
                std::cout << "Testing for initial event after a second subscribe from same client "
                             "CLIENT_SUBSCRIBES_TWICE"
                          << std::endl;
            } else {
                std::cerr << "Unknown argument: " << std::string(argv[i]) << std::endl;
                return EXIT_FAILURE;
            }
        }
    }

    return test_main(argc, argv);
}
#endif
