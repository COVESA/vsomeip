// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <functional>
#include <iomanip>
#include <forward_list>

#include <boost/system/error_code.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/error.hpp>
#include <vsomeip/internal/logger.hpp>

#include "../include/routing_manager_stub.hpp"
#include "../include/routing_manager_stub_host.hpp"
#include "../include/remote_subscription.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../security/include/security.hpp"

#include "../../endpoints/include/local_server_endpoint_impl.hpp"
#include "../../endpoints/include/endpoint_manager_impl.hpp"
#include "../../utility/include/byteorder.hpp"
#include "../../utility/include/utility.hpp"
#include "../implementation/message/include/payload_impl.hpp"

namespace vsomeip_v3 {

const std::vector<byte_t> routing_manager_stub::its_ping_(
    { VSOMEIP_PING, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 });

routing_manager_stub::routing_manager_stub(
        routing_manager_stub_host *_host,
        const std::shared_ptr<configuration>& _configuration) :
        host_(_host),
        io_(_host->get_io()),
        watchdog_timer_(_host->get_io()),
        client_id_timer_(_host->get_io()),
        endpoint_(nullptr),
        local_receiver_(nullptr),
        configuration_(_configuration),
        is_socket_activated_(false),
        client_registration_running_(false),
        max_local_message_size_(configuration_->get_max_message_size_local()),
        configured_watchdog_timeout_(configuration_->get_watchdog_timeout()),
        pinged_clients_timer_(io_),
        pending_security_update_id_(0) {
}

routing_manager_stub::~routing_manager_stub() {
}

void routing_manager_stub::init() {
    init_routing_endpoint();
}

void routing_manager_stub::start() {
    {
        std::lock_guard<std::mutex> its_lock(used_client_ids_mutex_);
        used_client_ids_ = utility::get_used_client_ids();
        // Wait VSOMEIP_MAX_CONNECT_TIMEOUT * 2 and expect after that time
        // that all client_ids are used have to be connected to the routing.
        // Otherwise they can be marked as "erroneous client".
        client_id_timer_.expires_from_now(std::chrono::milliseconds(VSOMEIP_MAX_CONNECT_TIMEOUT * 2));
        client_id_timer_.async_wait(
            std::bind(
                    &routing_manager_stub::on_client_id_timer_expired,
                    std::dynamic_pointer_cast<routing_manager_stub>(shared_from_this()),
                    std::placeholders::_1));
    }

    if (!endpoint_) {
        // application has been stopped and started again
        init_routing_endpoint();
    }
    if (endpoint_) {
        endpoint_->start();
    }

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

    {
        std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
        routing_info_[host_->get_client()].first = 0;
    }
}

void routing_manager_stub::stop() {
    {
        std::lock_guard<std::mutex> its_lock(client_registration_mutex_);
        client_registration_running_ = false;
        client_registration_condition_.notify_one();
    }
    if (client_registration_thread_->joinable()) {
        client_registration_thread_->join();
    }

    {
        std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
        watchdog_timer_.cancel();
    }

    {
        std::lock_guard<std::mutex> its_lock(used_client_ids_mutex_);
        client_id_timer_.cancel();
    }

    if( !is_socket_activated_) {
        endpoint_->stop();
        endpoint_ = nullptr;
        std::stringstream its_endpoint_path;
        its_endpoint_path << utility::get_base_path(configuration_) << std::hex
                << VSOMEIP_ROUTING_CLIENT;
#ifdef _WIN32
        ::_unlink(its_endpoint_path.str().c_str());
#else
        if (-1 == ::unlink(its_endpoint_path.str().c_str())) {
            VSOMEIP_ERROR << "routing_manager_stub::stop() unlink failed ("
                    << its_endpoint_path.str() << "): "<< std::strerror(errno);
        }
#endif
    }

    if(local_receiver_) {
        local_receiver_->stop();
        local_receiver_ = nullptr;
        std::stringstream its_local_receiver_path;
        its_local_receiver_path << utility::get_base_path(configuration_)
                << std::hex << host_->get_client();
#ifdef _WIN32
        ::_unlink(its_local_receiver_path.str().c_str());
#else
        if (-1 == ::unlink(its_local_receiver_path.str().c_str())) {
            VSOMEIP_ERROR << "routing_manager_stub::stop() unlink (local receiver) failed ("
                    << its_local_receiver_path.str() << "): "<< std::strerror(errno);
        }
#endif
    }
}

void routing_manager_stub::on_message(const byte_t *_data, length_t _size,
        endpoint *_receiver, const boost::asio::ip::address &_destination,
        client_t _bound_client,
        credentials_t _credentials,
        const boost::asio::ip::address &_remote_address,
        std::uint16_t _remote_port) {
    (void)_receiver;
    (void)_destination;
    (void)_remote_address;
    (void) _remote_port;
#if 0
    std::stringstream msg;
    msg << "rms::on_message: ";
    for (length_t i = 0; i < _size; ++i)
        msg << std::hex << std::setw(2) << std::setfill('0') << (int)_data[i] << " ";
    VSOMEIP_INFO << msg.str();
#endif

    std::uint32_t its_sender_uid = std::get<0>(_credentials);
    std::uint32_t its_sender_gid = std::get<1>(_credentials);

    if (VSOMEIP_COMMAND_SIZE_POS_MAX < _size) {
        byte_t its_command;
        client_t its_client;
        std::string its_client_endpoint;
        service_t its_service;
        instance_t its_instance;
        method_t its_method;
        eventgroup_t its_eventgroup;
        event_t its_notifier;
        event_type_e its_event_type;
        bool is_provided(false);
        major_version_t its_major;
        minor_version_t its_minor;
        std::shared_ptr<payload> its_payload;
        const byte_t *its_data;
        uint32_t its_size;
        bool its_reliable(false);
        client_t its_client_from_header;
        client_t its_target_client;
        client_t its_subscriber;
        uint8_t its_check_status(0);
        std::uint16_t its_subscription_id(PENDING_SUBSCRIPTION_ID);
        offer_type_e its_offer_type;

        its_command = _data[VSOMEIP_COMMAND_TYPE_POS];
        std::memcpy(&its_client, &_data[VSOMEIP_COMMAND_CLIENT_POS],
                sizeof(its_client));

        if (security::get()->is_enabled() && _bound_client != its_client) {
            VSOMEIP_WARNING << "vSomeIP Security: routing_manager_stub::on_message: "
                    << "Routing Manager received a message from client "
                    << std::hex << std::setw(4) << std::setfill('0')
                    << its_client << " with command " << (uint32_t)its_command
                    << " which doesn't match the bound client "
                    << std::setw(4) << std::setfill('0') << _bound_client
                    << " ~> skip message!";
            return;
        }

        std::memcpy(&its_size, &_data[VSOMEIP_COMMAND_SIZE_POS_MIN],
                sizeof(its_size));

        if (its_size <= _size - VSOMEIP_COMMAND_HEADER_SIZE) {
            switch (its_command) {
            case VSOMEIP_REGISTER_APPLICATION:
                if (_size != VSOMEIP_REGISTER_APPLICATION_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a REGISTER_APPLICATION command with wrong size ~> skip!";
                    break;
                }
                update_registration(its_client, registration_type_e::REGISTER);
                break;

            case VSOMEIP_DEREGISTER_APPLICATION:
                if (_size != VSOMEIP_DEREGISTER_APPLICATION_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a DEREGISTER_APPLICATION command with wrong size ~> skip!";
                    break;
                }
                update_registration(its_client, registration_type_e::DEREGISTER);
                break;

            case VSOMEIP_PONG:
                if (_size != VSOMEIP_PONG_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a PONG command with wrong size ~> skip!";
                    break;
                }
                on_pong(its_client);
                VSOMEIP_TRACE << "PONG("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << ")";
                break;

            case VSOMEIP_OFFER_SERVICE:
                if (_size != VSOMEIP_OFFER_SERVICE_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a OFFER_SERVICE command with wrong size ~> skip!";
                    break;
                }

                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance,
                        &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_major));
                std::memcpy(&its_minor, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 5],
                        sizeof(its_minor));

                if (security::get()->is_offer_allowed(its_sender_uid, its_sender_gid,
                        its_client, its_service, its_instance)) {
                    host_->offer_service(its_client, its_service, its_instance,
                            its_major, its_minor);
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_client
                            << " : routing_manager_stub::on_message: isn't allowed to offer "
                            << "the following service/instance " << its_service << "/" << its_instance
                            << " ~> Skip offer!";
                }
                break;

            case VSOMEIP_STOP_OFFER_SERVICE:
                if (_size != VSOMEIP_STOP_OFFER_SERVICE_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a STOP_OFFER_SERVICE command with wrong size ~> skip!";
                    break;
                }
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
                break;

            case VSOMEIP_SUBSCRIBE:
                if (_size != VSOMEIP_SUBSCRIBE_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a SUBSCRIBE command with wrong size ~> skip!";
                    break;
                }
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                        sizeof(its_major));
                std::memcpy(&its_notifier, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                        sizeof(its_notifier));

                if (its_notifier == ANY_EVENT) {
                    if (host_->is_subscribe_to_any_event_allowed(_credentials, its_client, its_service,
                            its_instance, its_eventgroup)) {
                        host_->subscribe(its_client, its_sender_uid, its_sender_gid, its_service, its_instance,
                                its_eventgroup, its_major, its_notifier);
                    } else {
                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_client
                                << " :  routing_manager_stub::on_message: "
                                << " subscribes to service/instance/event "
                                << its_service << "/" << its_instance << "/ANY_EVENT"
                                << " which violates the security policy ~> Skip subscribe!";
                    }
                } else {
                    if (security::get()->is_client_allowed(its_sender_uid, its_sender_gid,
                            its_client, its_service, its_instance, its_notifier)) {
                        host_->subscribe(its_client, its_sender_uid, its_sender_gid, its_service, its_instance,
                                its_eventgroup, its_major, its_notifier);
                    } else {
                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_client
                                << " :  routing_manager_stub::on_message: "
                                << " subscribes to service/instance/event "
                                << its_service << "/" << its_instance << "/" << its_notifier
                                << " which violates the security policy ~> Skip subscribe!";
                    }
                }
                break;

            case VSOMEIP_UNSUBSCRIBE:
                if (_size != VSOMEIP_UNSUBSCRIBE_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a UNSUBSCRIBE command with wrong size ~> skip!";
                    break;
                }
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                std::memcpy(&its_notifier, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                        sizeof(its_notifier));

                host_->unsubscribe(its_client, its_sender_uid, its_sender_gid, its_service,
                        its_instance, its_eventgroup, its_notifier);
                break;

            case VSOMEIP_SUBSCRIBE_ACK:
                if (_size != VSOMEIP_SUBSCRIBE_ACK_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a SUBSCRIBE_ACK command with wrong size ~> skip!";
                    break;
                }
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                std::memcpy(&its_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                        sizeof(its_subscriber));
                std::memcpy(&its_notifier, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                        sizeof(its_notifier));
                std::memcpy(&its_subscription_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 10],
                        sizeof(its_subscription_id));
                host_->on_subscribe_ack(its_subscriber, its_service,
                        its_instance, its_eventgroup, its_notifier, its_subscription_id);
                VSOMEIP_INFO << "SUBSCRIBE ACK("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_notifier << "]";
                break;

            case VSOMEIP_SUBSCRIBE_NACK:
                if (_size != VSOMEIP_SUBSCRIBE_NACK_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a SUBSCRIBE_NACK command with wrong size ~> skip!";
                    break;
                }
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                std::memcpy(&its_subscriber, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                        sizeof(its_subscriber));
                std::memcpy(&its_notifier, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                        sizeof(its_notifier));
                std::memcpy(&its_subscription_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 10],
                        sizeof(its_subscription_id));
                host_->on_subscribe_nack(its_subscriber, its_service,
                        its_instance, its_eventgroup, its_notifier, its_subscription_id);
                VSOMEIP_INFO << "SUBSCRIBE NACK("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_notifier << "]";
                break;
            case VSOMEIP_UNSUBSCRIBE_ACK:
                if (_size != VSOMEIP_UNSUBSCRIBE_ACK_COMMAND_SIZE) {
                    VSOMEIP_WARNING << "Received a VSOMEIP_UNSUBSCRIBE_ACK command with wrong size ~> skip!";
                    break;
                }
                std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                        sizeof(its_service));
                std::memcpy(&its_instance, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                        sizeof(its_instance));
                std::memcpy(&its_eventgroup, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                        sizeof(its_eventgroup));
                std::memcpy(&its_subscription_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                        sizeof(its_subscription_id));
                host_->on_unsubscribe_ack(its_client, its_service,
                        its_instance, its_eventgroup, its_subscription_id);
                VSOMEIP_INFO << "UNSUBSCRIBE ACK("
                    << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                    << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                    << std::hex << std::setw(4) << std::setfill('0') << its_eventgroup << "]";
                break;
            case VSOMEIP_SEND: {
                if (_size < VSOMEIP_SEND_COMMAND_SIZE + VSOMEIP_FULL_HEADER_SIZE) {
                    VSOMEIP_WARNING << "Received a SEND command with too small size ~> skip!";
                    break;
                }
                its_data = &_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS];
                its_service = VSOMEIP_BYTES_TO_WORD(
                                its_data[VSOMEIP_SERVICE_POS_MIN],
                                its_data[VSOMEIP_SERVICE_POS_MAX]);
                its_client_from_header = VSOMEIP_BYTES_TO_WORD(
                        its_data[VSOMEIP_CLIENT_POS_MIN],
                        its_data[VSOMEIP_CLIENT_POS_MAX]);
                its_method = VSOMEIP_BYTES_TO_WORD(
                        its_data[VSOMEIP_METHOD_POS_MIN],
                        its_data[VSOMEIP_METHOD_POS_MAX]);
                std::memcpy(&its_instance, &_data[VSOMEIP_SEND_COMMAND_INSTANCE_POS_MIN],
                            sizeof(its_instance));
                std::memcpy(&its_reliable, &_data[VSOMEIP_SEND_COMMAND_RELIABLE_POS],
                            sizeof(its_reliable));
                std::memcpy(&its_check_status, &_data[VSOMEIP_SEND_COMMAND_CHECK_STATUS_POS],
                            sizeof(its_check_status));

                // Allow response messages from local proxies as answer to remote requests
                // but check requests sent by local proxies to remote against policy.
                if (utility::is_request(its_data[VSOMEIP_MESSAGE_TYPE_POS])) {
                    if (!security::get()->is_client_allowed(its_sender_uid, its_sender_gid,
                            its_client_from_header, its_service, its_instance, its_method)) {
                        VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex << its_client_from_header
                                << " : routing_manager_stub::on_message: "
                                << " isn't allowed to send a request to service/instance/method "
                                << its_service << "/" << its_instance << "/" << its_method
                                << " ~> Skip message!";
                        return;
                    }
                }
                // reduce by size of instance, flush, reliable, client and is_valid_crc flag
                const std::uint32_t its_message_size = its_size -
                        (VSOMEIP_SEND_COMMAND_SIZE - VSOMEIP_COMMAND_HEADER_SIZE);
                if (its_message_size !=
                        VSOMEIP_BYTES_TO_LONG(_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 1],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 2],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 3])
                        + VSOMEIP_SOMEIP_HEADER_SIZE) {
                    VSOMEIP_WARNING << "Received a SEND command containing message with invalid size -> skip!";
                    break;
                }
                host_->on_message(its_service, its_instance, its_data, its_message_size,
                        its_reliable, _bound_client, _credentials, its_check_status, false);
                break;
            }
            case VSOMEIP_NOTIFY: {
                if (_size < VSOMEIP_SEND_COMMAND_SIZE + VSOMEIP_FULL_HEADER_SIZE) {
                    VSOMEIP_WARNING << "Received a NOTIFY command with too small size ~> skip!";
                    break;
                }
                its_data = &_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS];
                its_service = VSOMEIP_BYTES_TO_WORD(
                                its_data[VSOMEIP_SERVICE_POS_MIN],
                                its_data[VSOMEIP_SERVICE_POS_MAX]);
                std::memcpy(&its_instance, &_data[VSOMEIP_SEND_COMMAND_INSTANCE_POS_MIN],
                            sizeof(its_instance));
                // reduce by size of instance, flush, reliable, is_valid_crc flag and target client
                const std::uint32_t its_message_size = its_size -
                        (VSOMEIP_SEND_COMMAND_SIZE - VSOMEIP_COMMAND_HEADER_SIZE);
                if (its_message_size !=
                        VSOMEIP_BYTES_TO_LONG(_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 1],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 2],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 3])
                        + VSOMEIP_SOMEIP_HEADER_SIZE) {
                    VSOMEIP_WARNING << "Received a NOTIFY command containing message with invalid size -> skip!";
                    break;
                }
                host_->on_notification(VSOMEIP_ROUTING_CLIENT, its_service, its_instance, its_data, its_message_size);
                break;
            }
            case VSOMEIP_NOTIFY_ONE: {
                if (_size < VSOMEIP_SEND_COMMAND_SIZE + VSOMEIP_FULL_HEADER_SIZE) {
                    VSOMEIP_WARNING << "Received a NOTIFY_ONE command with too small size ~> skip!";
                    break;
                }
                its_data = &_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS];
                its_service = VSOMEIP_BYTES_TO_WORD(
                                its_data[VSOMEIP_SERVICE_POS_MIN],
                                its_data[VSOMEIP_SERVICE_POS_MAX]);
                std::memcpy(&its_instance, &_data[VSOMEIP_SEND_COMMAND_INSTANCE_POS_MIN],
                            sizeof(its_instance));
                std::memcpy(&its_target_client, &_data[VSOMEIP_SEND_COMMAND_DST_CLIENT_POS_MIN],
                            sizeof(client_t));
                // reduce by size of instance, flush, reliable flag, is_valid_crc and target client
                const std::uint32_t its_message_size = its_size -
                        (VSOMEIP_SEND_COMMAND_SIZE - VSOMEIP_COMMAND_HEADER_SIZE);
                if (its_message_size !=
                        VSOMEIP_BYTES_TO_LONG(_data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 1],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 2],
                                              _data[VSOMEIP_SEND_COMMAND_PAYLOAD_POS + VSOMEIP_LENGTH_POS_MIN + 3])
                        + VSOMEIP_SOMEIP_HEADER_SIZE) {
                    VSOMEIP_WARNING << "Received a NOTIFY_ONE command containing message with invalid size -> skip!";
                    break;
                }
                host_->on_notification(its_target_client, its_service, its_instance,
                        its_data, its_message_size, true);
                break;
            }
            case VSOMEIP_REQUEST_SERVICE:
                {
                    uint32_t entry_size = (sizeof(service_t) + sizeof(instance_t)
                            + sizeof(major_version_t) + sizeof(minor_version_t));
                    if (its_size % entry_size > 0) {
                        VSOMEIP_WARNING << "Received a REQUEST_SERVICE command with invalid size -> skip!";
                        break;
                    }
                    uint32_t request_count(its_size / entry_size);
                    std::set<service_data_t> requests;
                    for (uint32_t i = 0; i < request_count; ++i) {
                        service_t its_service;
                        instance_t its_instance;
                        major_version_t its_major;
                        minor_version_t its_minor;
                        std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + (i * entry_size)],
                                sizeof(its_service));
                        std::memcpy(&its_instance,
                                &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2 + (i * entry_size)],
                                sizeof(its_instance));
                        std::memcpy(&its_major, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4 + (i * entry_size)],
                                sizeof(its_major));
                        std::memcpy(&its_minor, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 5 + (i * entry_size)],
                                sizeof(its_minor));
                        if (security::get()->is_client_allowed(its_sender_uid, its_sender_gid,
                                its_client, its_service, its_instance, 0x00, true)) {
                            host_->request_service(its_client, its_service, its_instance,
                                    its_major, its_minor );
                            service_data_t request = {
                                    its_service, its_instance,
                                    its_major, its_minor
                            };
                            requests.insert(request);
                        } else {
                            VSOMEIP_WARNING << "vSomeIP Security: Client 0x" << std::hex
                                    << its_client << " : routing_manager_stub::on_message: "
                                    << "requests service/instance "
                                    << its_service << "/" << its_instance
                                    << " which violates the security policy ~> Skip request!";
                        }
                    }
                    if (security::get()->is_enabled()) {
                        handle_credentials(its_client, requests);
                    }
                    handle_requests(its_client, requests);
                    break;
                }

                case VSOMEIP_RELEASE_SERVICE:
                    if (_size != VSOMEIP_RELEASE_SERVICE_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "Received a RELEASE_SERVICE command with wrong size ~> skip!";
                        break;
                    }
                    std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_service));
                    std::memcpy(&its_instance,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                            sizeof(its_instance));

                    host_->release_service(its_client, its_service, its_instance);
                    break;

                case VSOMEIP_REGISTER_EVENT: {
                    if (_size < VSOMEIP_REGISTER_EVENT_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "Received a REGISTER_EVENT command with wrong size ~> skip!";
                        break;
                    }

                    std::set<eventgroup_t> its_eventgroups;
                    reliability_type_e its_reliability = reliability_type_e::RT_UNKNOWN;

                    std::memcpy(&its_service,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_service));
                    std::memcpy(&its_instance,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                            sizeof(its_instance));
                    std::memcpy(&its_notifier,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                            sizeof(its_notifier));
                    std::memcpy(&its_event_type,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                            sizeof(its_event_type));
                    std::memcpy(&is_provided,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 7],
                            sizeof(is_provided));
                    std::memcpy(&its_reliability,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 8],
                            sizeof(its_reliability));
                    if (is_provided
                            && !configuration_->is_offered_remote(its_service,
                                    its_instance)) {
                        break;
                    }
                    for (std::size_t i = 9; i+1 < its_size; i++) {
                        std::memcpy(&its_eventgroup,
                                &_data[VSOMEIP_COMMAND_PAYLOAD_POS + i],
                                sizeof(its_eventgroup));
                        its_eventgroups.insert(its_eventgroup);
                    }
                    host_->register_shadow_event(its_client,
                            its_service, its_instance,
                            its_notifier, its_eventgroups, its_event_type,
                            its_reliability,
                            is_provided);
                    VSOMEIP_INFO << "REGISTER EVENT("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_notifier
                        << ":is_provider=" << is_provided << ":reliability="
                        << (std::uint32_t)(its_reliability) << "]";
                    break;
                }
                case VSOMEIP_UNREGISTER_EVENT:
                    if (_size != VSOMEIP_UNREGISTER_EVENT_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "Received a UNREGISTER_EVENT command with wrong size ~> skip!";
                        break;
                    }
                    std::memcpy(&its_service, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_service));
                    std::memcpy(&its_instance,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 2],
                            sizeof(its_instance));
                    std::memcpy(&its_notifier, &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 4],
                            sizeof(its_notifier));
                    std::memcpy(&is_provided,
                            &_data[VSOMEIP_COMMAND_PAYLOAD_POS + 6],
                            sizeof(is_provided));
                    if (is_provided
                            && !configuration_->is_offered_remote(its_service,
                                    its_instance)) {
                        break;
                    }
                    host_->unregister_shadow_event(its_client, its_service, its_instance,
                            its_notifier, is_provided);
                    VSOMEIP_INFO << "UNREGISTER EVENT("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << "): ["
                        << std::hex << std::setw(4) << std::setfill('0') << its_service << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_instance << "."
                        << std::hex << std::setw(4) << std::setfill('0') << its_notifier
                        << ":is_provider=" << is_provided << "]";
                    break;

                case VSOMEIP_REGISTERED_ACK:
                    if (_size != VSOMEIP_REGISTERED_ACK_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "Received a REGISTERED_ACK command with wrong size ~> skip!";
                        break;
                    }
                    VSOMEIP_INFO << "REGISTERED_ACK("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << ")";
                    break;
                case VSOMEIP_OFFERED_SERVICES_REQUEST: {
                    if (_size != VSOMEIP_OFFERED_SERVICES_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "Received a VSOMEIP_OFFERED_SERVICES_REQUEST command with wrong size ~> skip!";
                        break;
                    }

                    std::memcpy(&its_offer_type, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(its_offer_type));

                    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
                    create_offered_services_info(its_client);

                    for (const auto& found_client : routing_info_) {
                        // skip services which are offered on remote hosts
                        if (found_client.first != VSOMEIP_ROUTING_CLIENT) {
                            for (const auto &its_service : found_client.second.second) {
                                for (const auto &its_instance : its_service.second) {
                                    uint16_t its_reliable_port = configuration_->get_reliable_port(its_service.first,
                                            its_instance.first);
                                    uint16_t its_unreliable_port = configuration_->get_unreliable_port(
                                            its_service.first, its_instance.first);

                                    if (its_offer_type == offer_type_e::OT_LOCAL) {
                                        if (its_reliable_port == ILLEGAL_PORT
                                                && its_unreliable_port == ILLEGAL_PORT) {
                                            insert_offered_services_info(its_client,
                                                    routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE,
                                                    its_service.first, its_instance.first,
                                                    its_instance.second.first, its_instance.second.second);
                                        }
                                    }
                                    else if (its_offer_type == offer_type_e::OT_REMOTE) {
                                        if (its_reliable_port != ILLEGAL_PORT
                                                || its_unreliable_port != ILLEGAL_PORT) {
                                            insert_offered_services_info(its_client,
                                                    routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE,
                                                    its_service.first, its_instance.first,
                                                    its_instance.second.first, its_instance.second.second);
                                        }
                                    } else if (its_offer_type == offer_type_e::OT_ALL) {
                                        insert_offered_services_info(its_client,
                                                routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE,
                                                its_service.first, its_instance.first,
                                                its_instance.second.first, its_instance.second.second);
                                    }
                                }
                            }
                        }
                    }
                    send_offered_services_info(its_client);
                    break;
                }
                case VSOMEIP_RESEND_PROVIDED_EVENTS: {
                    if (_size != VSOMEIP_RESEND_PROVIDED_EVENTS_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "Received a RESEND_PROVIDED_EVENTS command with wrong size ~> skip!";
                        break;
                    }
                    pending_remote_offer_id_t its_pending_remote_offer_id(0);
                    std::memcpy(&its_pending_remote_offer_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(pending_remote_offer_id_t));
                    host_->on_resend_provided_events_response(its_pending_remote_offer_id);
                    VSOMEIP_INFO << "RESEND_PROVIDED_EVENTS("
                        << std::hex << std::setw(4) << std::setfill('0') << its_client << ")";
                    break;
                }
                case VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE: {
                    if (_size != VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "vSomeIP Security: Received a VSOMEIP_UPDATE_SECURITY_POLICY_RESPONSE "
                                << "command with wrong size ~> skip!";
                        break;
                    }
                    pending_security_update_id_t its_pending_security_update_id(0);
                    std::memcpy(&its_pending_security_update_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(pending_security_update_id_t));

                    on_security_update_response(its_pending_security_update_id ,its_client);
                    break;
                }
                case VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE: {
                    if (_size != VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE_COMMAND_SIZE) {
                        VSOMEIP_WARNING << "vSomeIP Security: Received a VSOMEIP_REMOVE_SECURITY_POLICY_RESPONSE "
                                << "command with wrong size ~> skip!";
                        break;
                    }
                    pending_security_update_id_t its_pending_security_update_id(0);
                    std::memcpy(&its_pending_security_update_id, &_data[VSOMEIP_COMMAND_PAYLOAD_POS],
                            sizeof(pending_security_update_id_t));

                    on_security_update_response(its_pending_security_update_id ,its_client);
                    break;
                }
            }
        }
    }
}

void routing_manager_stub::on_register_application(client_t _client) {
    auto endpoint = host_->find_local(_client);
    if (endpoint) {
        VSOMEIP_WARNING << "Reregistering application: " << std::hex << _client
                << ". Last registration might have been taken too long.";
    } else {
        endpoint = host_->find_or_create_local(_client);
        {
            std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
            routing_info_[_client].first = 0;
        }

        std::pair<uid_t, gid_t> its_uid_gid;
        std::set<std::shared_ptr<policy> > its_policies;

        security::get()->get_client_to_uid_gid_mapping(_client, its_uid_gid);
        get_requester_policies(its_uid_gid.first, its_uid_gid.second, its_policies);

        if (!its_policies.empty())
            send_requester_policies({ _client }, its_policies);
    }
}

void routing_manager_stub::on_deregister_application(client_t _client) {
    std::vector<
            std::tuple<service_t, instance_t,
                       major_version_t, minor_version_t>> services_to_report;
    {
        std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
        auto its_info = routing_info_.find(_client);
        if (its_info != routing_info_.end()) {
            for (const auto &its_service : its_info->second.second) {
                for (const auto &its_instance : its_service.second) {
                    const auto its_version = its_instance.second;
                    services_to_report.push_back(
                            std::make_tuple(its_service.first,
                                    its_instance.first, its_version.first,
                                    its_version.second));
                }
            }
        }
        routing_info_.erase(_client);
    }
    for (const auto &s : services_to_report) {
        host_->on_availability(std::get<0>(s), std::get<1>(s), false,
                std::get<2>(s), std::get<3>(s));
        host_->on_stop_offer_service(_client, std::get<0>(s), std::get<1>(s),
                std::get<2>(s), std::get<3>(s));
    }
}

void routing_manager_stub::client_registration_func(void) {
#ifndef _WIN32
    {
        std::stringstream s;
        s << std::hex << std::setw(4) << std::setfill('0')
            << host_->get_client() << "_client_reg";
        pthread_setname_np(pthread_self(),s.str().c_str());
    }
#endif
    std::unique_lock<std::mutex> its_lock(client_registration_mutex_);
    while (client_registration_running_) {
        while (!pending_client_registrations_.size() && client_registration_running_) {
            client_registration_condition_.wait(its_lock);
        }

        std::map<client_t, std::vector<registration_type_e>> its_registrations(
                pending_client_registrations_);
        pending_client_registrations_.clear();
        its_lock.unlock();

        for (const auto& r : its_registrations) {
            for (auto b : r.second) {
                if (b == registration_type_e::REGISTER) {
                    on_register_application(r.first);
                } else {
                    on_deregister_application(r.first);
                }
                // Inform (de)registered client. All others will be informed after
                // the client acknowledged its registered state!
                // Don't inform client if we deregister because of an client
                // endpoint error to avoid writing in an already closed socket
                if (b != registration_type_e::DEREGISTER_ON_ERROR) {
                    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
                    create_client_routing_info(r.first);
                    insert_client_routing_info(r.first,
                            b == registration_type_e::REGISTER ?
                            routing_info_entry_e::RIE_ADD_CLIENT :
                            routing_info_entry_e::RIE_DEL_CLIENT,
                            r.first);
                    // distribute updated security config to new clients
                    if (b == registration_type_e::REGISTER) {
                        send_cached_security_policies(r.first);
                    }
                    send_client_routing_info(r.first);
                }
                if (b != registration_type_e::REGISTER) {
                    {
                        std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
                        auto its_connection = connection_matrix_.find(r.first);
                        if (its_connection != connection_matrix_.end()) {
                            for (auto its_client : its_connection->second) {
                                if (its_client != r.first &&
                                        its_client != VSOMEIP_ROUTING_CLIENT &&
                                        its_client != get_client()) {
                                    create_client_routing_info(its_client);
                                    insert_client_routing_info(its_client,
                                            routing_info_entry_e::RIE_DEL_CLIENT, r.first);
                                    send_client_routing_info(its_client);
                                }
                            }
                            connection_matrix_.erase(r.first);
                        }
                        for (const auto& its_client : connection_matrix_) {
                            connection_matrix_[its_client.first].erase(r.first);
                        }
                        service_requests_.erase(r.first);
                    }
                    // Don't remove client ID to UID maping as same client
                    // could have passed its credentials again
                    host_->remove_local(r.first, false);
                    utility::release_client_id(r.first);
                }
            }
        }
        its_lock.lock();
    }
}

void routing_manager_stub::init_routing_endpoint() {
    endpoint_ = host_->get_endpoint_manager()->create_local_server(
            &is_socket_activated_, shared_from_this());
}

void routing_manager_stub::on_offer_service(client_t _client,
        service_t _service, instance_t _instance, major_version_t _major, minor_version_t _minor) {
    if (_client == host_->get_client()) {
        create_local_receiver();
    }

    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    routing_info_[_client].second[_service][_instance] = std::make_pair(_major, _minor);
    if (security::get()->is_enabled()) {
        distribute_credentials(_client, _service, _instance);
    }
    inform_requesters(_client, _service, _instance, _major, _minor,
            routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE, true);
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
                    inform_requesters(_client, _service, _instance, _major, _minor,
                            routing_info_entry_e::RIE_DEL_SERVICE_INSTANCE, false);
                } else if( _major == DEFAULT_MAJOR && _minor == DEFAULT_MINOR) {
                    found_service->second.erase(_instance);
                    if (0 == found_service->second.size()) {
                        found_client->second.second.erase(_service);
                    }
                    inform_requesters(_client, _service, _instance, _major, _minor,
                            routing_info_entry_e::RIE_DEL_SERVICE_INSTANCE, false);
                }
            }
        }
    }
}

void routing_manager_stub::create_client_routing_info(const client_t _target) {
    std::vector<byte_t> its_command;
    its_command.push_back(VSOMEIP_ROUTING_INFO);

    // Sender client
    client_t client = get_client();
    for (uint32_t i = 0; i < sizeof(client_t); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&client)[i]);
    }

    // Overall size placeholder
    byte_t size_placeholder = 0x0;
    for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
        its_command.push_back(size_placeholder);
    }

    client_routing_info_[_target] = its_command;
}

void routing_manager_stub::create_client_credentials_info(const client_t _target) {
    std::vector<byte_t> its_command;
    its_command.push_back(VSOMEIP_UPDATE_SECURITY_CREDENTIALS);

    // Sender client
    client_t client = get_client();
    for (uint32_t i = 0; i < sizeof(client_t); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&client)[i]);
    }

    // Overall size placeholder
    byte_t size_placeholder = 0x0;
    for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
        its_command.push_back(size_placeholder);
    }

    client_credentials_info_[_target] = its_command;
}

void routing_manager_stub::insert_client_credentials_info(client_t _target, std::set<std::pair<uint32_t, uint32_t>> _credentials) {
    if (client_credentials_info_.find(_target) == client_credentials_info_.end()) {
        return;
    }

    auto its_command = client_credentials_info_[_target];

    // insert uid / gid credential pairs
    for (auto its_credentials : _credentials) {
        //uid
        for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&std::get<0>(its_credentials))[i]);
        }
        //gid
        for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&std::get<1>(its_credentials))[i]);
        }
    }

    client_credentials_info_[_target] = its_command;
}

void routing_manager_stub::send_client_credentials_info(const client_t _target) {
    if (client_credentials_info_.find(_target) == client_credentials_info_.end()) {
        return;
    }

    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_target);
    if (its_endpoint) {
        auto its_command = client_credentials_info_[_target];

        // File overall size
        std::size_t its_size = its_command.size() - VSOMEIP_COMMAND_PAYLOAD_POS;
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size, sizeof(uint32_t));
        its_size += VSOMEIP_COMMAND_PAYLOAD_POS;

#if 0
        std::stringstream msg;
        msg << "rms::send_credentials_info to (" << std::hex << _target << "): ";
        for (uint32_t i = 0; i < its_size; ++i)
            msg << std::hex << std::setw(2) << std::setfill('0') << (int)its_command[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif

        // Send routing info or error!
        if(its_command.size() <= max_local_message_size_
                || VSOMEIP_MAX_LOCAL_MESSAGE_SIZE == 0) {
            its_endpoint->send(&its_command[0], uint32_t(its_size));
        } else {
            VSOMEIP_ERROR << "Credentials info exceeds maximum message size: Can't send!";
        }

        client_credentials_info_.erase(_target);
    } else {
        VSOMEIP_ERROR << "Send credentials info to client 0x" << std::hex << _target
                << " failed: No valid endpoint!";
    }
}

void routing_manager_stub::create_offered_services_info(const client_t _target) {
    std::vector<byte_t> its_command;
    its_command.push_back(VSOMEIP_OFFERED_SERVICES_RESPONSE);

    // Sender client
    client_t client = get_client();
    for (uint32_t i = 0; i < sizeof(client_t); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&client)[i]);
    }

    // Overall size placeholder
    byte_t size_placeholder = 0x0;
    for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
        its_command.push_back(size_placeholder);
    }

    offered_services_info_[_target] = its_command;
}


void routing_manager_stub::send_client_routing_info(const client_t _target) {
    if (client_routing_info_.find(_target) == client_routing_info_.end()) {
        return;
    }
    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_target);
    if (its_endpoint) {
        auto its_command = client_routing_info_[_target];

        // File overall size
        std::size_t its_size = its_command.size() - VSOMEIP_COMMAND_PAYLOAD_POS;
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size, sizeof(uint32_t));
        its_size += VSOMEIP_COMMAND_PAYLOAD_POS;

#if 0
        std::stringstream msg;
        msg << "rms::send_routing_info to (" << std::hex << _target << "): ";
        for (uint32_t i = 0; i < its_size; ++i)
            msg << std::hex << std::setw(2) << std::setfill('0') << (int)its_command[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif

        // Send routing info or error!
        if(its_command.size() <= max_local_message_size_
                || VSOMEIP_MAX_LOCAL_MESSAGE_SIZE == 0) {
            its_endpoint->send(&its_command[0], uint32_t(its_size));
        } else {
            VSOMEIP_ERROR << "Routing info exceeds maximum message size: Can't send!";
        }

        client_routing_info_.erase(_target);
    } else {
        VSOMEIP_ERROR << "Send routing info to client 0x" << std::hex << _target
                << " failed: No valid endpoint!";
    }
}


void routing_manager_stub::send_offered_services_info(const client_t _target) {
    if (offered_services_info_.find(_target) == offered_services_info_.end()) {
        return;
    }
    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_target);
    if (its_endpoint) {
        auto its_command = offered_services_info_[_target];

        // File overall size
        std::size_t its_size = its_command.size() - VSOMEIP_COMMAND_PAYLOAD_POS;
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size, sizeof(uint32_t));
        its_size += VSOMEIP_COMMAND_PAYLOAD_POS;

#if 0
        std::stringstream msg;
        msg << "rms::send_offered_services_info to (" << std::hex << _target << "): ";
        for (uint32_t i = 0; i < its_size; ++i)
            msg << std::hex << std::setw(2) << std::setfill('0') << (int)its_command[i] << " ";
        VSOMEIP_INFO << msg.str();
#endif

        // Send routing info or error!
        if(its_command.size() <= max_local_message_size_
                || VSOMEIP_MAX_LOCAL_MESSAGE_SIZE == 0) {
            its_endpoint->send(&its_command[0], uint32_t(its_size));
        } else {
            VSOMEIP_ERROR << "Offered services info exceeds maximum message size: Can't send!";
        }

        offered_services_info_.erase(_target);
    } else {
        VSOMEIP_ERROR << "Send offered services info to client 0x" << std::hex << _target
                << " failed: No valid endpoint!";
    }
}

void routing_manager_stub::insert_client_routing_info(client_t _target,
        routing_info_entry_e _entry,
        client_t _client, service_t _service,
        instance_t _instance,
        major_version_t _major,
        minor_version_t _minor) {

    if (client_routing_info_.find(_target) == client_routing_info_.end()) {
        return;
    }

    connection_matrix_[_target].insert(_client);

    auto its_command = client_routing_info_[_target];

    // Routing Info State Change
    for (uint32_t i = 0; i < sizeof(routing_info_entry_e); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&_entry)[i]);
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
                 reinterpret_cast<const byte_t*>(&_client)[i]);
    }

    if (_entry == routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE ||
            _entry == routing_info_entry_e::RIE_DEL_SERVICE_INSTANCE) {
        //Service
        uint32_t its_service_entry_size = uint32_t(sizeof(service_t)
                + sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t));
        for (uint32_t i = 0; i < sizeof(its_service_entry_size); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&its_service_entry_size)[i]);
        }
        for (uint32_t i = 0; i < sizeof(service_t); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&_service)[i]);
        }
        // Instance
        for (uint32_t i = 0; i < sizeof(instance_t); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&_instance)[i]);
        }
        // Major version
        for (uint32_t i = 0; i < sizeof(major_version_t); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&_major)[i]);
        }
        // Minor version
        for (uint32_t i = 0; i < sizeof(minor_version_t); ++i) {
            its_command.push_back(
                    reinterpret_cast<const byte_t*>(&_minor)[i]);
        }
    }

    // File client size
    its_entry_size = its_command.size() - its_entry_size - uint32_t(sizeof(uint32_t));
    std::memcpy(&its_command[its_size_pos], &its_entry_size, sizeof(uint32_t));

    client_routing_info_[_target] = its_command;
}

void routing_manager_stub::insert_offered_services_info(client_t _target,
        routing_info_entry_e _entry,
        service_t _service,
        instance_t _instance,
        major_version_t _major,
        minor_version_t _minor) {

    if (offered_services_info_.find(_target) == offered_services_info_.end()) {
        return;
    }

    auto its_command = offered_services_info_[_target];

    // Routing Info State Change
    for (uint32_t i = 0; i < sizeof(routing_info_entry_e); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&_entry)[i]);
    }

    // entry size
    uint32_t its_service_entry_size = uint32_t(sizeof(service_t)
            + sizeof(instance_t) + sizeof(major_version_t) + sizeof(minor_version_t));
    for (uint32_t i = 0; i < sizeof(its_service_entry_size); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&its_service_entry_size)[i]);
    }
    //Service
    for (uint32_t i = 0; i < sizeof(service_t); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&_service)[i]);
    }
    // Instance
    for (uint32_t i = 0; i < sizeof(instance_t); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&_instance)[i]);
    }
    // Major version
    for (uint32_t i = 0; i < sizeof(major_version_t); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&_major)[i]);
    }
    // Minor version
    for (uint32_t i = 0; i < sizeof(minor_version_t); ++i) {
        its_command.push_back(
                reinterpret_cast<const byte_t*>(&_minor)[i]);
    }

    offered_services_info_[_target] = its_command;
}

void routing_manager_stub::distribute_credentials(client_t _hoster, service_t _service, instance_t _instance) {
    std::set<std::pair<uint32_t, uint32_t>> its_credentials;
    std::set<client_t> its_requesting_clients;
    // search for clients which shall receive the credentials
    for (auto its_requesting_client : service_requests_) {
        auto its_service = its_requesting_client.second.find(_service);
        if (its_service != its_requesting_client.second.end()) {
            for (auto its_instance : its_service->second) {
                if (its_instance.first == ANY_INSTANCE ||
                        its_instance.first == _instance) {
                    its_requesting_clients.insert(its_requesting_client.first);
                } else {
                    auto found_instance = its_service->second.find(_instance);
                    if (found_instance != its_service->second.end()) {
                        its_requesting_clients.insert(its_requesting_client.first);
                    }
                }
            }
        }
    }

    // search for UID / GID linked with the client ID that offers the requested services
    std::pair<uint32_t, uint32_t> its_uid_gid;
    if (security::get()->get_client_to_uid_gid_mapping(_hoster, its_uid_gid)) {
        for (auto its_requesting_client : its_requesting_clients) {
            std::pair<uint32_t, uint32_t> its_requester_uid_gid;
            if (security::get()->get_client_to_uid_gid_mapping(its_requesting_client, its_requester_uid_gid)) {
                if (its_uid_gid != its_requester_uid_gid) {
                    its_credentials.insert(std::make_pair(std::get<0>(its_uid_gid), std::get<1>(its_uid_gid)));
                    create_client_credentials_info(its_requesting_client);
                    insert_client_credentials_info(its_requesting_client, its_credentials);
                    send_client_credentials_info(its_requesting_client);
                }
            }
        }
    }
}

void routing_manager_stub::inform_requesters(client_t _hoster, service_t _service,
        instance_t _instance, major_version_t _major, minor_version_t _minor,
        routing_info_entry_e _entry, bool _inform_service) {
    for (auto its_client : service_requests_) {
        auto its_service = its_client.second.find(_service);
        if (its_service != its_client.second.end()) {
            bool send(false);
            for (auto its_instance : its_service->second) {
                if (its_instance.first == ANY_INSTANCE ||
                        its_instance.first == _instance) {
                    send = true;
                }
            }
            if (send) {
                if (_inform_service) {
                    if (_hoster != VSOMEIP_ROUTING_CLIENT &&
                            _hoster != host_->get_client()) {
                        if (!is_already_connected(_hoster, its_client.first)) {
                            create_client_routing_info(_hoster);
                            insert_client_routing_info(_hoster,
                                    routing_info_entry_e::RIE_ADD_CLIENT,
                                    its_client.first);
                            send_client_routing_info(_hoster);
                        }
                    }
                }
                if (its_client.first != VSOMEIP_ROUTING_CLIENT &&
                        its_client.first != get_client()) {
                    create_client_routing_info(its_client.first);
                    insert_client_routing_info(its_client.first, _entry, _hoster,
                            _service, _instance, _major, _minor);
                    send_client_routing_info(its_client.first);
                }
            }
        }
    }
}

bool routing_manager_stub::is_already_connected(client_t _source, client_t _sink) {
    return connection_matrix_[_source].find(_sink) != connection_matrix_[_source].end();
}

void routing_manager_stub::broadcast(const std::vector<byte_t> &_command) const {
    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    for (const auto& a : routing_info_) {
        if (a.first != VSOMEIP_ROUTING_CLIENT && a.first != host_->get_client()) {
            std::shared_ptr<endpoint> its_endpoint
                = host_->find_local(a.first);
            if (its_endpoint) {
                its_endpoint->send(&_command[0], uint32_t(_command.size()));
            }
        }
    }
}

bool routing_manager_stub::send_subscribe(const std::shared_ptr<endpoint>& _target,
        client_t _client, service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, major_version_t _major,
        event_t _event, remote_subscription_id_t _id) {
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
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 7], &_event,
                sizeof(_event));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 9], &_id,
                sizeof(_id));

        return _target->send(its_command, sizeof(its_command));
    } else {
        VSOMEIP_WARNING << __func__ << " Couldn't send subscription to local client ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "."
                << std::hex << std::setw(4) << std::setfill('0') << _event << "]"
                << " subscriber: "<< std::hex << std::setw(4) << std::setfill('0')
                << _client;
        return false;
    }
}

bool routing_manager_stub::send_unsubscribe(
        const std::shared_ptr<endpoint>& _target,
        client_t _client, service_t _service, instance_t _instance,
        eventgroup_t _eventgroup, event_t _event,
        remote_subscription_id_t _id) {
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
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &_event,
                sizeof(_event));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8], &_id,
                sizeof(_id));

        return _target->send(its_command, sizeof(its_command));
    } else {
        VSOMEIP_WARNING << __func__ << " Couldn't send unsubscription to local client ["
                << std::hex << std::setw(4) << std::setfill('0') << _service << "."
                << std::hex << std::setw(4) << std::setfill('0') << _instance << "."
                << std::hex << std::setw(4) << std::setfill('0') << _eventgroup << "."
                << std::hex << std::setw(4) << std::setfill('0') << _event << "]"
                << " subscriber: "<< std::hex << std::setw(4) << std::setfill('0')
                << _client;
        return false;
    }
}

void routing_manager_stub::send_subscribe_ack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event) {

    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        byte_t its_command[VSOMEIP_SUBSCRIBE_ACK_COMMAND_SIZE];
        uint32_t its_size = VSOMEIP_SUBSCRIBE_ACK_COMMAND_SIZE
                - VSOMEIP_COMMAND_HEADER_SIZE;

        client_t this_client = get_client();
        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE_ACK;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &this_client,
                sizeof(this_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                sizeof(_eventgroup));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &_client,
                sizeof(_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8], &_event,
                sizeof(_event));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 10],
                &PENDING_SUBSCRIPTION_ID, sizeof(PENDING_SUBSCRIPTION_ID));

        its_endpoint->send(&its_command[0], sizeof(its_command));
    }
}

void routing_manager_stub::send_subscribe_nack(client_t _client, service_t _service,
            instance_t _instance, eventgroup_t _eventgroup, event_t _event) {

    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        byte_t its_command[VSOMEIP_SUBSCRIBE_NACK_COMMAND_SIZE];
        uint32_t its_size = VSOMEIP_SUBSCRIBE_NACK_COMMAND_SIZE
                - VSOMEIP_COMMAND_HEADER_SIZE;

        client_t this_client = get_client();
        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_SUBSCRIBE_NACK;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &this_client,
                sizeof(this_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_service,
                sizeof(_service));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 2], &_instance,
                sizeof(_instance));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_eventgroup,
                sizeof(_eventgroup));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 6], &_client,
                sizeof(_client));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8], &_event,
                sizeof(_event));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 10],
                &PENDING_SUBSCRIPTION_ID, sizeof(PENDING_SUBSCRIPTION_ID));

        its_endpoint->send(&its_command[0], sizeof(its_command));
    }
}

bool routing_manager_stub::contained_in_routing_info(
        client_t _client, service_t _service, instance_t _instance,
        major_version_t _major, minor_version_t _minor) const {
    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    auto found_client = routing_info_.find(_client);
    if (found_client != routing_info_.end()) {
        auto found_service = found_client->second.second.find(_service);
        if (found_service != found_client->second.second.end()) {
            auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                if (found_instance->second.first == _major
                        && found_instance->second.second == _minor) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Watchdog
void routing_manager_stub::broadcast_ping() const {
    broadcast(its_ping_);
}

void routing_manager_stub::on_pong(client_t _client) {
    {
        std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
        auto found_info = routing_info_.find(_client);
        if (found_info != routing_info_.end()) {
            found_info->second.first = 0;
        } else {
            VSOMEIP_ERROR << "Received PONG from unregistered application: "
                    << std::hex << std::setw(4) << std::setfill('0') << _client;
        }
    }
    remove_from_pinged_clients(_client);
    host_->on_pong(_client);
}

void routing_manager_stub::start_watchdog() {

    auto its_callback =
            [this](boost::system::error_code const &_error) {
                if (!_error)
                    check_watchdog();
            };
    {
        std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
        // Divide / 2 as start and check sleep each
        watchdog_timer_.expires_from_now(
                std::chrono::milliseconds(
                        configuration_->get_watchdog_timeout() / 2));

        watchdog_timer_.async_wait(its_callback);
    }
}

void routing_manager_stub::check_watchdog() {
    {
        std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
        for (auto i = routing_info_.begin(); i != routing_info_.end(); ++i) {
            i->second.first++;
        }
    }
    broadcast_ping();


    auto its_callback = [this](boost::system::error_code const &_error) {
                (void)_error;
                std::list< client_t > lost;
                {
                    std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
                    for (const auto& i : routing_info_) {
                        if (i.first > 0 && i.first != host_->get_client()) {
                            if (i.second.first > configuration_->get_allowed_missing_pongs()) {
                                VSOMEIP_WARNING << "Lost contact to application " << std::hex << (int)i.first;
                                lost.push_back(i.first);
                            }
                        }
                    }
                }
                for (auto i : lost) {
                    host_->handle_client_error(i);
                }
                start_watchdog();
            };
    {
        std::lock_guard<std::mutex> its_lock(watchdog_timer_mutex_);
        watchdog_timer_.expires_from_now(
                std::chrono::milliseconds(
                        configuration_->get_watchdog_timeout() / 2));
        watchdog_timer_.async_wait(its_callback);
    }
}

void routing_manager_stub::create_local_receiver() {
    std::lock_guard<std::mutex> its_lock(local_receiver_mutex_);

    if (local_receiver_) {
        return;
    }
#ifndef _WIN32
    else if (!security::get()->check_credentials(get_client(), getuid(), getgid())) {
        VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << std::hex << get_client()
                << " : routing_manager_stub::create_local_receiver:  isn't allowed"
                << " to create a server endpoint due to credential check failed!";
        return;
    }
#endif
    local_receiver_ = std::static_pointer_cast<endpoint_manager_base>(
            host_->get_endpoint_manager())->create_local_server(shared_from_this());
    local_receiver_->start();
}

bool routing_manager_stub::send_ping(client_t _client) {
    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (!its_endpoint) {
        return false;
    }

    {
        std::lock_guard<std::mutex> its_lock(pinged_clients_mutex_);

        if (pinged_clients_.find(_client) != pinged_clients_.end()) {
            // client was already pinged: don't ping again and wait for answer
            // or timeout of previous ping.
            return true;
        }

        boost::system::error_code ec;
        pinged_clients_timer_.cancel(ec);
        if (ec) {
            VSOMEIP_ERROR << "routing_manager_stub::send_ping cancellation of "
                    "timer failed: " << ec.message();
        }
        const std::chrono::steady_clock::time_point now(
                std::chrono::steady_clock::now());

        std::chrono::milliseconds next_timeout(configured_watchdog_timeout_);
        for (const auto &tp : pinged_clients_) {
            const std::chrono::milliseconds its_clients_timeout =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - tp.second);
            if (next_timeout > its_clients_timeout) {
                next_timeout = its_clients_timeout;
            }
        }

        pinged_clients_[_client] = now;

        ec.clear();
        pinged_clients_timer_.expires_from_now(next_timeout, ec);
        if (ec) {
            VSOMEIP_ERROR<< "routing_manager_stub::send_ping setting "
            "expiry time of timer failed: " << ec.message();
        }
        pinged_clients_timer_.async_wait(
                std::bind(&routing_manager_stub::on_ping_timer_expired, this,
                        std::placeholders::_1));
        return its_endpoint->send(&its_ping_[0], uint32_t(its_ping_.size()));
    }
}

void routing_manager_stub::on_ping_timer_expired(
        boost::system::error_code const &_error) {
    if(_error) {
        return;
    }
    std::forward_list<client_t> timed_out_clients;
    std::chrono::milliseconds next_timeout(configured_watchdog_timeout_);
    bool pinged_clients_remaining(false);

    {
        // remove timed out clients
        std::lock_guard<std::mutex> its_lock(pinged_clients_mutex_);
        const std::chrono::steady_clock::time_point now(
                std::chrono::steady_clock::now());

        for (auto client_iter = pinged_clients_.begin();
                  client_iter != pinged_clients_.end(); ) {
            if ((now - client_iter->second) >= configured_watchdog_timeout_) {
                timed_out_clients.push_front(client_iter->first);
                client_iter = pinged_clients_.erase(client_iter);
            } else {
                ++client_iter;
            }
        }
        pinged_clients_remaining = (pinged_clients_.size() > 0);

        if(pinged_clients_remaining) {
            // find out next timeout
            for (const auto &tp : pinged_clients_) {
                const std::chrono::milliseconds its_clients_timeout =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - tp.second);
                if (next_timeout > its_clients_timeout) {
                    next_timeout = its_clients_timeout;
                }
            }
        }
    }

    for (const client_t client : timed_out_clients) {
        // Client did not respond to ping. Report client_error in order to
        // accept pending offers trying to replace the offers of the client
        // that seems to be gone.
        host_->handle_client_error(client);
    }
    if (pinged_clients_remaining) {
        boost::system::error_code ec;
        pinged_clients_timer_.expires_from_now(next_timeout, ec);
        if (ec) {
            VSOMEIP_ERROR<< "routing_manager_stub::on_ping_timer_expired "
            "setting expiry time of timer failed: " << ec.message();
        }
        pinged_clients_timer_.async_wait(
                std::bind(&routing_manager_stub::on_ping_timer_expired, this,
                        std::placeholders::_1));
    }
}

void routing_manager_stub::remove_from_pinged_clients(client_t _client) {
    std::lock_guard<std::mutex> its_lock(pinged_clients_mutex_);
    if (!pinged_clients_.size()) {
        return;
    }
    boost::system::error_code ec;
    pinged_clients_timer_.cancel(ec);
    if (ec) {
        VSOMEIP_ERROR << "routing_manager_stub::remove_from_pinged_clients "
                "cancellation of timer failed: " << ec.message();
    }
    pinged_clients_.erase(_client);

    if (!pinged_clients_.size()) {
            return;
    }
    const std::chrono::steady_clock::time_point now(
            std::chrono::steady_clock::now());
    std::chrono::milliseconds next_timeout(configured_watchdog_timeout_);
    // find out next timeout
    for (const auto &tp : pinged_clients_) {
        const std::chrono::milliseconds its_clients_timeout =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - tp.second);
        if (next_timeout > its_clients_timeout) {
            next_timeout = its_clients_timeout;
        }
    }
    ec.clear();
    pinged_clients_timer_.expires_from_now(next_timeout, ec);
    if (ec) {
        VSOMEIP_ERROR<< "routing_manager_stub::remove_from_pinged_clients "
        "setting expiry time of timer failed: " << ec.message();
    }
    pinged_clients_timer_.async_wait(
            std::bind(&routing_manager_stub::on_ping_timer_expired, this,
                      std::placeholders::_1));
}

bool routing_manager_stub::is_registered(client_t _client) const {
    std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
    return (routing_info_.find(_client) != routing_info_.end());
}

void routing_manager_stub::update_registration(client_t _client,
        registration_type_e _type) {

    VSOMEIP_INFO << "Application/Client "
        << std::hex << std::setw(4) << std::setfill('0') << _client
        << " is "
        << (_type == registration_type_e::REGISTER ?
                "registering." : "deregistering.");

    if (_type != registration_type_e::REGISTER) {
        security::get()->remove_client_to_uid_gid_mapping(_client);
    }

    if (_type == registration_type_e::DEREGISTER) {
        // If we receive a DEREGISTER client command
        // the endpoint error handler is not longer needed
        // as the client is going down anyways.

        // Normally the handler is removed in "remove_local"
        // anyways, but as some time takes place until
        // the client DEREGISTER command is consumed
        // and therefore "remove_local" is finally called
        // it was possible the same client registers itself
        // again in very short time and then could "overtake"
        // the occurring error in the endpoint and was then
        // erroneously unregistered even that error has
        // nothing to do with the newly registered client.

        auto its_endpoint = host_->find_local(_client);
        if (its_endpoint) {
            its_endpoint->register_error_handler(nullptr);
        }
    }

    std::lock_guard<std::mutex> its_lock(client_registration_mutex_);
    pending_client_registrations_[_client].push_back(_type);
    client_registration_condition_.notify_one();

    if (_type != registration_type_e::REGISTER) {
        std::lock_guard<std::mutex> its_lock(used_client_ids_mutex_);
        used_client_ids_.erase(_client);
    }
}

client_t routing_manager_stub::get_client() const {
    return host_->get_client();
}

void routing_manager_stub::handle_credentials(const client_t _client, std::set<service_data_t>& _requests) {
    if (!_requests.size()) {
        return;
    }

    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    std::set<std::pair<uint32_t, uint32_t>> its_credentials;
    std::pair<uint32_t, uint32_t> its_requester_uid_gid;
    if (security::get()->get_client_to_uid_gid_mapping(_client, its_requester_uid_gid)) {
        // determine credentials of offering clients using current routing info
        std::set<client_t> its_offering_clients;

        // search in local clients for the offering client
        for (auto request : _requests) {
            std::set<client_t> its_clients;
            its_clients = host_->find_local_clients(request.service_, request.instance_);
            for (auto its_client : its_clients) {
                its_offering_clients.insert(its_client);
            }
        }

        // search for UID / GID linked with the client ID that offers the requested services
        for (auto its_offering_client : its_offering_clients) {
            std::pair<uint32_t, uint32_t> its_uid_gid;
            if (security::get()->get_client_to_uid_gid_mapping(its_offering_client, its_uid_gid)) {
                if (its_uid_gid != its_requester_uid_gid) {
                    its_credentials.insert(std::make_pair(std::get<0>(its_uid_gid), std::get<1>(its_uid_gid)));
                }
            }
        }

        // send credentials to clients
        if (!its_credentials.empty()) {
            create_client_credentials_info(_client);
            insert_client_credentials_info(_client, its_credentials);
            send_client_credentials_info(_client);
        }
    }
}

void routing_manager_stub::handle_requests(const client_t _client, std::set<service_data_t>& _requests) {
    if (!_requests.size()) {
        return;
    }
    bool service_available(false);
    std::lock_guard<std::mutex> its_guard(routing_info_mutex_);
    create_client_routing_info(_client);
    for (auto request : _requests) {
        service_requests_[_client][request.service_][request.instance_]
                                                     = std::make_pair(request.major_, request.minor_);
        if (request.instance_ == ANY_INSTANCE) {
            std::set<client_t> its_clients = host_->find_local_clients(request.service_, request.instance_);
            // insert VSOMEIP_ROUTING_CLIENT to check wether service is remotely offered
            its_clients.insert(VSOMEIP_ROUTING_CLIENT);
            for (const client_t c : its_clients) {
                if (c != VSOMEIP_ROUTING_CLIENT &&
                        c != host_->get_client()) {
                    if (!is_already_connected(c, _client)) {
                        if (_client == c) {
                            service_available = true;
                            insert_client_routing_info(c,
                                routing_info_entry_e::RIE_ADD_CLIENT, _client);
                        } else {
                            create_client_routing_info(c);
                            insert_client_routing_info(c,
                                    routing_info_entry_e::RIE_ADD_CLIENT, _client);
                            send_client_routing_info(c);
                        }
                    }
                }
                if (_client != VSOMEIP_ROUTING_CLIENT &&
                        _client != host_->get_client()) {
                    const auto found_client = routing_info_.find(c);
                    if (found_client != routing_info_.end()) {
                        const auto found_service = found_client->second.second.find(request.service_);
                        if (found_service != found_client->second.second.end()) {
                            for (auto instance : found_service->second) {
                                service_available = true;
                                insert_client_routing_info(_client,
                                        routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE,
                                        c, request.service_, instance.first,
                                        instance.second.first, instance.second.second);
                            }
                        }
                    }
                }
            }
        } else {
            const client_t c = host_->find_local_client(request.service_, request.instance_);
            const auto found_client = routing_info_.find(c);
            if (found_client != routing_info_.end()) {
                const auto found_service = found_client->second.second.find(request.service_);
                if (found_service != found_client->second.second.end()) {
                    const auto found_instance = found_service->second.find(request.instance_);
                    if (found_instance != found_service->second.end()) {
                        if (c != VSOMEIP_ROUTING_CLIENT &&
                                c != host_->get_client()) {
                            if (!is_already_connected(c, _client)) {
                                if (_client == c) {
                                    service_available = true;
                                    insert_client_routing_info(c,
                                            routing_info_entry_e::RIE_ADD_CLIENT, _client);
                                } else {
                                    create_client_routing_info(c);
                                    insert_client_routing_info(c,
                                        routing_info_entry_e::RIE_ADD_CLIENT, _client);
                                    send_client_routing_info(c);
                                }
                            }
                        }
                        if (_client != VSOMEIP_ROUTING_CLIENT &&
                                _client != host_->get_client()) {
                            service_available = true;
                            insert_client_routing_info(_client,
                                    routing_info_entry_e::RIE_ADD_SERVICE_INSTANCE,
                                    c, request.service_, request.instance_,
                                    found_instance->second.first,
                                    found_instance->second.second);
                        }
                    }
                }
            }
        }
    }
    if (service_available) {
        send_client_routing_info(_client);
    }
}

void routing_manager_stub::on_client_id_timer_expired(boost::system::error_code const &_error) {
    std::set<client_t> used_client_ids;
    {
        std::lock_guard<std::mutex> its_lock(used_client_ids_mutex_);
        used_client_ids = used_client_ids_;
        used_client_ids_.clear();
    }

    std::set<client_t> erroneous_clients;
    if (!_error) {
        std::lock_guard<std::mutex> its_lock(routing_info_mutex_);
        for (auto client : used_client_ids) {
            if (client != VSOMEIP_ROUTING_CLIENT && client != get_client()) {
                if (routing_info_.find(client) == routing_info_.end()) {
                    erroneous_clients.insert(client);
                }
            }
        }
    }
    for (auto client : erroneous_clients) {
        VSOMEIP_WARNING << "Releasing client identifier "
                << std::hex << std::setw(4) << std::setfill('0') << client << ". "
                << "Its corresponding application went offline while no "
                << "routing manager was running.";
        host_->handle_client_error(client);
    }
}

void routing_manager_stub::print_endpoint_status() const {
    if (local_receiver_) {
        local_receiver_->print_status();
    }
    if (endpoint_) {
        endpoint_->print_status();
    }
}

bool routing_manager_stub::send_provided_event_resend_request(client_t _client,
                                                              pending_remote_offer_id_t _id) {
    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        byte_t its_command[VSOMEIP_RESEND_PROVIDED_EVENTS_COMMAND_SIZE];
        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_RESEND_PROVIDED_EVENTS;
        const client_t routing_client(VSOMEIP_ROUTING_CLIENT);
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &routing_client,
                sizeof(routing_client));
        std::uint32_t its_size = sizeof(pending_remote_offer_id_t);
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_id,
                        sizeof(pending_remote_offer_id_t));
        return its_endpoint->send(its_command, sizeof(its_command));
    } else {
        VSOMEIP_WARNING << __func__ << " Couldn't send provided event resend "
                "request to local client: 0x"
                << std::hex << std::setw(4) << std::setfill('0') << _client;
        return false;
    }
}

bool routing_manager_stub::is_policy_cached(uint32_t _uid) {
    {
        std::lock_guard<std::mutex> its_lock(updated_security_policies_mutex_);
        if (updated_security_policies_.find(_uid)
                != updated_security_policies_.end()) {
            VSOMEIP_INFO << __func__ << " Policy for UID: " << std::dec
                    << _uid << " was already updated before!";
            return true;
        } else {
            return false;
        }
    }
}

void routing_manager_stub::policy_cache_add(uint32_t _uid, const std::shared_ptr<payload>& _payload) {
    // cache security policy payload for later distribution to new registering clients
    {
        std::lock_guard<std::mutex> its_lock(updated_security_policies_mutex_);
        updated_security_policies_[_uid] = _payload;
    }
}

void routing_manager_stub::policy_cache_remove(uint32_t _uid) {
    {
        std::lock_guard<std::mutex> its_lock(updated_security_policies_mutex_);
        updated_security_policies_.erase(_uid);
    }
}

bool routing_manager_stub::send_update_security_policy_request(client_t _client, pending_security_update_id_t _update_id,
                                                               uint32_t _uid, const std::shared_ptr<payload>& _payload) {
    (void)_uid;
    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        std::vector<byte_t> its_command;
        // command
        its_command.push_back(VSOMEIP_UPDATE_SECURITY_POLICY);

        // client ID
        for (uint32_t i = 0; i < sizeof(client_t); ++i) {
             its_command.push_back(
                     reinterpret_cast<const byte_t*>(&_client)[i]);
        }
        // security update id length + payload length including gid and uid
        std::uint32_t its_size = uint32_t(sizeof(pending_security_update_id_t) + _payload->get_length());
        for (uint32_t i = 0; i < sizeof(its_size); ++i) {
             its_command.push_back(
                     reinterpret_cast<const byte_t*>(&its_size)[i]);
        }
        // ID of update request
        for (uint32_t i = 0; i < sizeof(pending_security_update_id_t); ++i) {
             its_command.push_back(
                     reinterpret_cast<const byte_t*>(&_update_id)[i]);
        }
        // payload
        for (uint32_t i = 0; i < _payload->get_length(); ++i) {
             its_command.push_back(_payload->get_data()[i]);
        }

        return its_endpoint->send(its_command.data(), uint32_t(its_command.size()));
    } else {
        return false;
    }
}

bool routing_manager_stub::send_cached_security_policies(client_t _client) {
    std::vector<byte_t> its_command;
    std::size_t its_size(0);

    std::lock_guard<std::mutex> its_lock(updated_security_policies_mutex_);
    uint32_t its_policy_count = uint32_t(updated_security_policies_.size());

    if (!its_policy_count) {
        return true;
    }

    VSOMEIP_INFO << __func__ << " Distributing ["
            << std::dec << its_policy_count
            << "] security policy updates to registering client: "
            << std::hex << _client;

    // command
    its_command.push_back(VSOMEIP_DISTRIBUTE_SECURITY_POLICIES);

    // client ID
    client_t its_client = get_client();
    for (uint32_t i = 0; i < sizeof(client_t); ++i) {
         its_command.push_back(
                 reinterpret_cast<const byte_t*>(&its_client)[i]);
    }

    //overall size (placeholder
    for (uint32_t i = 0; i < sizeof(uint32_t); ++i) {
         its_command.push_back(0x00);
    }

    // number of policies contained in message
    for (uint32_t i = 0; i < sizeof(its_policy_count); ++i) {
         its_command.push_back(
                 reinterpret_cast<const byte_t*>(&its_policy_count)[i]);
    }

    for (const auto& its_uid_gid : updated_security_policies_) {
        // policy payload length including gid and uid
        std::uint32_t its_length = uint32_t(its_uid_gid.second->get_length());
        for (uint32_t i = 0; i < sizeof(its_length); ++i) {
             its_command.push_back(
                     reinterpret_cast<const byte_t*>(&its_length)[i]);
        }
        // payload
        its_command.insert(its_command.end(), its_uid_gid.second->get_data(),
                its_uid_gid.second->get_data() + its_uid_gid.second->get_length());
    }
    // File overall size
    its_size = its_command.size() - VSOMEIP_COMMAND_PAYLOAD_POS;
    std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size, sizeof(uint32_t));

    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        return its_endpoint->send(its_command.data(), uint32_t(its_command.size()));
    } else {
        VSOMEIP_WARNING << __func__ << " Couldn't send cached security policies "
                " to registering client: 0x"
                << std::hex << std::setw(4) << std::setfill('0') << _client;
        return false;
    }
}

bool routing_manager_stub::send_remove_security_policy_request( client_t _client, pending_security_update_id_t _update_id,
                                                                uint32_t _uid, uint32_t _gid) {
    std::shared_ptr<endpoint> its_endpoint = host_->find_local(_client);
    if (its_endpoint) {
        byte_t its_command[VSOMEIP_REMOVE_SECURITY_POLICY_COMMAND_SIZE];
        its_command[VSOMEIP_COMMAND_TYPE_POS] = VSOMEIP_REMOVE_SECURITY_POLICY;
        std::memcpy(&its_command[VSOMEIP_COMMAND_CLIENT_POS], &_client,
                sizeof(client_t));
        std::uint32_t its_size = sizeof(_update_id) + sizeof(_uid) + sizeof(_gid);
        std::memcpy(&its_command[VSOMEIP_COMMAND_SIZE_POS_MIN], &its_size,
                sizeof(its_size));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS], &_update_id,
                        sizeof(uint32_t));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 4], &_uid,
                        sizeof(uint32_t));
        std::memcpy(&its_command[VSOMEIP_COMMAND_PAYLOAD_POS + 8], &_gid,
                        sizeof(uint32_t));
        return its_endpoint->send(its_command, sizeof(its_command));
    } else {
        return false;
    }
}

bool
routing_manager_stub::add_requester_policies(uid_t _uid, gid_t _gid,
        const std::set<std::shared_ptr<policy> > &_policies) {

    std::lock_guard<std::mutex> its_lock(requester_policies_mutex_);
    auto found_uid = requester_policies_.find(_uid);
    if (found_uid != requester_policies_.end()) {
        auto found_gid = found_uid->second.find(_gid);
        if (found_gid != found_uid->second.end()) {
            found_gid->second.insert(_policies.begin(), _policies.end());
        } else {
            found_uid->second.insert(std::make_pair(_gid, _policies));
        }
    } else {
        requester_policies_[_uid][_gid] = _policies;
    }

    // Check whether clients with uid/gid are already registered.
    // If yes, update their policy
    std::unordered_set<client_t> its_clients;
    security::get()->get_clients(_uid, _gid, its_clients);

    if (!its_clients.empty())
        return send_requester_policies(its_clients, _policies);

    return (true);
}

void
routing_manager_stub::remove_requester_policies(uid_t _uid, gid_t _gid) {

    std::lock_guard<std::mutex> its_lock(requester_policies_mutex_);
    auto found_uid = requester_policies_.find(_uid);
    if (found_uid != requester_policies_.end()) {
        found_uid->second.erase(_gid);
        if (found_uid->second.empty())
            requester_policies_.erase(_uid);
    }
}

void
routing_manager_stub::get_requester_policies(uid_t _uid, gid_t _gid,
        std::set<std::shared_ptr<policy> > &_policies) const {

    std::lock_guard<std::mutex> its_lock(requester_policies_mutex_);
    auto found_uid = requester_policies_.find(_uid);
    if (found_uid != requester_policies_.end()) {
        auto found_gid = found_uid->second.find(_gid);
        if (found_gid != found_uid->second.end())
            _policies = found_gid->second;
    }
}

void
routing_manager_stub::add_pending_security_update_handler(
        pending_security_update_id_t _id, security_update_handler_t _handler) {

    std::lock_guard<std::recursive_mutex> its_lock(security_update_handlers_mutex_);
    security_update_handlers_[_id] = _handler;
}

void
routing_manager_stub::add_pending_security_update_timer(
        pending_security_update_id_t _id) {

    std::shared_ptr<boost::asio::steady_timer> its_timer
        = std::make_shared<boost::asio::steady_timer>(io_);

    boost::system::error_code ec;
    its_timer->expires_from_now(std::chrono::milliseconds(3000), ec);
    if (!ec) {
        its_timer->async_wait(
                std::bind(
                        &routing_manager_stub::on_security_update_timeout,
                        shared_from_this(),
                        std::placeholders::_1, _id, its_timer));
    } else {
        VSOMEIP_ERROR << __func__
                << "[" << std::dec << _id << "]: timer creation: "
                << ec.message();
    }
    std::lock_guard<std::mutex> its_lock(security_update_timers_mutex_);
    security_update_timers_[_id] = its_timer;
}

bool
routing_manager_stub::send_requester_policies(const std::unordered_set<client_t> &_clients,
        const std::set<std::shared_ptr<policy> > &_policies) {

    pending_security_update_id_t its_policy_id;

    // serialize the policies and send them...
    for (const auto& p : _policies) {
        std::vector<byte_t> its_policy_data;
        if (p->serialize(its_policy_data)) {
            std::vector<byte_t> its_message;
            its_message.push_back(VSOMEIP_UPDATE_SECURITY_POLICY_INT);
            its_message.push_back(0);
            its_message.push_back(0);

            uint32_t its_policy_size = static_cast<uint32_t>(its_policy_data.size() + sizeof(uint32_t));
            its_message.push_back(VSOMEIP_LONG_BYTE0(its_policy_size));
            its_message.push_back(VSOMEIP_LONG_BYTE1(its_policy_size));
            its_message.push_back(VSOMEIP_LONG_BYTE2(its_policy_size));
            its_message.push_back(VSOMEIP_LONG_BYTE3(its_policy_size));

            its_policy_id = pending_security_update_add(_clients);
            its_message.push_back(VSOMEIP_LONG_BYTE0(its_policy_id));
            its_message.push_back(VSOMEIP_LONG_BYTE1(its_policy_id));
            its_message.push_back(VSOMEIP_LONG_BYTE2(its_policy_id));
            its_message.push_back(VSOMEIP_LONG_BYTE3(its_policy_id));

            its_message.insert(its_message.end(), its_policy_data.begin(), its_policy_data.end());

            for (const auto& c : _clients) {
                std::shared_ptr<endpoint> its_endpoint = host_->find_local(c);
                if (its_endpoint)
                    its_endpoint->send(&its_message[0], static_cast<uint32_t>(its_message.size()));
            }
        }
    }

    return (true);
}

void routing_manager_stub::on_security_update_timeout(
        const boost::system::error_code& _error,
        pending_security_update_id_t _id,
        std::shared_ptr<boost::asio::steady_timer> _timer) {
    (void)_timer;
    if (_error) {
        // timer was cancelled
        return;
    }
    security_update_state_e its_state = security_update_state_e::SU_UNKNOWN_USER_ID;
    std::unordered_set<client_t> its_missing_clients = pending_security_update_get(_id);
    {
        // erase timer
        std::lock_guard<std::mutex> its_lock(security_update_timers_mutex_);
        security_update_timers_.erase(_id);
    }
    {
        //  print missing responses and check if some clients did not respond because they already disconnected
        if (!its_missing_clients.empty()) {
            for (auto its_client : its_missing_clients) {
                VSOMEIP_INFO << __func__ << ": Client 0x" << std::hex << its_client
                        << " did not respond to the policy update / removal with ID: 0x" << std::hex << _id;
                if (!host_->find_local(its_client)) {
                    VSOMEIP_INFO << __func__ << ": Client 0x" << std::hex << its_client
                            << " is not connected anymore, do not expect answer for policy update / removal with ID: 0x"
                            << std::hex << _id;
                    pending_security_update_remove(_id, its_client);
                }
            }
        }

        its_missing_clients = pending_security_update_get(_id);
        if (its_missing_clients.empty()) {
            VSOMEIP_INFO << __func__ << ": Received all responses for "
                    "security update/removal ID: 0x" << std::hex << _id;
            its_state = security_update_state_e::SU_SUCCESS;
        }
        {
            // erase pending security update
            std::lock_guard<std::mutex> its_lock(pending_security_updates_mutex_);
            pending_security_updates_.erase(_id);
        }

        // call handler with error on timeout or with SUCCESS if missing clients are not connected
        std::lock_guard<std::recursive_mutex> its_lock(security_update_handlers_mutex_);
        const auto found_handler = security_update_handlers_.find(_id);
        if (found_handler != security_update_handlers_.end()) {
            found_handler->second(its_state);
            security_update_handlers_.erase(found_handler);
        } else {
            VSOMEIP_WARNING << __func__ << ": Callback not found for security update / removal with ID: 0x"
                    << std::hex << _id;
        }
    }
}

bool routing_manager_stub::update_security_policy_configuration(
        uint32_t _uid, uint32_t _gid,
        const std::shared_ptr<policy> &_policy,
        const std::shared_ptr<payload> &_payload,
        const security_update_handler_t &_handler) {

    bool ret(true);

    // cache security policy payload for later distribution to new registering clients
    policy_cache_add(_uid, _payload);

    // update security policy from configuration
    security::get()->update_security_policy(_uid, _gid, _policy);

    // Build requester policies for the services offered by the new policy
    std::set<std::shared_ptr<policy> > its_requesters;
    security::get()->get_requester_policies(_policy, its_requesters);

    // and add them to the requester policy cache
    add_requester_policies(_uid, _gid, its_requesters);

    // determine currently connected clients
    std::unordered_set<client_t> its_clients_to_inform;
    auto its_epm = host_->get_endpoint_manager();
    if (its_epm)
        its_clients_to_inform = its_epm->get_connected_clients();

    // add handler
    pending_security_update_id_t its_id;
    if (!its_clients_to_inform.empty()) {
        its_id = pending_security_update_add(its_clients_to_inform);

        add_pending_security_update_handler(its_id, _handler);
        add_pending_security_update_timer(its_id);

        // trigger all currently connected clients to update the security policy
        uint32_t sent_counter(0);
        uint32_t its_tranche =
                uint32_t(its_clients_to_inform.size() >= 10 ? (its_clients_to_inform.size() / 10) : 1);
        VSOMEIP_INFO << __func__ << ": Informing [" << std::dec << its_clients_to_inform.size()
                << "] currently connected clients about policy update for UID: "
                << std::dec << _uid << " with update ID: 0x" << std::hex << its_id;
        for (auto its_client : its_clients_to_inform) {
            if (!send_update_security_policy_request(its_client, its_id, _uid, _payload)) {
                VSOMEIP_INFO << __func__ << ": Couldn't send update security policy "
                                        << "request to client 0x" << std::hex << std::setw(4)
                                        << std::setfill('0') << its_client << " policy UID: "
                                        << std::hex << std::setw(4) << std::setfill('0') << _uid << " GID: "
                                        << std::hex << std::setw(4) << std::setfill('0') << _gid
                                        << " with update ID: 0x" << std::hex << its_id
                                        << " as client already disconnected";
                // remove client from expected answer list
                pending_security_update_remove(its_id, its_client);
            }
            sent_counter++;
            // Prevent burst
            if (sent_counter % its_tranche == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    } else {
        // if routing manager has no client call the handler directly
        _handler(security_update_state_e::SU_SUCCESS);
    }

    return ret;
}

bool routing_manager_stub::remove_security_policy_configuration(
        uint32_t _uid, uint32_t _gid, const security_update_handler_t &_handler) {

    bool ret(true);

    // remove security policy from configuration (only if there was a updateACL call before)
    if (is_policy_cached(_uid)) {
        if (!security::get()->remove_security_policy(_uid, _gid)) {
            _handler(security_update_state_e::SU_UNKNOWN_USER_ID);
            ret = false;
        } else {
            // remove policy from cache to prevent sending it to registering clients
            policy_cache_remove(_uid);

            // add handler
            pending_security_update_id_t its_id;

            // determine currently connected clients
            std::unordered_set<client_t> its_clients_to_inform;
            auto its_epm = host_->get_endpoint_manager();
            if (its_epm)
                its_clients_to_inform = its_epm->get_connected_clients();

            if (!its_clients_to_inform.empty()) {
                its_id = pending_security_update_add(its_clients_to_inform);

                add_pending_security_update_handler(its_id, _handler);
                add_pending_security_update_timer(its_id);

                // trigger all clients to remove the security policy
                uint32_t sent_counter(0);
                uint32_t its_tranche =
                        uint32_t(its_clients_to_inform.size() >= 10 ? (its_clients_to_inform.size() / 10) : 1);
                VSOMEIP_INFO << __func__ << ": Informing [" << std::dec << its_clients_to_inform.size()
                        << "] currently connected clients about policy removal for UID: "
                        << std::dec << _uid << " with update ID: " << its_id;
                for (auto its_client : its_clients_to_inform) {
                    if (!send_remove_security_policy_request(its_client, its_id, _uid, _gid)) {
                        VSOMEIP_INFO << __func__ << ": Couldn't send remove security policy "
                                                << "request to client 0x" << std::hex << std::setw(4)
                                                << std::setfill('0') << its_client << " policy UID: "
                                                << std::hex << std::setw(4) << std::setfill('0') << _uid << " GID: "
                                                << std::hex << std::setw(4) << std::setfill('0') << _gid
                                                << " with update ID: 0x" << std::hex << its_id
                                                << " as client already disconnected";
                        // remove client from expected answer list
                        pending_security_update_remove(its_id, its_client);
                    }
                    sent_counter++;
                    // Prevent burst
                    if (sent_counter % its_tranche == 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            } else {
                // if routing manager has no client call the handler directly
                _handler(security_update_state_e::SU_SUCCESS);
            }
        }
    }
    else {
        _handler(security_update_state_e::SU_UNKNOWN_USER_ID);
        ret = false;
    }
    return ret;
}

pending_security_update_id_t routing_manager_stub::pending_security_update_add(
        const std::unordered_set<client_t>& _clients) {
    std::lock_guard<std::mutex> its_lock(pending_security_updates_mutex_);
    if (++pending_security_update_id_ == 0) {
        pending_security_update_id_++;
    }
    pending_security_updates_[pending_security_update_id_] = _clients;

    return pending_security_update_id_;
}

std::unordered_set<client_t> routing_manager_stub::pending_security_update_get(
        pending_security_update_id_t _id) {
    std::lock_guard<std::mutex> its_lock(pending_security_updates_mutex_);
    std::unordered_set<client_t> its_missing_clients;
    auto found_si = pending_security_updates_.find(_id);
    if (found_si != pending_security_updates_.end()) {
        its_missing_clients = pending_security_updates_[_id];
    }
    return its_missing_clients;
}

bool routing_manager_stub::pending_security_update_remove(
        pending_security_update_id_t _id, client_t _client) {
    std::lock_guard<std::mutex> its_lock(pending_security_updates_mutex_);
    auto found_si = pending_security_updates_.find(_id);
    if (found_si != pending_security_updates_.end()) {
        if (found_si->second.erase(_client)) {
            return true;
        }
    }
    return false;
}

bool routing_manager_stub::is_pending_security_update_finished(
        pending_security_update_id_t _id) {
    std::lock_guard<std::mutex> its_lock(pending_security_updates_mutex_);
    bool ret(false);
    auto found_si = pending_security_updates_.find(_id);
    if (found_si != pending_security_updates_.end()) {
        if (!found_si->second.size()) {
            ret = true;
        }
    }
    if (ret) {
        pending_security_updates_.erase(_id);
    }
    return ret;
}

void routing_manager_stub::on_security_update_response(
        pending_security_update_id_t _id, client_t _client) {
    if (pending_security_update_remove(_id, _client)) {
        if (is_pending_security_update_finished(_id)) {
            // cancel timeout timer
            {
                std::lock_guard<std::mutex> its_lock(security_update_timers_mutex_);
                auto found_timer = security_update_timers_.find(_id);
                if (found_timer != security_update_timers_.end()) {
                    boost::system::error_code ec;
                    found_timer->second->cancel(ec);
                    security_update_timers_.erase(found_timer);
                } else {
                    VSOMEIP_WARNING << __func__ << ": Received all responses "
                            "for security update/removal ID: 0x"
                            << std::hex << _id << " but timeout already happened";
                }
            }

            // call handler
            {
                std::lock_guard<std::recursive_mutex> its_lock(security_update_handlers_mutex_);
                auto found_handler = security_update_handlers_.find(_id);
                if (found_handler != security_update_handlers_.end()) {
                    found_handler->second(security_update_state_e::SU_SUCCESS);
                    security_update_handlers_.erase(found_handler);
                    VSOMEIP_INFO << __func__ << ": Received all responses for "
                            "security update/removal ID: 0x" << std::hex << _id;
                } else {
                    VSOMEIP_WARNING << __func__ << ": Received all responses "
                            "for security update/removal ID: 0x"
                            << std::hex << _id << " but didn't find handler";
                }
            }
        }
    }
}

} // namespace vsomeip_v3
