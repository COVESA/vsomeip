// Copyright (C) 2014-2016 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <functional>
#include <iomanip>

#ifndef WIN32
// for umask
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <boost/system/error_code.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/runtime.hpp>

#include "../include/routing_manager_stub.hpp"
#include "../include/routing_manager_stub_host.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/internal.hpp"
#include "../../endpoints/include/local_server_endpoint_impl.hpp"
#include "../../logging/include/logger.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip {

routing_manager_stub::routing_manager_stub(
        routing_manager_stub_host *_host,
        std::shared_ptr<configuration> _configuration) :
        host_(_host),
        io_(_host->get_io()),
        watchdog_timer_(_host->get_io()),
        configuration_(_configuration),
        routingCommandSize_(VSOMEIP_ROUTING_INFO_SIZE_INIT) {
}

routing_manager_stub::~routing_manager_stub() {
}

void routing_manager_stub::init() {
    std::stringstream its_endpoint_path;
    its_endpoint_path << VSOMEIP_BASE_PATH << VSOMEIP_ROUTING_CLIENT;
    endpoint_path_ = its_endpoint_path.str();

    std::stringstream its_local_receiver_path;
    its_local_receiver_path << VSOMEIP_BASE_PATH << std::hex << host_->get_client();
    local_receiver_path_ = its_local_receiver_path.str();
#if WIN32
    ::_unlink(endpoint_path_.c_str());
    ::_unlink(local_receiver_path_.c_str());
    int port = VSOMEIP_INTERNAL_BASE_PORT;
    VSOMEIP_DEBUG << "Routing endpoint at " << port;
#else
    ::unlink(endpoint_path_.c_str());
    ::unlink(local_receiver_path_.c_str());
    VSOMEIP_DEBUG << "Routing endpoint at " << endpoint_path_;

    const mode_t previous_mask(::umask(static_cast<mode_t>(configuration_->get_umask())));
#endif

    endpoint_ =
            std::make_shared < local_server_endpoint_impl
                    > (shared_from_this(),
                    #ifdef WIN32
                        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
                    #else
                        boost::asio::local::stream_protocol::endpoint(endpoint_path_),
                    #endif
                        io_, configuration_->get_max_message_size_local());

    local_receiver_ =
            std::make_shared < local_server_endpoint_impl
                    > (shared_from_this(),
                    #ifdef WIN32
                        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port + host_->get_client()),
                    #else
                        boost::asio::local::stream_protocol::endpoint(local_receiver_path_),
                    #endif
                        io_, configuration_->get_max_message_size_local());

#ifndef WIN32
    ::umask(previous_mask);
#endif
}

void routing_manager_stub::start() {
    endpoint_->start();
    local_receiver_->start();

    client_registration_running_ = true;
    client_registration_thread_ = std::make_shared<std::thread>(
            std::bind(&routing_manager_stub::client_registration_func, this));

    if (configuration_->is_watchdog_enabled()) {
        VSOMEIP_INFO << "Watchdog is enabled : Timeout in ms = "
                     << configuration_->get_watchdog_timeout()
                     << " : Allowed missing pongs = "
                     << configuration_->get_allowed_missing_pongs()
					 << ".";
        start_watchdog();
    } else {
        VSOMEIP_INFO << "Watchdog is disabled!";
    }
}

void routing_manager_stub::stop() {
    client_registration_running_ = false;
    client_registration_condition_.notify_one();
    if (client_registration_thread_->joinable()) {
        client_registration_thread_->join();
    }

    watchdog_timer_.cancel();
    endpoint_->stop();
    local_receiver_->stop();

#ifdef WIN32
    ::_unlink(endpoint_path_.c_str());
    ::_unlink(local_receiver_path_.c_str());
#else
    ::unlink(endpoint_path_.c_str());
    ::unlink(local_receiver_path_.c_str());
#endif

    broadcast_routing_info(true);
}

void routing_manager_stub::on_connect(std::shared_ptr<endpoint> _endpoint) {
    (void)_endpoint;
}

void routing_manager_stub::on_disconnect(std::shared_ptr<endpoint> _endpoint) {
    (void)_endpoint;
}

void routing_manager_stub::on_error(const byte_t *_data, length_t _length,
        endpoint *_receiver) {

    // Implement me when needed

    (void)(_data);
    (void)(_length);
    (void)(_receiver);
}

void routing_manager_stub::release_port(uint16_t _port, bool _reliable) {
	(void)_port;
	(void)_reliable;
	// intentionally empty
}

void routing_manager_stub::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination) {
    (void)_receiver;
    (void)_destination;
#if 0
    std::stringstream msg;
    msg << "rms::on_message: ";
    for (length_t i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif

    if (VSOMEIP_COMMAND_SIZE_POS_MAX < _size) {
        byte_t its_command;
        client_t its_client;
        std::string its_client_endpoint;
        service_t its_service;
        instance_t its_instance;
        eventgroup_t its_eventgroup;
        std::set<eventgroup_t> its_eventgroups;
        event_t its_event;
        bool is_field(false);
        bool is_provided(false);
        major_version_t its_major;
        minor_version_t its_minor;
        std::shared_ptr<payload> its_payload;
        const byte_t *its_data;
        uint32_t its_size;
        bool its_reliable(false);
        bool use_exclusive_proxy(false);
        subscription_type_e its_subscription_type;
        bool is_remote_subscriber(false);

        its_command = _data[VSOMEIP_COMMAND_TYPE_POS];
        std::memcpy(&its_client, &_data[VSOMEIP_COMMAND_CLIENT_POS],
                sizeof(its_client));

        std::memcpy(&its_size, &_data[VSOMEIP_COMMAND_SIZE_POS_MIN],
                sizeof(its_size));

        if (its_size <= _size - VSOMEIP_COMMAND_HEADER_SIZE) {
            switch (its_command) {
            case VSOMEIP_REGISTER_APPLICATION:
                {
                    std::lock_guard<std::mutex> its_lock(client_registration_mutex_);
                    VSOMEIP_INFO << "Application/Client "
                        << std::hex << std::setw(4) << std::setfill('0')
                        << its_client << " is registering."; 
                    pending_client_registrations_[its_client].push_back(true);
                    client_registration_condition_.notify_one();
                }
                break;

            case VSOMEIP_DEREGISTER_APPLICATION:
                {
                    std::lock_guard<std::mutex> its_lock(client_registration_mutex_);
                    VSOMEIP_INFO << "Application/Client "
                            << std::hex << std::setw(4) << std::setfill('0')
                            << its_client << " is deregistering.";
                    pending_client_registrations_[its_client].push_back(false);
                    client_registration_condition_.notify_one();
                }
                break;

            case VSOMEIP_PONG:
                on_pong(its_client);
                VSOMEIP_TRACE << "PONG("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << ")";
                break;

            case VSOMEIP_OFFER_SERVICE:
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance,
                        &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_major));
                std::memcpy(&its_minor, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 5],
                        sizeof(its_minor));
                host_->offer_service(its_client, its_service, its_instance,
                        its_major, its_minor);
                VSOMEIP_DEBUG << "OFFER("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance
                    << ":" << std::dec << int(its_major) << "." << std::dec << its_minor << "]";
                break;

            case VSOMEIP_STOP_OFFER_SERVICE:
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance,
                        &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));

                std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_major));
                std::memcpy(&its_minor, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 5],
                        sizeof(its_minor));

                host_->stop_offer_service(its_client, its_service, its_instance, its_major, its_minor);
                VSOMEIP_DEBUG << "STOP OFFER("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance
                    << ":" << std::dec << int(its_major) << "." << its_minor << "]";
                break;

            case VSOMEIP_SUBSCRIBE:
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                        sizeof(its_major));
                std::memcpy(&is_remote_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                            sizeof(is_remote_subscriber));
                std::memcpy(&its_subscription_type, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                            sizeof(its_subscription_type));
                host_->subscribe(its_client, its_service,
                        its_instance, its_eventgroup, its_major, its_subscription_type);
                VSOMEIP_DEBUG << "SUBSCRIBE("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client <<"): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << ":"
                    << std::dec << (uint16_t)its_major << "]";
                break;

            case VSOMEIP_UNSUBSCRIBE:
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                host_->unsubscribe(its_client, its_service,
                        its_instance, its_eventgroup);
                VSOMEIP_DEBUG << "UNSUBSCRIBE("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]";
                break;

            case VSOMEIP_SUBSCRIBE_ACK:
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                host_->on_subscribe_ack(its_client, its_service, its_instance, its_eventgroup);
                VSOMEIP_DEBUG << "SUBSCRIBE ACK("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]";
                break;

            case VSOMEIP_SUBSCRIBE_NACK:
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                host_->on_subscribe_nack(its_client, its_service, its_instance, its_eventgroup);
                VSOMEIP_DEBUG << "SUBSCRIBE NACK("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]";
                break;

            case VSOMEIP_SEND:
                its_data = &_data[VSOMEIP_COMMAND_PAYLOAD_POS];
                its_service = VSOMEIP_BYTES_TO_WORD(
                                its_data[VSOMEIP_SERVICE_POS_MIN],
                                its_data[VSOMEIP_SERVICE_POS_MAX]);
                std::memcpy(&its_instance, &_data[_size - sizeof(instance_t)
                                                  - sizeof(bool) - sizeof(bool)
                                                  - sizeof(bool)], sizeof(its_instance));
                std::memcpy(&its_reliable, &_data[_size - sizeof(bool)
                                                  - sizeof(bool)], sizeof(its_reliable));
                host_->on_message(its_service, its_instance, its_data, its_size, its_reliable);
                break;

            case VSOMEIP_NOTIFY:
                its_data = &_data[VSOMEIP_COMMAND_PAYLOAD_POS];
                its_service = VSOMEIP_BYTES_TO_WORD(
                                its_data[VSOMEIP_SERVICE_POS_MIN],
                                its_data[VSOMEIP_SERVICE_POS_MAX]);
                its_client = VSOMEIP_BYTES_TO_WORD(
                        its_data[VSOMEIP_CLIENT_POS_MIN],
                        its_data[VSOMEIP_CLIENT_POS_MAX]);
                std::memcpy(&its_instance, &_data[_size - sizeof(instance_t)
                                                  - sizeof(bool) - sizeof(bool)
                                                  - sizeof(bool)], sizeof(its_instance));
                host_->on_notification(its_client, its_service, its_instance, its_data, its_size);
                break;
            case VSOMEIP_NOTIFY_ONE:
                its_data = &_data[VSOMEIP_COMMAND_PAYLOAD_POS];
                its_service = VSOMEIP_BYTES_TO_WORD(
                                its_data[VSOMEIP_SERVICE_POS_MIN],
                                its_data[VSOMEIP_SERVICE_POS_MAX]);
                std::memcpy(&its_instance, &_data[_size - sizeof(instance_t)
                                                  - sizeof(bool) - sizeof(bool)
                                                  - sizeof(bool)], sizeof(its_instance));
                host_->on_notification(its_client, its_service, its_instance, its_data, its_size, true);
                break;

            case VSOMEIP_REQUEST_SERVICE:
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance,
                        &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_major));
                std::memcpy(&its_minor, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 5],
                        sizeof(its_minor));
                std::memcpy(&use_exclusive_proxy, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 9],
                                        sizeof(use_exclusive_proxy));
                host_->request_service(its_client, its_service, its_instance,
                        its_major, its_minor, use_exclusive_proxy);
                VSOMEIP_DEBUG << "REQUEST("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << ":"
                    << std::dec << int(its_major) << "." << std::dec << its_minor << "]";
                break;

                case VSOMEIP_RELEASE_SERVICE:
                    std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_service));
                    std::memcpy(&its_instance,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                            sizeof(its_instance));
                    host_->release_service(its_client, its_service, its_instance);
                    VSOMEIP_DEBUG << "RELEASE("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_instance << "]";
                    break;

                case VSOMEIP_REGISTER_EVENT:
                    std::memcpy(&its_service,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_service));
                    std::memcpy(&its_instance,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                            sizeof(its_instance));
                    std::memcpy(&its_event,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                            sizeof(its_event));
                    std::memcpy(&is_field,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                            sizeof(is_field));
                    std::memcpy(&is_provided,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                            sizeof(is_provided));
                    for (std::size_t i = 8; i+1 < its_size; i++) {
                        std::memcpy(&its_eventgroup,
                                &_data[VSOMEIP_COMMAND_PAYLOAD_POS + i],
                                sizeof(its_eventgroup));
                        its_eventgroups.insert(its_eventgroup);
                    }
                    host_->register_shadow_event(its_client, its_service,
                            its_instance, its_event, its_eventgroups,
                            is_field, is_provided);
                    VSOMEIP_DEBUG << "REGISTER EVENT("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_event
                        << ":is_provider=" << is_provided << "]";
                    break;

                case VSOMEIP_UNREGISTER_EVENT:
                    std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_service));
                    std::memcpy(&its_instance,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                            sizeof(its_instance));
                    std::memcpy(&its_event, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                            sizeof(its_event));
                    std::memcpy(&is_provided,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                            sizeof(is_provided));
                    host_->unregister_shadow_event(its_client, its_service, its_instance,
                            its_event, is_provided);
                    VSOMEIP_DEBUG << "UNREGISTER EVENT("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_event
                        << ":is_provider=" << is_provided << "]";
                    break;

                case VSOMEIP_ID_RESPONSE:
                    std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_service));
                    std::memcpy(&its_instance,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                            sizeof(its_instance));
                    std::memcpy(&its_reliable,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                            sizeof(its_reliable));
                    host_->on_identify_response(its_client, its_service, its_instance, its_reliable);
                    VSOMEIP_TRACE << "ID RESPONSE("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_instance
                        << ":is_reliable=" << its_reliable << "]";
                    break;
            }
        }
    }
}

void routing_manager_stub::on_register_application(client_t _client) {
    auto endpoint = host_->find_local(_client);
    if (endpoint) {
        VSOMEIP_ERROR << "Registering application: " << std::hex << _client
                << " failed. It is already registered!";
    } else {
        (void)host_->find_or_create_local(_client);
        std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
        routing_info_[_client].first = 0;
    }
}

void routing_manager_stub::on_deregister_application(client_t _client) {
    routing_info_mutex_.lock();
    auto its_info = routing_info_.find(_client);
    if (its_info != routing_info_.end()) {
        for (auto &its_service : its_info->second.second) {
            for (auto &its_instance : its_service.second) {
                auto its_version = its_instance.second;
                routing_info_mutex_.unlock();
                host_->on_stop_offer_service(its_service.first, its_instance.first, its_version.first, its_version.second);
                routing_info_mutex_.lock();
            }
        }
    }
    routing_info_.erase(_client);
    routing_info_mutex_.unlock();
    host_->remove_local(_client);
}

void routing_manager_stub::client_registration_func(void) {
    std::unique_lock<std::mutex> its_lock(client_registration_mutex_);
    while (client_registration_running_) {
        while (!pending_client_registrations_.size() && client_registration_running_) {
            client_registration_condition_.wait(its_lock);
        }

        std::map<client_t, std::vector<bool>> its_registrations(
        		pending_client_registrations_);
        pending_client_registrations_.clear();
        its_lock.unlock();

        for (auto r : its_registrations) {
            for (auto b : r.second) {
                if (b) {
                    on_register_application(r.first);
                } else {
                    on_deregister_application(r.first);
                }
            }
        }

        {
        	std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
        	broadcast_routing_info();
        }

        its_lock.lock();
    }
}


void routing_manager_stub::on_offer_service(client_t _client,
        service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor) {
    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    routing_info_[_client].second[_service][_instance] = std::make_pair(_major, _minor);
    broadcast_routing_info();
}

void routing_manager_stub::on_stop_offer_service(client_t _client,
        service_t _service, instance_t _instance,  major_version_t _major, minor_version_t _minor) {
    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    auto found_client = routing_info_.find(_client);
    if (found_client != routing_info_.end()) {
        auto found_service = found_client->second.second.find(_service);
        if (found_service != found_client->second.second.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                auto found_version = found_instance->second;
                if( _major == found_version.first && _minor == found_version.second) {
                    found_service->second.erase(_instance);
                    if (0 == found_service->second.size()) {
                        found_client->second.second.erase(_service);
                    }
                    broadcast_routing_info();
                } else if( _major == DEFAULT_MAJOR && _minor == DEFAULT_MINOR) {
                    found_service->second.erase(_instance);
                    if (0 == found_service->second.size()) {
                        found_client->second.second.erase(_service);
                    }
                    broadcast_routing_info();
                }
            }
        }
    }
}

void routing_manager_stub::send_routing_info(client_t _client, bool _empty) {
    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        // Create the command vector & reserve some bytes initially..
        // ..to avoid reallocation for smaller messages!
        std::vector<byte_t> its_command;
        its_command.reserve(routingCommandSize_);

        // Routing command
        its_command.push_back(VSOMEIP_ROUTING_INFO);

        // Sender client
        client_t client = 0x0;
        for (uint32_t i = 0; i < sizeof(client_t); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&client)[i]);
        }

        // Overall size placeholder
        byte_t size_placeholder = 0x0;
        for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
            its_command.push_back(size_placeholder);
        }

        // Routing info loop
        for (auto &info : routing_info_) {
            if (_empty) {
                break;
            }
            std::size_t its_size_pos = its_command.size();
            std::size_t its_entry_size = its_command.size();
            // Client size placeholder
            byte_t placeholder = 0x0;
            for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
                its_command.push_back(placeholder);
            }
            // Client
            for (uint32_t i = 0; i < sizeof(client_t); ++i) {
                 its_command.push_back(
                         reinterpret_cast<const byte_t*>(&info.first)[i]);
            }
            // Iterate over all services
            for (auto &service : info.second.second) {
                // Service entry size
                uint32_t its_service_entry_size = uint32_t(sizeof(service_t)
                        + service.second.size() * (sizeof(instance_t)
                        + sizeof(major_version_t) + sizeof(minor_version_t)));
                for (uint32_t i = 0; i < sizeof(its_service_entry_size); ++i) {
                    its_command.push_back(
                            reinterpret_cast<const byte_t*>(&its_service_entry_size)[i]);
                }
                // Service
                for (uint32_t i = 0; i < sizeof(service_t); ++i) {
                    its_command.push_back(
                            reinterpret_cast<const byte_t*>(&service.first)[i]);
                }
                // Iterate over all instances
                for (auto &instance : service.second) {
                    // Instance
                    for (uint32_t i = 0; i < sizeof(instance_t); ++i) {
                        its_command.push_back(
                                reinterpret_cast<const byte_t*>(&instance)[i]);
                    }
                    // Major version
                    for (uint32_t i = 0; i < sizeof(major_version_t); ++i) {
                        its_command.push_back(
                                reinterpret_cast<const byte_t*>(&instance.second.first)[i]);
                    }
                    // Minor version
                    for (uint32_t i = 0; i < sizeof(minor_version_t); ++i) {
                        its_command.push_back(
                                reinterpret_cast<const byte_t*>(&instance.second.second)[i]);
                    }
                }
            }
            // File client size
            its_entry_size = its_command.size() - its_entry_size - uint32_t(sizeof(uint32_t));
            std::memcpy(&its_command[its_size_pos], &its_entry_size, sizeof(uint32_t));
        }

        // File overall size
        std::size_t its_size = its_command.size() - VSOMEIP_COMMAND_PAYLOAD_POS;
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size, sizeof(uint32_t));
        its_size += VSOMEIP_COMMAND_PAYLOAD_POS;

        // Double init size until it fits into the actual size for next run
        size_t newInitSize;
        for (newInitSize = VSOMEIP_ROUTING_INFO_SIZE_INIT;
                newInitSize < its_size; newInitSize *= 2);
        routingCommandSize_ = newInitSize;

#if 0
        std::stringstream msg;
        msg << "rms::send_routing_info ";
        for (uint32_t i = 0; i < its_size; ++i)
            msg << std::hex << std::setw(2) << std::setfill('0') << (int)its_command[i] << " ";
        VSOMEIP_DEBUG << msg.str();
#endif

        // Send routing info or error!
        if(its_command.size() <= VSOMEIP_MAX_LOCAL_MESSAGE_SIZE) {
            its_endpoint->send(&its_command[0], uint32_t(its_size), true);
        } else {
            VSOMEIP_ERROR << "Routing info exceeds maximum message size: Can't send!";
        }
    }
}

void routing_manager_stub::broadcast_routing_info(bool _empty) {
    for (auto& info : routing_info_) {
        if (info.first != VSOMEIP_ROUTING_CLIENT) {
            send_routing_info(info.first, _empty);
        }
    }
}

void routing_manager_stub::broadcast(std::vector<byte_t> &_command) const {
    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    for (auto a : routing_info_) {
        if (a.first > 0) {
            std::shared_ptr<endpoint> its_endpoint
                = host_->find_local(a.first);
            if (its_endpoint) {
                its_endpoint->send(&_command[0], uint32_t(_command.size()), true);
            }
        }
    }
}

void routing_manager_stub::send_subscribe(std::shared_ptr<vsomeip::endpoint> _target,
        client_t _client, service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, major_version_t _major, bool _is_remote_subscriber) {
    if (_target) {
        byte_t its_command[VSOMEIP_SUBSCRIBE_COMMAND_SIZE];
        uint32_t its_size = VSOMEIP_SUBSCRIBE_COMMAND_SIZE
                - VSOMEIP_COMMAND_HEADER_SIZE;
        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
                sizeof(_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                sizeof(_eventgroup));
        its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6] = _major;
        its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 7] = _is_remote_subscriber;

        _target->send(its_command, sizeof(its_command));
    }
}

void routing_manager_stub::send_unsubscribe(std::shared_ptr<vsomeip::endpoint> _target,
        client_t _client, service_t _service, instance_t _instance,
        eventgroup_t _eventgroup) {
    if (_target) {
        byte_t its_command[VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE];
        uint32_t its_size = VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE
                - VSOMEIP_COMMAND_HEADER_SIZE;
        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_UNSUBSCRIBE;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
                sizeof(_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                sizeof(_eventgroup));

        _target->send(its_command, sizeof(its_command));
    }
}

void routing_manager_stub::send_subscribe_ack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) {

    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        byte_t its_command[VSOMEIP_SUBSCRIBE_COMMAND_SIZE];
        uint32_t its_size = VSOMEIP_SUBSCRIBE_COMMAND_SIZE
                - VSOMEIP_COMMAND_HEADER_SIZE;

        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE_ACK;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
                sizeof(_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                sizeof(_eventgroup));

        its_endpoint->send(&its_command[0], sizeof(its_command), true);
    }
}

void routing_manager_stub::send_subscribe_nack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup) {

    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        byte_t its_command[VSOMEIP_SUBSCRIBE_COMMAND_SIZE];
        uint32_t its_size = VSOMEIP_SUBSCRIBE_COMMAND_SIZE
                - VSOMEIP_COMMAND_HEADER_SIZE;

        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE_NACK;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
                sizeof(_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                sizeof(_eventgroup));

        its_endpoint->send(&its_command[0], sizeof(its_command), true);
    }
}

// Watchdog
void routing_manager_stub::broadcast_ping() const {
    const byte_t its_ping[] = {
    VSOMEIP_PING, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

    std::vector<byte_t> its_command(sizeof(its_ping));
    its_command.assign(its_ping, its_ping + sizeof(its_ping));
    broadcast(its_command);
}

void routing_manager_stub::on_pong(client_t _client) {
    auto found_info = routing_info_.find(_client);
    if (found_info != routing_info_.end()) {
        found_info->second.first = 0;
    } else {
        VSOMEIP_ERROR << "Received PONG from unregistered application!";
    }
}

void routing_manager_stub::start_watchdog() {
    // Divide / 2 as start and check sleep each
    watchdog_timer_.expires_from_now(
            std::chrono::milliseconds(configuration_->get_watchdog_timeout() / 2));

    std::function<void(boost::system::error_code const &)> its_callback =
            [this](boost::system::error_code const &_error) {
                if (!_error)
                check_watchdog();
            };

    watchdog_timer_.async_wait(its_callback);
}

void routing_manager_stub::check_watchdog() {
    {
        std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
        for (auto i = routing_info_.begin(); i != routing_info_.end(); ++i) {
            i->second.first++;
        }
    }
    broadcast_ping();

    watchdog_timer_.expires_from_now(
            std::chrono::milliseconds(configuration_->get_watchdog_timeout() / 2));

    std::function<void(boost::system::error_code const &)> its_callback =
            [this](boost::system::error_code const &_error) {
                (void)_error;
                std::list< client_t > lost;
                {
                    std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
                    for (auto i : routing_info_) {
                        if (i.first > 0 && i.first != host_->get_client()) {
                            if (i.second.first > configuration_->get_allowed_missing_pongs()) {
                                VSOMEIP_WARNING << "Lost contact to application " << std::hex << (int)i.first;
                                lost.push_back(i.first);
                            }
                        }
                    }
                }
                for (auto i : lost) {
                    utility::release_client_id(i);
                    on_deregister_application(i);
                }
                start_watchdog();
            };

    watchdog_timer_.async_wait(its_callback);
}

bool routing_manager_stub::queue_message(const byte_t *_data, uint32_t _size) {
    std::shared_ptr<local_server_endpoint_impl> its_server_endpoint
     = std::dynamic_pointer_cast<local_server_endpoint_impl>(endpoint_);
    return its_server_endpoint->queue_message(_data, _size);
}

} // namespace vsomeip
