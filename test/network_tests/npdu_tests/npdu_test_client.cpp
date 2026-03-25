// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <iomanip>

#include "../npdu_tests/npdu_test_client.hpp"

#include <mutex>
#include <vsomeip/internal/logger.hpp>
#include "common/test_main.hpp"
#include "../../implementation/configuration/include/configuration.hpp"
#include "../../implementation/configuration/include/configuration_impl.hpp"
#include "../../implementation/configuration/include/configuration_plugin.hpp"
#include "../../implementation/plugin/include/plugin_manager_impl.hpp"

enum class payloadsize : std::uint8_t { UDS, TCP, UDP };

// this variables are changed via cmdline parameters
static bool use_tcp = false;

npdu_test_client::npdu_test_client(bool _use_tcp, std::array<std::array<std::chrono::milliseconds, 4>, 4> _applicative_debounce) :
    app_(vsomeip::runtime::get()->create_application()), request_(vsomeip::runtime::get()->create_request(_use_tcp)),
    blocked_({false, false, false, false}), is_available_({false, false, false, false}),
    number_of_messages_to_send_(vsomeip_test::NUMBER_OF_MESSAGES_TO_SEND),
    number_of_acknowledged_messages_{{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}, current_payload_size_({0, 0, 0, 0}),
    all_msg_acknowledged_(
            {{{false, false, false, false}, {false, false, false, false}, {false, false, false, false}, {false, false, false, false}}}),
    applicative_debounce_(_applicative_debounce), finished_waiter_(&npdu_test_client::wait_for_all_senders, this),
    shutdown_service_available_(false) {
    senders_[0] = std::thread(&npdu_test_client::run<0>, this);
    senders_[1] = std::thread(&npdu_test_client::run<1>, this);
    senders_[2] = std::thread(&npdu_test_client::run<2>, this);
    senders_[3] = std::thread(&npdu_test_client::run<3>, this);
}

npdu_test_client::~npdu_test_client() {
    finished_waiter_.join();
}

void npdu_test_client::init() {
    app_->init();

    app_->register_state_handler(std::bind(&npdu_test_client::on_state, this, std::placeholders::_1));

    register_availability_handler<0>();
    register_availability_handler<1>();
    register_availability_handler<2>();
    register_availability_handler<3>();

    register_message_handler_for_all_service_methods<0>();
    register_message_handler_for_all_service_methods<1>();
    register_message_handler_for_all_service_methods<2>();
    register_message_handler_for_all_service_methods<3>();

    app_->request_service(npdu_test::RMD_SERVICE_ID_CLIENT_SIDE, npdu_test::RMD_INSTANCE_ID);
    app_->register_availability_handler(npdu_test::RMD_SERVICE_ID_CLIENT_SIDE, npdu_test::RMD_INSTANCE_ID,
                                        std::bind(&npdu_test_client::on_shutdown_service_available, this, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3));

    request_->set_service(vsomeip_test::TEST_SERVICE_SERVICE_ID);
    request_->set_instance(vsomeip_test::TEST_SERVICE_INSTANCE_ID);
}

template<int service_idx>
void npdu_test_client::register_availability_handler() {
    app_->register_availability_handler(npdu_test::service_ids[service_idx], npdu_test::instance_ids[service_idx],
                                        std::bind(&npdu_test_client::on_availability<service_idx>, this, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3));
}

template<int service_idx>
void npdu_test_client::register_message_handler_for_all_service_methods() {
    register_message_handler<service_idx, 0>();
    register_message_handler<service_idx, 1>();
    register_message_handler<service_idx, 2>();
    register_message_handler<service_idx, 3>();
}

template<int service_idx, int method_idx>
void npdu_test_client::register_message_handler() {
    app_->register_message_handler(npdu_test::service_ids[service_idx], npdu_test::instance_ids[service_idx],
                                   npdu_test::method_ids[service_idx][method_idx],
                                   std::bind(&npdu_test_client::on_message<service_idx, method_idx>, this, std::placeholders::_1));
}

void npdu_test_client::start() {
    VSOMEIP_INFO << "Starting...";
    app_->start();
}

void npdu_test_client::stop() {
    VSOMEIP_INFO << "Stopping...";

    app_->unregister_state_handler();

    for (std::size_t i = 0; i < npdu_test::service_ids.size(); ++i) {
        app_->unregister_availability_handler(npdu_test::service_ids[i], npdu_test::instance_ids[i]);

        for (std::size_t j = 0; j < npdu_test::method_ids[i].size(); ++j) {
            app_->unregister_message_handler(npdu_test::service_ids[i], npdu_test::instance_ids[i], npdu_test::method_ids[i][j]);
        }
    }

    // notify the routing manager daemon that were finished
    request_->set_service(npdu_test::RMD_SERVICE_ID_CLIENT_SIDE);
    request_->set_instance(npdu_test::RMD_INSTANCE_ID);
    request_->set_method(npdu_test::RMD_SHUTDOWN_METHOD_ID);
    request_->set_payload(vsomeip::runtime::get()->create_payload());
    request_->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
    app_->send(request_);

    // magic sleep to give time for the last message to be read
    // in the router, before the clean-up starts the forceful stop
    // of the "server" connection within the router.
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    app_->stop();
}

void npdu_test_client::join_sender_thread() {
    for (auto& t : senders_) {
        t.join();
    }
}

void npdu_test_client::on_state(vsomeip::state_type_e _state) {
    if (_state == vsomeip::state_type_e::ST_REGISTERED) {
        for (std::size_t i = 0; i < npdu_test::service_ids.size(); ++i) {
            app_->request_service(npdu_test::service_ids[i], npdu_test::instance_ids[i]);
        }
    }
}

template<int service_idx>
void npdu_test_client::on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    VSOMEIP_INFO << "Service [" << std::hex << std::setfill('0') << std::setw(4) << _service << "." << std::setw(4) << _instance << "] is "
                 << (_is_available ? "available." : "NOT available.");
    if (npdu_test::service_ids[service_idx] == _service && npdu_test::instance_ids[service_idx] == _instance) {
        if (is_available_[service_idx] && !_is_available) {
            is_available_[service_idx] = false;
        } else if (_is_available && !is_available_[service_idx]) {
            is_available_[service_idx] = true;
            send<service_idx>();
        }
    }
}

void npdu_test_client::on_shutdown_service_available(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
    (void)_service;
    (void)_instance;
    if (_is_available) {
        std::unique_lock lock(shutdown_service_available_mtx_);
        shutdown_service_available_ = true;
        shutdown_service_available_cv_.notify_all();
    }
}

template<int service_idx, int method_idx>
void npdu_test_client::on_message(const std::shared_ptr<vsomeip::message>& _response) {
    (void)_response;
    // TODO make sure the replies were sent within demanded debounce times
    VSOMEIP_DEBUG << "Received reply from:" << std::setfill('0') << std::hex << std::setw(4) << npdu_test::service_ids[service_idx] << ":"
                  << std::setw(4) << npdu_test::instance_ids[service_idx] << ":" << std::setw(4)
                  << npdu_test::method_ids[service_idx][method_idx];

    // We notify the sender thread every time a message was acknowledged
    std::scoped_lock lk(all_msg_acknowledged_mutexes_[service_idx][method_idx]);
    all_msg_acknowledged_[service_idx][method_idx] = true;
    all_msg_acknowledged_cvs_[service_idx][method_idx].notify_one();
}

template<int service_idx>
void npdu_test_client::send() {
    std::scoped_lock its_lock(mutexes_[service_idx]);
    blocked_[service_idx] = true;
    conditions_[service_idx].notify_one();
}

template<int service_idx>
void npdu_test_client::run() {
    std::unique_lock<std::mutex> its_lock(mutexes_[service_idx]);
    conditions_[service_idx].wait(its_lock, [this] { return blocked_[service_idx]; });
    current_payload_size_[service_idx] = 1;

    {
        std::unique_lock lock(shutdown_service_available_mtx_);
        if (!shutdown_service_available_) {
            shutdown_service_available_cv_.wait(lock, [this] { return shutdown_service_available_; });
        }
    }

    std::uint32_t max_allowed_payload = VSOMEIP_MAX_LOCAL_MESSAGE_SIZE;

    for (std::size_t var = 0; var < payloads_[service_idx].size(); ++var) {
        payloads_[service_idx][var] = vsomeip::runtime::get()->create_payload();
        payload_data_[service_idx][var] = std::vector<vsomeip::byte_t>();
    }

    bool lastrun = false;
    while (current_payload_size_[service_idx] <= max_allowed_payload) {
        // prepare the payloads w/ current payloadsize
        for (std::size_t var = 0; var < payloads_[service_idx].size(); ++var) {
            // assign 0x11 to first, 0x22 to second...
            payload_data_[service_idx][var].assign(current_payload_size_[service_idx], static_cast<vsomeip::byte_t>(0x11 * (var + 1)));
            payloads_[service_idx][var]->set_data(payload_data_[service_idx][var]);
        }

        // send the payloads to the service's methods
        send_messages_sync<service_idx>();

        // Increase array size for next iteration
        current_payload_size_[service_idx] *= 2;

        // special case to test the biggest payload possible as last test
        //  16 Bytes are reserved for the SOME/IP header
        if (current_payload_size_[service_idx] > max_allowed_payload - 16 && !lastrun) {
            current_payload_size_[service_idx] = max_allowed_payload - 16;
            lastrun = true;
        }
    }
    blocked_[service_idx] = false;

    {
        std::scoped_lock its_lock(finished_mutex_);
        finished_[service_idx] = true;
    }
}

template<int service_idx>
void npdu_test_client::send_messages_sync() {
    std::thread t0 = start_send_thread_sync<service_idx, 0>();
    std::thread t1 = start_send_thread_sync<service_idx, 1>();
    std::thread t2 = start_send_thread_sync<service_idx, 2>();
    std::thread t3 = start_send_thread_sync<service_idx, 3>();
    t0.join();
    t1.join();
    t2.join();
    t3.join();
}

template<int service_idx, int method_idx>
std::thread npdu_test_client::start_send_thread_sync() {
    return std::thread([&]() {
        all_msg_acknowledged_unique_locks_[service_idx][method_idx] =
                std::unique_lock<std::mutex>(all_msg_acknowledged_mutexes_[service_idx][method_idx]);

        std::shared_ptr<vsomeip::message> request = vsomeip::runtime::get()->create_request(use_tcp);
        request->set_service(npdu_test::service_ids[service_idx]);
        request->set_instance(npdu_test::instance_ids[service_idx]);
        request->set_method(npdu_test::method_ids[service_idx][method_idx]);
        request->set_payload(payloads_[service_idx][method_idx]);
        for (std::uint32_t i = 0; i < number_of_messages_to_send_; i++) {
            all_msg_acknowledged_[service_idx][method_idx] = false;
            app_->send(request);

            std::chrono::high_resolution_clock::time_point sent = std::chrono::high_resolution_clock::now();

            all_msg_acknowledged_cvs_[service_idx][method_idx].wait(all_msg_acknowledged_unique_locks_[service_idx][method_idx],
                                                                    [this] { return all_msg_acknowledged_[service_idx][method_idx]; });

            std::chrono::nanoseconds waited_for_response = std::chrono::high_resolution_clock::now() - sent;
            if (waited_for_response < applicative_debounce_[service_idx][method_idx]) {
                // make sure we don't send faster than debounce time + max retention time
                std::this_thread::sleep_for(applicative_debounce_[service_idx][method_idx] - waited_for_response);
            }
        }
        all_msg_acknowledged_unique_locks_[service_idx][method_idx].unlock();
    });
}

void npdu_test_client::wait_for_all_senders() {
    bool all_finished(false);
    while (!all_finished) {
        {
            std::scoped_lock its_lock(finished_mutex_);
            if (std::all_of(finished_.begin(), finished_.end(), [](bool i) { return i; })) {
                all_finished = true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    join_sender_thread();

    stop();
}

TEST(someip_npdu_test, send_different_payloads) {
    // get the configuration
    std::shared_ptr<vsomeip::configuration> its_configuration;
    auto its_plugin = vsomeip::plugin_manager::get()->get_plugin(vsomeip::plugin_type_e::CONFIGURATION_PLUGIN, VSOMEIP_CFG_LIBRARY);
    if (its_plugin) {
        auto its_config_plugin = std::dynamic_pointer_cast<vsomeip::configuration_plugin>(its_plugin);
        if (its_config_plugin) {
            its_configuration = its_config_plugin->get_configuration("", "");
        }
    }
    if (!its_configuration) {
        ADD_FAILURE() << "No configuration object. "
                         "Either memory overflow or loading error detected!";
        return;
    }

    // used to store the debounce times
    std::array<std::array<std::chrono::milliseconds, 4>, 4> applicative_debounce;

    // query the debounce times from the configuration. We want to know the
    // debounce times which the _clients_ of this service have to comply with
    // when they send requests to this service.
    // This is necessary as we must ensure a applicative debouncing greater than
    // debounce time + maximum retention time. Therefore the send threads sleep
    // for this amount of time after sending a message.
    for (std::size_t service_id = 0; service_id < applicative_debounce.size(); ++service_id) {
        for (std::size_t method_id = 0; method_id < applicative_debounce[service_id].size(); ++method_id) {
            std::chrono::nanoseconds debounce(0), retention(0);
            its_configuration->get_configured_timing_requests(
                    npdu_test::service_ids[service_id],
                    its_configuration->get_unicast_address(npdu_test::service_ids[service_id], npdu_test::instance_ids[service_id]),
                    its_configuration->get_unreliable_port(npdu_test::service_ids[service_id], npdu_test::instance_ids[service_id]),
                    npdu_test::method_ids[service_id][method_id], &debounce, &retention);
            if (debounce == std::chrono::nanoseconds(VSOMEIP_DEFAULT_NPDU_DEBOUNCING_NANO)
                && retention == std::chrono::nanoseconds(VSOMEIP_DEFAULT_NPDU_MAXIMUM_RETENTION_NANO)) {
                // no timings specified don't don't sleep after sending...
                applicative_debounce[service_id][method_id] = std::chrono::milliseconds(0);
            } else {
                // we add 1 milliseconds to sleep a little bit longer
                applicative_debounce[service_id][method_id] =
                        std::chrono::duration_cast<std::chrono::milliseconds>(debounce + retention) + std::chrono::milliseconds(1);
            }
        }
    }

    npdu_test_client test_client_(use_tcp, applicative_debounce);
    test_client_.init();
    test_client_.start();
}

#if defined(__linux__) || defined(__QNX__)
int main(int argc, char** argv) {
    std::string tcp_enable("--TCP");
    std::string udp_enable("--UDP");
    std::string help("--help");

    int i = 1;
    while (i < argc) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
        } else if (udp_enable == argv[i]) {
            use_tcp = false;
        } else if (help == argv[i]) {
            VSOMEIP_INFO << "Parameters:\n"
                         << "--TCP: Send messages via TCP\n"
                         << "--UDP: Send messages via UDP (default)\n"
                         << "--help: print this help";
        }
        i++;
    }

    return test_main(argc, argv);
}
#endif
