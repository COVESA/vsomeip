// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <vsomeip/internal/logger.hpp>
#include <vsomeip/vsomeip.hpp>

#include "common/test_main.hpp"
#include "initial_event_test_globals.hpp"

class initial_event_test_stop_service {
public:
    static constexpr auto peer_notification_retry_interval_ = std::chrono::milliseconds(100);

    initial_event_test_stop_service(initial_event_test::service_info _service_info, bool _is_master,
                                    std::array<initial_event_test::service_info, 7> _service_infos,
                                    vsomeip::reliability_type_e _reliability_type, std::uint16_t _events_to_subscribe,
                                    std::vector<pid_t> _local_service_pids) :
        service_info_(_service_info), is_master_(_is_master), service_infos_(_service_infos), reliability_type_(_reliability_type),
        events_to_subscribe_(_events_to_subscribe), local_service_pids_(std::move(_local_service_pids)),
        app_(vsomeip::runtime::get()->create_application()), app_initialized_(false), wait_until_registered_(true),
        peer_stop_service_available_(false), peer_ready_received_(false), local_services_stopped_(false), failed_(false) {

        if (!app_->init()) {
            failed_ = true;
            ADD_FAILURE() << "Couldn't initialize application";
            return;
        }
        app_initialized_ = true;

        app_->register_state_handler(std::bind(&initial_event_test_stop_service::on_state, this, std::placeholders::_1));

        app_->register_message_handler(service_info_.service_id, service_info_.instance_id, service_info_.method_id,
                                       std::bind(&initial_event_test_stop_service::on_peer_ready_message, this, std::placeholders::_1));

        const auto peer_stop_service = get_peer_stop_service();
        app_->register_message_handler(peer_stop_service.service_id, peer_stop_service.instance_id, peer_stop_service.method_id,
                                       std::bind(&initial_event_test_stop_service::on_peer_ready_message, this, std::placeholders::_1));

        register_service_watchers();
        register_peer_stop_service_watcher();
    }

    ~initial_event_test_stop_service() {
        stop_application();

        if (app_thread_.joinable()) {
            app_thread_.join();
        }
    }

    void start() {
        if (failed_) {
            return;
        }
        app_thread_ = std::thread([this] { app_->start(); });
    }

    void on_state(vsomeip::state_type_e _state) {
        VSOMEIP_INFO << "Application " << app_->get_name() << " is "
                     << (_state == vsomeip::state_type_e::ST_REGISTERED ? "registered." : "deregistered.");

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            std::scoped_lock its_lock(state_mutex_);
            wait_until_registered_ = false;
            state_condition_.notify_all();
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        bool tracked_service(false);
        bool tracked_peer(false);
        {
            std::scoped_lock its_lock(state_mutex_);
            const auto key = make_service_key(_service, _instance);
            auto its_service = services_available_.find(key);
            if (its_service != services_available_.end()) {
                its_service->second = _is_available;
                if (_is_available) {
                    services_were_available_[key] = true;
                }
                tracked_service = true;
            }

            if (is_peer_stop_service(_service, _instance)) {
                peer_stop_service_available_ = _is_available;
                tracked_peer = true;
            }

            if (tracked_service || tracked_peer) {
                state_condition_.notify_all();
            }
        }

        if (tracked_service || tracked_peer) {
            VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Service ["
                         << std::setw(4) << _service << "." << std::setw(4) << _instance << "] is "
                         << (_is_available ? "available." : "not available.");
        }
    }

    void on_subscription_status(vsomeip::service_t _service, vsomeip::instance_t _instance, vsomeip::eventgroup_t _eventgroup,
                                vsomeip::event_t _event, std::uint16_t _error_code) {
        EXPECT_EQ(0x0u, _error_code) << "Subscription to [" << std::hex << std::setfill('0') << std::setw(4) << _service << "."
                                     << std::setw(4) << _instance << "." << std::setw(4) << _eventgroup << "." << std::setw(4) << _event
                                     << "] failed";

        std::scoped_lock its_lock(state_mutex_);
        if (_error_code == 0x0u) {
            auto its_subscription = subscriptions_acknowledged_.find(make_service_key(_service, _instance));
            if (its_subscription != subscriptions_acknowledged_.end()) {
                its_subscription->second = true;
            }
        } else {
            failed_ = true;
        }
        state_condition_.notify_all();
    }

    void on_peer_ready_message(const std::shared_ptr<vsomeip::message>& _message) {
        if (_message->get_message_type() == vsomeip::message_type_e::MT_REQUEST) {
            VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                         << "] Peer stop service is ready";
            app_->send(vsomeip::runtime::get()->create_response(_message));

            std::scoped_lock its_lock(state_mutex_);
            peer_ready_received_ = true;
            state_condition_.notify_all();
        } else if (_message->get_message_type() == vsomeip::message_type_e::MT_RESPONSE) {
            VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                         << "] Peer stop service acknowledged readiness";
        }
    }

    void run() {
        {
            std::unique_lock<std::mutex> its_lock(state_mutex_);
            state_condition_.wait(its_lock, [this] { return !wait_until_registered_ || failed_; });
        }

        if (failed_) {
            return;
        }

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id << "] Offering";
        app_->offer_service(service_info_.service_id, service_info_.instance_id);

        wait_for_local_subscriptions();
        if (failed_) {
            return;
        }

        notify_peer_until_ready();
        if (failed_) {
            return;
        }

        stop_local_non_host_services();

        wait_for_remote_non_host_services_to_stop();
    }

private:
    using service_key_t = std::pair<vsomeip::service_t, vsomeip::instance_t>;

    static service_key_t make_service_key(vsomeip::service_t _service, vsomeip::instance_t _instance) {
        return std::make_pair(_service, _instance);
    }

    void register_service_watchers() {
        for (std::size_t i = 1; i < service_infos_.size(); ++i) {
            const auto& service = service_infos_[i];
            const auto key = make_service_key(service.service_id, service.instance_id);

            services_available_[key] = false;
            services_were_available_[key] = false;
            subscriptions_acknowledged_[key] = false;

            app_->register_availability_handler(service.service_id, service.instance_id,
                                                std::bind(&initial_event_test_stop_service::on_availability, this, std::placeholders::_1,
                                                          std::placeholders::_2, std::placeholders::_3));
            app_->request_service(service.service_id, service.instance_id);

            std::set<vsomeip::eventgroup_t> eventgroups;
            eventgroups.insert(service.eventgroup_id);
            for (std::uint16_t event_index = 0; event_index < events_to_subscribe_; ++event_index) {
                app_->request_event(service.service_id, service.instance_id, static_cast<vsomeip::event_t>(service.event_id + event_index),
                                    eventgroups, vsomeip::event_type_e::ET_FIELD, reliability_type_);
            }

            app_->register_subscription_status_handler(service.service_id, service.instance_id, service.eventgroup_id, vsomeip::ANY_EVENT,
                                                       std::bind(&initial_event_test_stop_service::on_subscription_status, this,
                                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                                                 std::placeholders::_4, std::placeholders::_5),
                                                       true);
            app_->subscribe(service.service_id, service.instance_id, service.eventgroup_id, vsomeip::DEFAULT_MAJOR);
        }
    }

    void register_peer_stop_service_watcher() {
        const auto peer_stop_service = get_peer_stop_service();
        app_->register_availability_handler(peer_stop_service.service_id, peer_stop_service.instance_id,
                                            std::bind(&initial_event_test_stop_service::on_availability, this, std::placeholders::_1,
                                                      std::placeholders::_2, std::placeholders::_3));
        app_->request_service(peer_stop_service.service_id, peer_stop_service.instance_id);
    }

    void wait_for_local_subscriptions() {
        std::unique_lock<std::mutex> its_lock(state_mutex_);
        state_condition_.wait(its_lock, [this] { return failed_ || (all_subscriptions_acknowledged() && peer_stop_service_available_); });

        if (!failed_) {
            VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                         << "] All service subscriptions are acknowledged";
        }
    }

    void notify_peer_until_ready() {
        for (;;) {
            {
                std::unique_lock<std::mutex> its_lock(state_mutex_);
                if (failed_ || peer_ready_received_ || expected_remote_services_unavailable()) {
                    return;
                }
            }

            send_peer_ready_notification();

            std::unique_lock<std::mutex> its_lock(state_mutex_);
            if (state_condition_.wait_for(its_lock, peer_notification_retry_interval_,
                                          [this] { return failed_ || peer_ready_received_ || expected_remote_services_unavailable(); })) {
                return;
            }
        }
    }

    void send_peer_ready_notification() {
        const auto peer_stop_service = get_peer_stop_service();
        auto message = vsomeip::runtime::get()->create_request();
        message->set_service(peer_stop_service.service_id);
        message->set_instance(peer_stop_service.instance_id);
        message->set_method(peer_stop_service.method_id);

        VSOMEIP_DEBUG << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                      << "] Notifying peer stop service";
        app_->send(message);
    }

    void stop_local_non_host_services() {
        {
            std::scoped_lock its_lock(state_mutex_);
            if (local_services_stopped_) {
                return;
            }
        }

        VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                     << "] Stopping local non-host services";

        for (const auto pid : local_service_pids_) {
            const auto result = ::kill(pid, SIGTERM);
            if (result != 0) {
                const auto saved_errno = errno;
                if (saved_errno != ESRCH) {
                    ADD_FAILURE() << "Failed to stop local service pid " << pid << ": " << std::strerror(saved_errno);
                    std::scoped_lock its_lock(state_mutex_);
                    failed_ = true;
                    state_condition_.notify_all();
                }
            }
        }

        std::scoped_lock its_lock(state_mutex_);
        local_services_stopped_ = true;
        state_condition_.notify_all();
    }

    void wait_for_remote_non_host_services_to_stop() {
        std::unique_lock<std::mutex> its_lock(state_mutex_);
        state_condition_.wait(its_lock, [this] { return failed_ || expected_remote_services_unavailable(); });

        if (!failed_) {
            VSOMEIP_INFO << "[" << std::hex << std::setfill('0') << std::setw(4) << service_info_.service_id
                         << "] Remote non-host services are unavailable";
        }
    }

    bool all_subscriptions_acknowledged() const {
        return std::all_of(subscriptions_acknowledged_.cbegin(), subscriptions_acknowledged_.cend(),
                           [](const auto& subscription) { return subscription.second; });
    }

    bool expected_remote_services_unavailable() const {
        const auto expected_remote_services = expected_remote_services_to_stop();
        return std::all_of(expected_remote_services.cbegin(), expected_remote_services.cend(), [this](const auto key) {
            const auto service = services_available_.find(key);
            const auto was_available = services_were_available_.find(key);
            return service != services_available_.end() && !service->second && was_available != services_were_available_.end()
                    && was_available->second;
        });
    }

    std::array<service_key_t, 2> expected_remote_services_to_stop() const {
        if (is_master_) {
            return {make_service_key(service_infos_[5].service_id, service_infos_[5].instance_id),
                    make_service_key(service_infos_[6].service_id, service_infos_[6].instance_id)};
        }

        return {make_service_key(service_infos_[2].service_id, service_infos_[2].instance_id),
                make_service_key(service_infos_[3].service_id, service_infos_[3].instance_id)};
    }

    initial_event_test::service_info get_peer_stop_service() const {
        return is_master_ ? initial_event_test::stop_service_slave : initial_event_test::stop_service_master;
    }

    bool is_peer_stop_service(vsomeip::service_t _service, vsomeip::instance_t _instance) const {
        const auto peer_stop_service = get_peer_stop_service();
        return _service == peer_stop_service.service_id && _instance == peer_stop_service.instance_id;
    }

    void release_subscriptions() {
        for (std::size_t i = 1; i < service_infos_.size(); ++i) {
            const auto& service = service_infos_[i];
            app_->unsubscribe(service.service_id, service.instance_id, service.eventgroup_id);
            for (std::uint16_t event_index = 0; event_index < events_to_subscribe_; ++event_index) {
                app_->release_event(service.service_id, service.instance_id, static_cast<vsomeip::event_t>(service.event_id + event_index));
            }
            app_->release_service(service.service_id, service.instance_id);
        }

        const auto peer_stop_service = get_peer_stop_service();
        app_->release_service(peer_stop_service.service_id, peer_stop_service.instance_id);
    }

    void stop_application() {
        if (!app_initialized_) {
            return;
        }

        release_subscriptions();
        app_->stop_offer_service(service_info_.service_id, service_info_.instance_id);
        app_->clear_all_handler();
        app_->stop();
    }

    initial_event_test::service_info service_info_;
    bool is_master_;
    std::array<initial_event_test::service_info, 7> service_infos_;
    vsomeip::reliability_type_e reliability_type_;
    std::uint16_t events_to_subscribe_;
    std::vector<pid_t> local_service_pids_;
    std::shared_ptr<vsomeip::application> app_;
    bool app_initialized_;

    std::mutex state_mutex_;
    std::condition_variable state_condition_;
    bool wait_until_registered_;
    bool peer_stop_service_available_;
    bool peer_ready_received_;
    bool local_services_stopped_;
    bool failed_;

    std::map<service_key_t, bool> services_available_;
    std::map<service_key_t, bool> services_were_available_;
    std::map<service_key_t, bool> subscriptions_acknowledged_;
    std::once_flag stop_application_once_;
    std::thread app_thread_;
};

static bool is_master = false;
static bool use_same_service_id = false;
static std::uint16_t events_to_subscribe = 1;
static std::vector<pid_t> local_service_pids;
static vsomeip::reliability_type_e reliability_type = vsomeip::reliability_type_e::RT_UNKNOWN;

TEST(someip_initial_event_test, shutdown_services) {
    const auto service_infos = use_same_service_id ? initial_event_test::service_infos_same_service_id : initial_event_test::service_infos;
    if (is_master) {
        initial_event_test_stop_service its_sample(initial_event_test::stop_service_master, is_master, service_infos, reliability_type,
                                                   events_to_subscribe, local_service_pids);
        its_sample.start();
        its_sample.run();
    } else {
        initial_event_test_stop_service its_sample(initial_event_test::stop_service_slave, is_master, service_infos, reliability_type,
                                                   events_to_subscribe, local_service_pids);
        its_sample.start();
        its_sample.run();
    }
}

static bool is_number(const std::string& _value) {
    return !_value.empty() && std::all_of(_value.cbegin(), _value.cend(), [](const auto c) { return isdigit(c); });
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Please specify a valid type, like: " << argv[0] << " MASTER" << std::endl;
        std::cerr << "Valid types are in the range of [MASTER,SLAVE]" << std::endl;
        return EXIT_FAILURE;
    }

    if (std::string("MASTER") == std::string(argv[1])) {
        is_master = true;
    } else if (std::string("SLAVE") == std::string(argv[1])) {
        is_master = false;
    } else {
        std::cerr << "Unknown stop service role: " << std::string(argv[1]) << std::endl;
        return EXIT_FAILURE;
    }

    for (int i = 2; i < argc; ++i) {
        const std::string argument(argv[i]);
        if (argument == "SAME_SERVICE_ID") {
            use_same_service_id = true;
            std::cout << "Using same service ID" << std::endl;
        } else if (argument == "MULTIPLE_EVENTS") {
            events_to_subscribe = 5;
            std::cout << "Subscribing to multiple events" << std::endl;
        } else if (argument == "TCP") {
            reliability_type = vsomeip::reliability_type_e::RT_RELIABLE;
            std::cout << "Using reliability type RT_RELIABLE" << std::endl;
        } else if (argument == "UDP") {
            reliability_type = vsomeip::reliability_type_e::RT_UNRELIABLE;
            std::cout << "Using reliability type RT_UNRELIABLE" << std::endl;
        } else if (argument == "TCP_AND_UDP") {
            reliability_type = vsomeip::reliability_type_e::RT_BOTH;
            std::cout << "Using reliability type RT_BOTH" << std::endl;
        } else if (argument == "SUBSCRIBE_ON_AVAILABILITY" || argument == "SUBSCRIBE_BEFORE_START" || argument == "DONT_EXIT"
                   || argument == "SUBSCRIBE_ONLY_ONE" || argument == "CLIENT_SUBSCRIBES_TWICE") {
            std::cout << argument << " will be unused for stop service" << std::endl;
        } else if (is_number(argument)) {
            local_service_pids.push_back(static_cast<pid_t>(std::stol(argument)));
        } else {
            std::cerr << "Unknown argument: " << argument << std::endl;
            return EXIT_FAILURE;
        }
    }
    if (local_service_pids.empty()) {
        std::cout << "No local service PIDs were passed to the stop service";
        return 1;
    }

    return test_main(argc, argv, std::chrono::seconds(60));
}
#endif
