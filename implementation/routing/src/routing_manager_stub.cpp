// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <functional>
#include <iomanip>
#include <forward_list>
#include <set>
#include <sstream>

#include <boost/system/error_code.hpp>

#include <vsomeip/constants.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/primitive_types.hpp>
#include <vsomeip/runtime.hpp>
#include <vsomeip/structured_types.hpp>

#include "logger_ext.hpp"
#include "../include/local_service_table.hpp"
#include "../include/routing_manager_stub.hpp"
#include "../include/routing_manager_stub_host.hpp"
#include "../include/remote_subscription.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../endpoints/include/endpoint_manager_impl.hpp"
#include "../../endpoints/include/boardnet_endpoint.hpp"
#include "../../endpoints/include/abstract_socket_factory.hpp"
#include "../../endpoints/include/local_endpoint.hpp"
#include "../../endpoints/include/local_server.hpp"
#include "../../protocol/include/logging.hpp"
#include "../../protocol/include/deserialize.hpp"
#include "../../protocol/include/command_types.hpp"
#include "../../protocol/include/serialize.hpp"
#include "../../security/include/policy_manager_impl.hpp"
#include "../../security/include/security.hpp"
#include "../../utility/include/bithelper.hpp"
#include "../../utility/include/utility.hpp"

namespace vsomeip_v3 {

#define VSOMEIP_LOG_PREFIX "rms"

routing_manager_stub::routing_manager_stub(routing_manager_stub_host* _host, const std::shared_ptr<configuration>& _configuration) :
    host_(_host), io_(_host->get_io()), configuration_(_configuration), is_socket_activated_(false),
    routing_mode_(configuration_->is_local_routing()           ? routing_mode_e::UDS_ONLY
                          : configuration_->is_uds_preferred() ? routing_mode_e::UDS_AND_TCP
                                                               : routing_mode_e::TCP_ONLY),
    pinged_clients_timer_(io_), pending_security_update_id_(0) { }

routing_manager_stub::~routing_manager_stub() { }

std::string routing_manager_stub::get_client_info(client_t _client) const {
    std::stringstream its_info;
    its_info << "[" << hex4(_client) << ", '" << utility::get_client_name(configuration_, _client) << "'";
    vsomeip_sec_client_t its_sec_client;
    if (configuration_->get_policy_manager()->get_client_to_sec_client_mapping(_client, its_sec_client)
        && its_sec_client.port == VSOMEIP_SEC_PORT_UNUSED) {
        its_info << ", uid " << its_sec_client.user;
    }
    its_info << "]";
    return its_info.str();
}

void routing_manager_stub::init() {
    init_routing_endpoint();
}

void routing_manager_stub::start() {

    auto start_root = [](auto& root) {
        if (root) {
            root->set_id(VSOMEIP_ROUTING_CLIENT);
            root->start();
        }
    };

    // Re-initialize only the roots required for the current mode (e.g. after stop/restart).
    if ((routing_mode_ != routing_mode_e::TCP_ONLY && !uds_root_) || (routing_mode_ != routing_mode_e::UDS_ONLY && !tcp_root_)) {
        init_routing_endpoint();
    }

    start_root(uds_root_);
    start_root(tcp_root_);
}

void routing_manager_stub::stop() {
    auto stop_local_server = [](auto& local_server) {
        if (local_server) {
            local_server->stop();
            local_server = nullptr;
        }
    };

    // Stop routing roots
    if (!is_socket_activated_) {
        stop_local_server(uds_root_);
        stop_local_server(tcp_root_);
    }
}

connection_control_response_e routing_manager_stub::change_connection_control(connection_control_request_e _control,
                                                                              const boost::asio::ip::address& _guest_address) {

    // Only tcp matters here, as makes no sense to block UDS connections
    // therefore, uds_root does not need to be handled here
    if (_control == connection_control_request_e::CCR_ACCEPT) {
        if (tcp_root_) {
            tcp_root_->allow_from(_guest_address);
        } else {
            VSOMEIP_ERROR_P << "change_connection_control(ACCEPT) called for " << _guest_address.to_string()
                            << " but tcp_root_ is not available";
        }
        return connection_control_response_e::CCR_OK;
    }

    // BLOCK case
    if (tcp_root_) {
        tcp_root_->block_from(_guest_address);
    } else {
        VSOMEIP_ERROR_P << "change_connection_control(BLOCK) called for " << _guest_address.to_string()
                        << " but tcp_root_ is not available";
    }

    // Due to the single lock in the local_server it is guaranteed that after
    // returning from the next line, no endpoint is in a transient state into
    // the endpoint_manager_impl
    host_->get_endpoint_manager()->drop_from(_guest_address);
    return connection_control_response_e::CCR_OK;
}

void routing_manager_stub::on_message(const byte_t* _data, length_t _size, const local_client_data& _peer_data) {
    protocol::command_header its_header{};
    uint32_t parsed_hdr_bytes = 0;
    if (parsed_hdr_bytes = protocol::deserialize(its_header, _data, _size); !parsed_hdr_bytes) {
        VSOMEIP_ERROR_P << "Deserialization of command header failed, memory: " << utility::dump(_data, _size);
        return;
    }

    client_t its_client = its_header.client_;
    protocol::id_e its_id = its_header.id_;

    if (_peer_data.id_ != its_client) {
        VSOMEIP_ERROR << "vSomeIP Security: routing_manager_stub::on_message: "
                      << "Routing Manager received a message from client " << hex4(its_client) << " with command " << its_id
                      << " which doesn't match the bound client " << hex4(_peer_data.id_) << " ~> skip message!";
        return;
    }

    switch (its_id) {

    case protocol::id_e::PING_ID: {
        on_ping(its_client);
        VSOMEIP_INFO << "PING(" << hex4(its_client) << ")";
        break;
    }

    case protocol::id_e::PONG_ID: {
        on_pong(its_client);
        VSOMEIP_INFO << "PONG(" << hex4(its_client) << ")";
        break;
    }

    case protocol::id_e::OFFER_SERVICE_ID:
    case protocol::id_e::STOP_OFFER_SERVICE_ID: {
        if (protocol::service_data its_service_data;
            protocol::deserialize(its_service_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            auto its_service = its_service_data.service_;
            auto its_instance = its_service_data.instance_;
            auto its_major = its_service_data.major_version_;
            auto its_minor = its_service_data.minor_version_;

            if (its_id == protocol::id_e::OFFER_SERVICE_ID) {
                if (VSOMEIP_SEC_OK
                    == configuration_->get_security()->is_client_allowed_to_offer(&_peer_data.sec_client_, its_service, its_instance)) {
                    host_->offer_service(its_client, its_service, its_instance, its_major, its_minor);
                } else {
                    VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_client)
                                  << " : routing_manager_stub::on_message: isn't allowed to offer the following service / instance "
                                  << hex4(its_service) << " / " << hex4(its_instance) << " ~ > Skip offer !";
                }
            } else {
                host_->stop_offer_service(its_client, its_service, its_instance, its_major, its_minor);
            }
        } else {
            VSOMEIP_ERROR_P << "Deserializing offer/stop offer service command failed, memory: " << utility::dump(_data, _size);
        }
        break;
    }

    case protocol::id_e::SUBSCRIBE_ID: {
        if (protocol::subscribe_with_filter_data its_data;
            protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            auto its_service = its_data.data_.service_;
            auto its_instance = its_data.data_.instance_;
            auto its_eventgroup = its_data.data_.eventgroup_;
            auto its_major = its_data.data_.major_;
            auto its_notifier = its_data.data_.event_;
            auto its_filter = its_data.filter_;

            if (its_notifier == ANY_EVENT) {
                if (host_->is_subscribe_to_any_event_allowed(&_peer_data.sec_client_, its_client, its_service, its_instance,
                                                             its_eventgroup)) {
                    host_->subscribe(its_client, &_peer_data.sec_client_, its_service, its_instance, its_eventgroup, its_major,
                                     its_notifier, its_filter);
                } else {
                    VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_client) << " :  routing_manager_stub::on_message: "
                                  << " subscribes to service/instance/event " << hex4(its_service) << "/" << hex4(its_instance)
                                  << "/ANY_EVENT which violates the security policy ~> Skip subscribe!";
                }
            } else {
                if (VSOMEIP_SEC_OK
                    == configuration_->get_security()->is_client_allowed_to_access_member(&_peer_data.sec_client_, its_service,
                                                                                          its_instance, its_notifier)) {
                    host_->subscribe(its_client, &_peer_data.sec_client_, its_service, its_instance, its_eventgroup, its_major,
                                     its_notifier, its_filter);
                } else {
                    VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(its_client) << " :  routing_manager_stub::on_message: "
                                  << " subscribes to service/instance/event " << hex4(its_service) << "/" << hex4(its_instance) << "/"
                                  << hex4(its_notifier) << " which violates the security policy ~> Skip subscribe!";
                }
            }
        } else {
            VSOMEIP_ERROR_P << "Deserializing subscribe failed: " << utility::dump(_data, _size);
        }
        break;
    }

    case protocol::id_e::UNSUBSCRIBE_ID: {
        if (protocol::subscribe_data its_data; protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
            host_->unsubscribe(its_client, its_data.service_, its_data.instance_, its_data.eventgroup_, its_data.event_);
        } else
            VSOMEIP_ERROR_P << "Deserializing unsubscribe failed: " << utility::dump(_data, _size);
        break;
    }

    case protocol::id_e::SUBSCRIBE_ACK_ID: {
        if (protocol::subscribe_answer_data its_data; protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            host_->on_subscribe_ack(its_data.subscriber_, its_data.service_, its_data.instance_, its_data.eventgroup_, its_data.event_,
                                    its_data.pending_id_);

            VSOMEIP_INFO << "SUBSCRIBE ACK(" << hex4(its_client) << "): [" << hex4(its_data.service_) << "." << hex4(its_data.instance_)
                         << "." << hex4(its_data.eventgroup_) << "." << hex4(its_data.event_) << "] id=" << hex4(its_data.pending_id_);
        } else
            VSOMEIP_ERROR_P << "Deserializing subscribe ack failed: " << utility::dump(_data, _size);

        break;
    }

    case protocol::id_e::SUBSCRIBE_NACK_ID: {
        if (protocol::subscribe_answer_data its_data; protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            host_->on_subscribe_nack(its_data.subscriber_, its_data.service_, its_data.instance_, its_data.eventgroup_, false,
                                     its_data.pending_id_);

            VSOMEIP_INFO << "SUBSCRIBE NACK(" << hex4(its_client) << "): [" << hex4(its_data.service_) << "." << hex4(its_data.instance_)
                         << "." << hex4(its_data.eventgroup_) << "." << hex4(its_data.event_) << "] id=" << hex4(its_data.pending_id_);
        } else
            VSOMEIP_ERROR_P << "Deserializing subscribe nack failed :" << utility::dump(_data, _size);

        break;
    }

    case protocol::id_e::UNSUBSCRIBE_ACK_ID: {
        if (protocol::unsubscribe_ack_data its_data; protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            auto its_service = its_data.service_;
            auto its_instance = its_data.instance_;
            auto its_eventgroup = its_data.eventgroup_;
            auto its_subscription_id = its_data.pending_id_;

            host_->on_unsubscribe_ack(its_client, its_service, its_instance, its_eventgroup, its_subscription_id);

            VSOMEIP_INFO << "UNSUBSCRIBE ACK(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                         << hex4(its_eventgroup) << "] id=" << hex4(its_subscription_id);
        } else
            VSOMEIP_ERROR_P << "Deserializing unsubscribe ack failed, memory: " << utility::dump(_data, _size);
        break;
    }

    case protocol::id_e::SEND_ID: {
        protocol::ipc_message_header its_ipc{};
        if (auto const ipc_bytes = protocol::deserialize(its_ipc, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            auto const* its_message_data = _data + parsed_hdr_bytes + ipc_bytes;
            auto const its_message_size = _size - parsed_hdr_bytes - ipc_bytes;

            if (its_message_size > VSOMEIP_MESSAGE_TYPE_POS) {

                auto its_service = bithelper::read_uint16_be(&its_message_data[VSOMEIP_SERVICE_POS_MIN]);
                auto its_method = bithelper::read_uint16_be(&its_message_data[VSOMEIP_METHOD_POS_MIN]);
                auto its_sender = bithelper::read_uint16_be(&its_message_data[VSOMEIP_CLIENT_POS_MIN]);

                auto its_instance = its_ipc.instance_;
                bool is_reliable = its_ipc.reliable_;
                uint8_t its_check_status = its_ipc.status_;

                // Allow response messages from local proxies as answer to remote requests
                // but check requests sent by local proxies to remote against policy.
                if (utility::is_request(its_message_data[VSOMEIP_MESSAGE_TYPE_POS])) {
                    if (VSOMEIP_SEC_OK
                        != configuration_->get_security()->is_client_allowed_to_access_member(&_peer_data.sec_client_, its_service,
                                                                                              its_instance, its_method)) {
                        VSOMEIP_WARNING
                                << "vSomeIP Security: Client 0x" << hex4(its_sender)
                                << " : routing_manager_stub::on_message: isn't allowed to send a request to service/instance/method "
                                << hex4(its_service) << "/" << hex4(its_instance) << "/" << hex4(its_method) << " ~> Skip message!";
                        return;
                    }
                }
                uint32_t its_contained_size = bithelper::read_uint32_be(&its_message_data[VSOMEIP_LENGTH_POS_MIN]);
                if (its_message_size != its_contained_size + VSOMEIP_SOMEIP_HEADER_SIZE) {
                    VSOMEIP_WARNING_P << "Received a SEND command containing message with invalid size -> skip!";
                    break;
                }
                host_->on_message(its_service, its_instance, its_message_data, length_t(its_message_size), is_reliable, _peer_data.id_,
                                  &_peer_data.sec_client_, its_check_status, false);
            }
        } else {
            VSOMEIP_ERROR_P << "SEND_ID deserialization failed: " << utility::dump(_data, _size);
        }
        break;
    }

    case protocol::id_e::NOTIFY_ID:
    case protocol::id_e::NOTIFY_ONE_ID: {
        protocol::ipc_message_header its_ipc{};
        if (auto const ipc_bytes = protocol::deserialize(its_ipc, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            auto const* its_message_data = _data + parsed_hdr_bytes + ipc_bytes;
            auto const its_message_size = _size - parsed_hdr_bytes - ipc_bytes;

            if (its_message_size > VSOMEIP_MESSAGE_TYPE_POS) {

                auto its_target = its_ipc.target_;
                auto its_service = bithelper::read_uint16_be(&its_message_data[VSOMEIP_SERVICE_POS_MIN]);
                auto its_instance = its_ipc.instance_;

                uint32_t its_contained_size = bithelper::read_uint32_be(&its_message_data[VSOMEIP_LENGTH_POS_MIN]);

                if (its_message_size != its_contained_size + VSOMEIP_SOMEIP_HEADER_SIZE) {
                    VSOMEIP_WARNING_P << "Received a NOTIFY command containing message with invalid size -> skip!";
                    break;
                }

                host_->on_notification(its_target, its_service, its_instance, its_message_data, length_t(its_message_size),
                                       its_id == protocol::id_e::NOTIFY_ONE_ID);
                break;
            }
        } else {
            VSOMEIP_ERROR_P << "NOTIFY deserialization failed: " << utility::dump(_data, _size);
        }
        break;
    }

    case protocol::id_e::REQUEST_SERVICE_ID: {
        if (std::vector<protocol::service_data> its_services;
            protocol::deserialize(its_services, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            std::set<protocol::service> its_allowed_requests;
            for (const auto& r : its_services) {
                if (VSOMEIP_SEC_OK
                    == configuration_->get_security()->is_client_allowed_to_request(&_peer_data.sec_client_, r.service_, r.instance_)) {
                    if (has_client_requested(its_client, r.service_, r.instance_)) {
                        VSOMEIP_WARNING_P << " Client 0x" << hex4(its_client) << " has already requested service [" << hex4(r.service_)
                                          << "." << hex4(r.instance_) << "]";
                        if (!host_->handle_service_rerequest(its_client, r.service_, r.instance_, r.major_version_)) {
                            continue;
                        }
                    }
                    host_->request_service(its_client, r.service_, r.instance_, r.major_version_, r.minor_version_);
                    its_allowed_requests.insert(protocol::service(r.service_, r.instance_, r.major_version_, r.minor_version_));
                } else {
                    VSOMEIP_ERROR << "vSomeIP Security: Client 0x" << hex4(get_client()) << " received a request from client 0x"
                                  << hex4(its_client) << " to service/instance " << hex4(r.service_) << "/" << hex4(r.instance_)
                                  << " ~> skip message!";
                }
            }
            if (configuration_->is_security_enabled()) {
                handle_credentials(its_client, its_allowed_requests);
            }

            handle_requests(its_client, its_allowed_requests);
        } else
            VSOMEIP_ERROR_P << "Request service deserialization failed, memory: " << utility::dump(_data, _size);

        break;
    }

    case protocol::id_e::RELEASE_SERVICE_ID: {
        if (protocol::release_service_data its_release_data;
            protocol::deserialize(its_release_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
            host_->release_service(its_client, its_release_data.service_, its_release_data.instance_);
        } else {
            VSOMEIP_ERROR_P << "Release service deserialization failed, memory: " << utility::dump(_data, _size);
        }
        break;
    }

    case protocol::id_e::REGISTER_EVENT_ID: {
        if (std::vector<protocol::register_event_data> its_registrations;
            protocol::deserialize(its_registrations, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            for (auto const& reg : its_registrations) {
                auto its_service = reg.service_;
                auto its_instance = reg.instance_;

                if (reg.is_provided_ && !configuration_->is_offered_remote(its_service, its_instance)) {
                    continue;
                }

                host_->register_shadow_event(its_client, its_service, its_instance, reg.event_,
                                             std::set<eventgroup_t>{reg.eventgroups_.begin(), reg.eventgroups_.end()}, reg.event_type_,
                                             reg.reliability_, reg.is_provided_, reg.is_cyclic_);

                VSOMEIP_INFO << "REGISTER EVENT(" << hex4(its_client) << "): [" << hex4(its_service) << "." << hex4(its_instance) << "."
                             << hex4(reg.event_) << ":eventtype=" << static_cast<int>(reg.event_type_) << ":is_provided=" << std::boolalpha
                             << reg.is_provided_ << ":reliable=" << static_cast<int>(reg.reliability_) << "]";
            }

        } else
            VSOMEIP_ERROR_P << "Register event deserialization failed, memory: " << utility::dump(_data, _size);
        break;
    }

    case protocol::id_e::UNREGISTER_EVENT_ID: {
        if (protocol::unregister_event_data its_data; protocol::deserialize(its_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {

            host_->unregister_shadow_event(its_client, its_data.service_, its_data.instance_, its_data.event_, its_data.is_provided_);

            VSOMEIP_INFO << "UNREGISTER EVENT(" << hex4(its_client) << "): [" << hex4(its_data.service_) << "." << hex4(its_data.instance_)
                         << "." << hex4(its_data.event_) << ":is_provider=" << std::boolalpha << its_data.is_provided_ << "]";
        } else
            VSOMEIP_ERROR_P << "Unregister event deserialization failed, memory: " << utility::dump(_data, _size);
        break;
    }

    case protocol::id_e::OFFERED_SERVICES_REQUEST_ID: {
        if (offer_type_e its_offer_type; protocol::deserialize(its_offer_type, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
            on_offered_service_request(its_client, its_offer_type);
        } else {
            VSOMEIP_ERROR_P << "Deserialization of offered services request command failed, memory: " << utility::dump(_data, _size);
        }
        break;
    }

    case protocol::id_e::RESEND_PROVIDED_EVENTS_ID: {
        if (pending_remote_offer_id_t its_resend_command_data;
            protocol::deserialize(its_resend_command_data, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
            host_->on_resend_provided_events_response(its_resend_command_data);
        } else {
            VSOMEIP_ERROR_P << "Deserialization of resend provided events command failed, memory: " << utility::dump(_data, _size);
        }
        break;
    }
#ifndef VSOMEIP_DISABLE_SECURITY
    case protocol::id_e::UPDATE_SECURITY_POLICY_RESPONSE_ID: {
        if (uint32_t its_update_id; protocol::deserialize(its_update_id, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
            on_security_update_response(its_update_id, its_client);
        } else
            VSOMEIP_ERROR_P << "Update security policy deserialization failed, memory: " << utility::dump(_data, _size);
        break;
    }

    case protocol::id_e::REMOVE_SECURITY_POLICY_RESPONSE_ID: {
        if (uint32_t its_update_id; protocol::deserialize(its_update_id, _data + parsed_hdr_bytes, _size - parsed_hdr_bytes)) {
            on_security_update_response(its_update_id, its_client);
        } else
            VSOMEIP_ERROR_P << "Remove security policy deserialization failed, memory: " << utility::dump(_data, _size);
        break;
    }
#endif // !VSOMEIP_DISABLE_SECURITY
    default:
        VSOMEIP_ERROR_P << "Received an unhandled command (" << static_cast<int>(its_id) << ")";
    }
}

void routing_manager_stub::lazy_load(const std::string& _client_host) {
#if !defined(VSOMEIP_DISABLE_SECURITY) && (defined(__linux__))
    std::scoped_lock lock{lazy_load_mtx_};
    if (configuration_->is_security_enabled() && !configuration_->is_security_external()) {
        configuration_->lazy_load_security(_client_host);
        configuration_->lazy_load_security(host_->get_client_host()); // necessary for lazy loading from inside android container
    }
#endif
}

void routing_manager_stub::on_deregister_application(client_t _client) {
    std::vector<std::tuple<service_t, instance_t, major_version_t, minor_version_t>> services_to_report;

    std::unique_lock its_lock{routing_info_mutex_};
    auto its_info = routing_info_.find(_client);
    if (its_info != routing_info_.end()) {
        for (const auto& [si, version] : its_info->second) {
            services_to_report.push_back(std::make_tuple(si.service, si.instance, version.first, version.second));
        }
    }

    host_->remove_pending_requests(pending_request_removal_type_e::BOTH, _client);

    for (const auto& [_service, _instance, _major, _minor] : services_to_report) {
        host_->on_stop_offer_service_unlocked(_client, _service, _instance, _major, _minor, false);
        on_stop_offer_service_unlocked(_client, _service, _instance, _major, _minor);
    }

    routing_info_.erase(_client);
}

void routing_manager_stub::on_offered_service_request(client_t _client, offer_type_e _offer_type) {

    local_service_table table;

    std::scoped_lock its_guard{routing_info_mutex_};
    for (const auto& found_client : routing_info_) {
        // skip services which are offered on remote hosts
        if (found_client.first != VSOMEIP_ROUTING_CLIENT) {
            for (const auto& [si, version] : found_client.second) {
                const auto& [its_service, its_instance] = si;
                uint16_t its_reliable_port = configuration_->get_reliable_port(its_service, its_instance);
                uint16_t its_unreliable_port = configuration_->get_unreliable_port(its_service, its_instance);
                bool has_port = (its_reliable_port != ILLEGAL_PORT || its_unreliable_port != ILLEGAL_PORT);

                if (_offer_type == offer_type_e::OT_ALL || (_offer_type == offer_type_e::OT_LOCAL && !has_port)
                    || (_offer_type == offer_type_e::OT_REMOTE && has_port)) {
                    table.insert(protocol::service_data{.service_ = its_service,
                                                        .instance_ = its_instance,
                                                        .major_version_ = version.first,
                                                        .minor_version_ = version.second});
                }
            }
        }
    }
    if (auto its_endpoint = find_local_routing_endpoint(_client); its_endpoint) {
        its_endpoint->send(protocol::create_offered_services_response_cmd(_client, table.view()));
    } else {
        VSOMEIP_ERROR_P << "Failed for client " << get_client_info(_client) << ", as no routing connection was given";
    }
}

void routing_manager_stub::on_register_application(client_t _client, const boost::asio::ip::address& _address, port_t _port) {
    std::stringstream its_address;

    if (_port > 0 && _port < ILLEGAL_PORT) {
        its_address << " @ " << _address.to_string() << ":" << _port;
    }

    VSOMEIP_INFO << "Application/Client " << hex4(_client) << " is registering" << its_address.str();

    // Find or create a local endpoint.
    {
        std::scoped_lock its_lock{routing_info_mutex_};
        routing_info_[_client]; // ensure entry exists
    }
#ifndef VSOMEIP_DISABLE_SECURITY
    // distribute updated security config to new clients
    send_cached_security_policies(_client);
    if (configuration_->is_local_routing()) {
        vsomeip_sec_client_t its_sec_client;
        std::set<std::shared_ptr<policy>> its_policies;

        bool has_mapping = configuration_->get_policy_manager()->get_client_to_sec_client_mapping(_client, its_sec_client);
        if (has_mapping) {
            if (its_sec_client.port == VSOMEIP_SEC_PORT_UNUSED) {
                get_requester_policies(its_sec_client.user, its_sec_client.group, its_policies);
            }

            if (!its_policies.empty()) {
                send_requester_policies({_client}, its_policies);
            }
        }
    }
#endif // !VSOMEIP_DISABLE_SECURITY
}

void routing_manager_stub::remove_client_connections(client_t client_id) {
    {
        std::scoped_lock its_guard{routing_info_mutex_};
        service_requests_.erase(client_id);
    }
    host_->remove_local(client_id);
}

void routing_manager_stub::init_routing_endpoint() {
    auto ep_mgr = host_->get_endpoint_manager();
    bool is_successful{true};

    auto create_root = [&](auto& _root, transport_protocol_e _protocol) {
        if (_root)
            return; // already initialised, skip
        if (!ep_mgr->create_routing_root(_root, _protocol, is_socket_activated_, shared_from_this())) {
            is_successful = false;
        }
    };

    if (routing_mode_ != routing_mode_e::TCP_ONLY) {
        create_root(uds_root_, transport_protocol_e::UDS);
    }
    if (routing_mode_ != routing_mode_e::UDS_ONLY) {
        create_root(tcp_root_, transport_protocol_e::TCP);
    }

    if (!is_successful) {
        VSOMEIP_WARNING_P << "Routing root creating (partially) failed. Please check your configuration.";
    }
}
void routing_manager_stub::on_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                            minor_version_t _minor) {
    std::scoped_lock its_guard{routing_info_mutex_};
    on_offer_service_unlocked(_client, _service, _instance, _major, _minor);
}

void routing_manager_stub::on_offer_service_unlocked(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                     minor_version_t _minor) {

    VSOMEIP_INFO << "ON_OFFER_SERVICE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":"
                 << static_cast<int>(_major) << "." << _minor << "]";

    routing_info_[_client][{_service, _instance}] = std::make_pair(_major, _minor);
    if (configuration_->is_security_enabled()) {
        distribute_credentials(_client, _service, _instance);
    }
    inform_requesters(_client, _service, _instance, _major, _minor, protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE);
}

bool routing_manager_stub::offer_service_if_established(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                        minor_version_t _minor, const std::shared_ptr<boardnet_endpoint>& _endpoint) {
    std::scoped_lock its_guard{routing_info_mutex_};
    if (!_endpoint || !_endpoint->is_established() || contained_in_routing_info_unlocked(_client, _service, _instance, _major, _minor)) {
        return false;
    }

    on_offer_service_unlocked(_client, _service, _instance, _major, _minor);
    return true;
}

void routing_manager_stub::on_stop_offer_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                 minor_version_t _minor) {
    {
        std::scoped_lock its_lock{routing_info_mutex_};
        on_stop_offer_service_unlocked(_client, _service, _instance, _major, _minor);
    }
}

void routing_manager_stub::on_stop_offer_service_unlocked(client_t _client, service_t _service, instance_t _instance,
                                                          major_version_t _major, minor_version_t _minor) {

    VSOMEIP_INFO << "ON_STOP_OFFER_SERVICE(" << hex4(_client) << "): [" << hex4(_service) << "." << hex4(_instance) << ":"
                 << static_cast<int>(_major) << "." << _minor << "]";

    if (auto found_client = routing_info_.find(_client); found_client != routing_info_.end()) {
        if (auto found_si = found_client->second.find({_service, _instance}); found_si != found_client->second.end()) {
            const auto& [found_major, found_minor] = found_si->second;
            if ((_major == found_major && _minor == found_minor) || (_major == DEFAULT_MAJOR && _minor == DEFAULT_MINOR)) {
                found_client->second.erase(found_si);
                inform_requesters(_client, _service, _instance, _major, _minor,
                                  protocol::routing_info_entry_type_e::RIE_DELETE_SERVICE_INSTANCE);
            }
        }
    }
}

void routing_manager_stub::send_client_credentials(const client_t _target, std::set<std::pair<uid_t, gid_t>>& _credentials) {

    if (auto its_endpoint = find_local_routing_endpoint(_target); its_endpoint) {
        its_endpoint->send(protocol::create_update_security_credentials_cmd(_target, _credentials));
    } else {
        VSOMEIP_ERROR_P << "Sending credentials to client " << get_client_info(_target) << " failed";
    }
}

void routing_manager_stub::send_client_routing_info(const client_t _target, protocol::routing_info_entry_data _entry) {

    std::vector<protocol::routing_info_entry_data> its_entries;
    its_entries.emplace_back(std::move(_entry));
    send_client_routing_info(_target, std::move(its_entries));
}

void routing_manager_stub::send_client_routing_info(const client_t _target, std::vector<protocol::routing_info_entry_data>&& _entries) {

    if (auto its_target_endpoint = find_local_routing_endpoint(_target); its_target_endpoint) {
        its_target_endpoint->send(protocol::create_routing_info_cmd(VSOMEIP_ROUTING_CLIENT, std::move(_entries)));
    } else
        VSOMEIP_ERROR_P << "Sending routing info to client " << get_client_info(_target) << " failed";
}

void routing_manager_stub::distribute_credentials(client_t _hoster, service_t _service, instance_t _instance) {
    std::set<std::pair<uid_t, gid_t>> its_credentials;
    std::set<client_t> its_requesting_clients;
    // search for clients which shall receive the credentials
    for (auto its_requesting_client : service_requests_) {
        if (its_requesting_client.second.count({_service, _instance}) > 0
            || its_requesting_client.second.count({_service, ANY_INSTANCE}) > 0) {
            its_requesting_clients.insert(its_requesting_client.first);
        }
    }

    // search for UID / GID linked with the client ID that offers the requested services
    vsomeip_sec_client_t its_sec_client;
    if (configuration_->get_policy_manager()->get_client_to_sec_client_mapping(_hoster, its_sec_client)) {
        std::pair<uid_t, gid_t> its_uid_gid;
        its_uid_gid.first = its_sec_client.user;
        its_uid_gid.second = its_sec_client.group;
        its_credentials.insert(its_uid_gid);
        for (auto its_requesting_client : its_requesting_clients) {
            vsomeip_sec_client_t its_requester_sec_client;
            if (configuration_->get_policy_manager()->get_client_to_sec_client_mapping(its_requesting_client, its_requester_sec_client)) {
                if (!utility::compare(its_sec_client, its_requester_sec_client))
                    send_client_credentials(its_requesting_client, its_credentials);
            }
        }
    }
}

void routing_manager_stub::inform_requesters(client_t _hoster, service_t _service, instance_t _instance, major_version_t _major,
                                             minor_version_t _minor, protocol::routing_info_entry_type_e _type) {

    boost::asio::ip::address its_address;
    port_t its_port;

    for (auto its_client : service_requests_) {
        auto const& service_map = its_client.second;
        auto it = service_map.find({_service, _instance});
        if (it == service_map.end()) {
            it = service_map.find({_service, ANY_INSTANCE});
            if (it == service_map.end()) {
                continue;
            }
        }
        if (_major == it->second.first || ANY_MAJOR == it->second.first) {
            if (its_client.first != VSOMEIP_ROUTING_CLIENT) {
                protocol::routing_info_entry_data its_entry;
                its_entry.type_ = _type;
                its_entry.client_ = _hoster;
                if (_type == protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE
                    && host_->get_endpoint_manager()->get_guest(_hoster, its_address, its_port)) {
                    its_entry.address_ = its_address.to_v4();
                    its_entry.port_ = its_port;
                }
                its_entry.services_.push_back({_service, _instance, _major, _minor});
                send_client_routing_info(its_client.first, std::move(its_entry));
            }
        }
    }
}

bool routing_manager_stub::has_client_requested(client_t _client, service_t _service, instance_t _instance) const {
    std::scoped_lock its_lock(routing_info_mutex_);
    if (auto found_client = service_requests_.find(_client); found_client != service_requests_.end()) {
        if (found_client->second.count({_service, _instance}) > 0) {
            return true;
        }
    }

    return false;
}

void routing_manager_stub::broadcast(protocol::simple_command_data const& _command) const {
    if (auto epm = host_->get_endpoint_manager(); epm) {
        epm->broadcast_locally(_command);
    }
}

void routing_manager_stub::send_subscribe_ack(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                              event_t _event) {

    if (auto its_target = find_local_routing_endpoint(_client); its_target) {
        its_target->send(protocol::create_subscribe_ack_cmd(VSOMEIP_ROUTING_CLIENT,
                                                            protocol::subscribe_answer_data{.service_ = _service,
                                                                                            .instance_ = _instance,
                                                                                            .eventgroup_ = _eventgroup,
                                                                                            .subscriber_ = _client,
                                                                                            .event_ = _event,
                                                                                            .pending_id_ = 0x0}));
    }
}

bool routing_manager_stub::contained_in_routing_info(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                                     minor_version_t _minor) const {
    std::scoped_lock its_guard{routing_info_mutex_};
    return contained_in_routing_info_unlocked(_client, _service, _instance, _major, _minor);
}

bool routing_manager_stub::contained_in_routing_info_unlocked(client_t _client, service_t _service, instance_t _instance,
                                                              major_version_t _major, minor_version_t _minor) const {
    if (auto found_client = routing_info_.find(_client); found_client != routing_info_.end()) {
        if (auto found_si = found_client->second.find({_service, _instance}); found_si != found_client->second.end()) {
            if (found_si->second.first == _major && found_si->second.second == _minor) {
                return true;
            }
        }
    }
    return false;
}

void routing_manager_stub::on_ping(client_t _client) {

    if (auto its_endpoint = find_local_routing_endpoint(_client); its_endpoint) {
        its_endpoint->send(protocol::create_pong_cmd(VSOMEIP_ROUTING_CLIENT));
    } else {
        VSOMEIP_WARNING_P << "Couldn't find endpoint for client " << hex4(_client);
    }
}

void routing_manager_stub::on_pong(client_t _client) {
    remove_from_pinged_clients(_client);
    host_->on_pong(_client);
}

bool routing_manager_stub::send_ping(client_t _client) {

    bool has_sent(false);

    if (auto its_endpoint = find_local_routing_endpoint(_client); its_endpoint) {
        std::scoped_lock its_lock{pinged_clients_mutex_};

        if (pinged_clients_.count(_client) > 0) {
            // client was already pinged: don't ping again and wait for answer
            // or timeout of previous ping.
            has_sent = true;
        } else {
            pinged_clients_timer_.cancel();
            const std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());

            std::chrono::milliseconds next_timeout(VSOMEIP_DEFAULT_PING_TIMEOUT);
            for (const auto& tp : pinged_clients_) {
                const std::chrono::milliseconds its_clients_timeout =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - tp.second);
                if (next_timeout > its_clients_timeout) {
                    next_timeout = its_clients_timeout;
                }
            }

            pinged_clients_[_client] = now;

            pinged_clients_timer_.expires_after(next_timeout);
            pinged_clients_timer_.async_wait(std::bind(&routing_manager_stub::on_ping_timer_expired, this, std::placeholders::_1));

            has_sent = its_endpoint->send(protocol::create_ping_cmd(VSOMEIP_ROUTING_CLIENT));
        }
    }

    return has_sent;
}

void routing_manager_stub::on_ping_timer_expired(boost::system::error_code const& _error) {
    if (_error) {
        return;
    }
    std::forward_list<client_t> timed_out_clients;
    std::chrono::milliseconds next_timeout(VSOMEIP_DEFAULT_PING_TIMEOUT);
    bool pinged_clients_remaining(false);

    {
        // remove timed out clients
        std::scoped_lock its_lock{pinged_clients_mutex_};
        const std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());

        std::erase_if(pinged_clients_, [&now, this](const auto& _entry) {
            if ((now - _entry.second) >= std::chrono::milliseconds(VSOMEIP_DEFAULT_PING_TIMEOUT)) {
                // Trigger the error under the pinged_clients_mutex_, to ensure that a concurrent clean-up
                // of the client is not racing with this timer.
                if (auto ep = host_->get_endpoint_manager()->find_routing_endpoint(_entry.first); ep) {
                    VSOMEIP_WARNING_P << "Triggering a client error for: " << hex4(_entry.first);
                    ep->trigger_error();
                }
                return true;
            }
            return false;
        });
        pinged_clients_remaining = (pinged_clients_.size() > 0);

        if (pinged_clients_remaining) {
            // find out next timeout
            for (const auto& tp : pinged_clients_) {
                const std::chrono::milliseconds its_clients_timeout =
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - tp.second);
                if (next_timeout > its_clients_timeout) {
                    next_timeout = its_clients_timeout;
                }
            }
        }
        if (pinged_clients_remaining) {
            pinged_clients_timer_.expires_after(next_timeout);
            pinged_clients_timer_.async_wait(std::bind(&routing_manager_stub::on_ping_timer_expired, this, std::placeholders::_1));
        }
    }
}

void routing_manager_stub::remove_from_pinged_clients(client_t _client) {
    std::scoped_lock its_lock{pinged_clients_mutex_};
    if (!pinged_clients_.size()) {
        return;
    }
    pinged_clients_timer_.cancel();
    pinged_clients_.erase(_client);

    if (!pinged_clients_.size()) {
        return;
    }
    const std::chrono::steady_clock::time_point now(std::chrono::steady_clock::now());
    std::chrono::milliseconds next_timeout(VSOMEIP_DEFAULT_PING_TIMEOUT);
    // find out next timeout
    for (const auto& tp : pinged_clients_) {
        const std::chrono::milliseconds its_clients_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(now - tp.second);
        if (next_timeout > its_clients_timeout) {
            next_timeout = its_clients_timeout;
        }
    }
    pinged_clients_timer_.expires_after(next_timeout);
    pinged_clients_timer_.async_wait(std::bind(&routing_manager_stub::on_ping_timer_expired, this, std::placeholders::_1));
}

bool routing_manager_stub::is_registered(client_t _client) const {
    std::scoped_lock its_lock{routing_info_mutex_};
    return (routing_info_.count(_client) > 0);
}

void routing_manager_stub::deregister_client(client_t _client) {

    configuration_->get_policy_manager()->remove_client_to_sec_client_mapping(_client);
    VSOMEIP_INFO << "Application/Client " << hex4(_client) << " is deregistering";
    on_deregister_application(_client);
    remove_from_pinged_clients(_client);
    remove_client_connections(_client);
    utility::release_client_id(configuration_->get_network(), _client);
}

client_t routing_manager_stub::get_client() const {
    return host_->get_client();
}

void routing_manager_stub::handle_credentials(const client_t _client, std::set<protocol::service>& _requests) {
    if (!_requests.size()) {
        return;
    }

    std::scoped_lock its_guard{routing_info_mutex_};
    std::set<std::pair<uid_t, gid_t>> its_credentials;
    vsomeip_sec_client_t its_requester_sec_client;
    if (configuration_->get_policy_manager()->get_client_to_sec_client_mapping(_client, its_requester_sec_client)) {
        // determine credentials of offering clients using current routing info
        std::set<client_t> its_offering_clients;

        // search in local clients for the offering client
        for (auto request : _requests) {
            std::set<client_t> its_clients;
            // TODO
            its_clients = host_->find_local_clients(request.service_, request.instance_, ANY_MAJOR);
            for (auto its_client : its_clients) {
                its_offering_clients.insert(its_client);
            }
        }

        // search for UID / GID linked with the client ID that offers the requested services
        for (auto its_offering_client : its_offering_clients) {
            vsomeip_sec_client_t its_sec_client;
            if (configuration_->get_policy_manager()->get_client_to_sec_client_mapping(its_offering_client, its_sec_client)) {
                if (its_sec_client.port == VSOMEIP_SEC_PORT_UNUSED && !utility::compare(its_sec_client, its_requester_sec_client)) {

                    its_credentials.insert(std::make_pair(its_sec_client.user, its_sec_client.group));
                }
            }
        }

        // send credentials to clients
        if (!its_credentials.empty())
            send_client_credentials(_client, its_credentials);
    }
}

void routing_manager_stub::handle_requests(const client_t _client, std::set<protocol::service>& _requests) {

    if (_requests.empty())
        return;

    boost::asio::ip::address its_address;
    port_t its_port;

    std::vector<protocol::routing_info_entry_data> its_entries;
    std::scoped_lock its_guard{routing_info_mutex_};

    for (auto const& request : _requests) {
        service_requests_[_client][{request.service_, request.instance_}] = std::make_pair(request.major_, request.minor_);
        if (_client == VSOMEIP_ROUTING_CLIENT) {
            continue;
        }
        std::set<client_t> its_clients = host_->find_local_clients(request.service_, request.instance_, request.major_);
        // insert VSOMEIP_ROUTING_CLIENT to check whether service is remotely offered
        its_clients.insert(VSOMEIP_ROUTING_CLIENT);
        for (const client_t c : its_clients) {
            if (const auto found_client = routing_info_.find(c); found_client != routing_info_.end()) {
                if (request.instance_ == ANY_INSTANCE) {
                    for (const auto& [si, version] : found_client->second) {
                        if (si.service == request.service_ && (version.first == request.major_ || request.major_ == ANY_MAJOR)) {
                            protocol::routing_info_entry_data its_entry;
                            its_entry.type_ = protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE;
                            its_entry.client_ = c;
                            if (host_->get_endpoint_manager()->get_guest(c, its_address, its_port)) {
                                its_entry.address_ = its_address.to_v4();
                                its_entry.port_ = its_port;
                            }
                            its_entry.services_.push_back({request.service_, si.instance, version.first, version.second});
                            its_entries.emplace_back(std::move(its_entry));
                        }
                    }
                } else {
                    if (auto found_si = found_client->second.find({request.service_, request.instance_});
                        found_si != found_client->second.end()
                        && (found_si->second.first == request.major_ || request.major_ == ANY_MAJOR)) {
                        protocol::routing_info_entry_data its_entry;
                        its_entry.type_ = protocol::routing_info_entry_type_e::RIE_ADD_SERVICE_INSTANCE;
                        its_entry.client_ = c;
                        if (host_->get_endpoint_manager()->get_guest(c, its_address, its_port)) {
                            its_entry.address_ = its_address.to_v4();
                            its_entry.port_ = its_port;
                        }
                        its_entry.services_.push_back(
                                {request.service_, request.instance_, found_si->second.first, found_si->second.second});
                        its_entries.emplace_back(std::move(its_entry));
                    }
                }
            }
        }
    }

    if (!its_entries.empty()) {
        send_client_routing_info(_client, std::move(its_entries));
    }
}

bool routing_manager_stub::send_provided_event_resend_request(client_t _client, pending_remote_offer_id_t _id) {

    if (auto its_endpoint = find_local_routing_endpoint(_client); its_endpoint) {
        return its_endpoint->send(protocol::create_resend_provided_events_cmd(VSOMEIP_ROUTING_CLIENT, _id));
    } else {
        VSOMEIP_WARNING_P << "Couldn't send provided event resend request to local client: 0x" << hex4(_client);
    }

    return false;
}

#ifndef VSOMEIP_DISABLE_SECURITY
bool routing_manager_stub::is_policy_cached(uid_t _uid) {
    {
        std::scoped_lock its_lock{updated_security_policies_mutex_};
        if (updated_security_policies_.count(_uid) > 0) {
            VSOMEIP_INFO_P << "Policy for UID: " << _uid << " was already updated before!";
            return true;
        } else {
            return false;
        }
    }
}

void routing_manager_stub::policy_cache_add(uid_t _uid, const std::shared_ptr<payload>& _payload) {
    // cache security policy payload for later distribution to new registering clients
    {
        std::scoped_lock its_lock{updated_security_policies_mutex_};
        updated_security_policies_[_uid] = _payload;
    }
}

void routing_manager_stub::policy_cache_remove(uid_t _uid) {
    {
        std::scoped_lock its_lock{updated_security_policies_mutex_};
        updated_security_policies_.erase(_uid);
    }
}

bool routing_manager_stub::send_update_security_policy_request(client_t _client, pending_security_update_id_t _update_id, uid_t _uid,
                                                               const std::shared_ptr<payload>& _payload) {
    (void)_uid;

    if (auto its_endpoint = find_local_routing_endpoint(_client); its_endpoint) {
        return its_endpoint->send(protocol::create_update_security_policy_cmd(
                _client, _update_id, std::span<unsigned char>(_payload->get_data(), _payload->get_length())));
    } else {
        return false;
    }
}

bool routing_manager_stub::send_cached_security_policies(client_t _client) {

    if (auto its_endpoint = find_local_routing_endpoint(_client); its_endpoint) {

        std::scoped_lock its_lock{updated_security_policies_mutex_};
        if (!updated_security_policies_.empty()) {

            VSOMEIP_INFO_P << "Distributing " << updated_security_policies_.size()
                           << " security policy updates to registering client: " << hex4(_client);
            std::vector<std::span<byte_t const>> data;
            data.reserve(updated_security_policies_.size());
            for (auto const& [_, payload] : updated_security_policies_) {
                data.emplace_back(payload->get_data(), static_cast<std::size_t>(payload->get_length()));
            }
            its_endpoint->send(protocol::create_distribute_security_policy_cmd(VSOMEIP_ROUTING_CLIENT, data));
        }
    } else
        VSOMEIP_WARNING_P << "Could not send cached security policies to registering client: 0x" << hex4(_client);

    return false;
}

bool routing_manager_stub::send_remove_security_policy_request(client_t _client, pending_security_update_id_t _update_id, uid_t _uid,
                                                               gid_t _gid) {

    if (auto its_endpoint = find_local_routing_endpoint(_client); its_endpoint) {
        return its_endpoint->send(protocol::create_remove_security_policy_cmd(_client, _update_id, _uid, _gid));
    } else {
        VSOMEIP_ERROR_P << "Cannot find local client endpoint for client " << get_client_info(_client);
    }

    return false;
}

bool routing_manager_stub::add_requester_policies(uid_t _uid, gid_t _gid, const std::set<std::shared_ptr<policy>>& _policies) {

    std::scoped_lock its_lock{requester_policies_mutex_};
    auto found_uid = requester_policies_.find(_uid);
    if (found_uid != requester_policies_.end()) {
        auto found_gid = found_uid->second.find(_gid);
        if (found_gid != found_uid->second.end()) {
            found_gid->second.insert(_policies.begin(), _policies.end());
        } else {
            found_uid->second[_gid] = _policies;
        }
    } else {
        requester_policies_[_uid][_gid] = _policies;
    }

    // Check whether clients with uid/gid are already registered.
    // If yes, update their policy
    std::unordered_set<client_t> its_clients;
    configuration_->get_policy_manager()->get_clients(_uid, _gid, its_clients);

    if (!its_clients.empty())
        return send_requester_policies(its_clients, _policies);

    return true;
}

void routing_manager_stub::remove_requester_policies(uid_t _uid, gid_t _gid) {

    std::scoped_lock its_lock{requester_policies_mutex_};
    auto found_uid = requester_policies_.find(_uid);
    if (found_uid != requester_policies_.end()) {
        found_uid->second.erase(_gid);
        if (found_uid->second.empty())
            requester_policies_.erase(_uid);
    }
}

void routing_manager_stub::get_requester_policies(uid_t _uid, gid_t _gid, std::set<std::shared_ptr<policy>>& _policies) const {

    std::scoped_lock its_lock{requester_policies_mutex_};
    auto found_uid = requester_policies_.find(_uid);
    if (found_uid != requester_policies_.end()) {
        auto found_gid = found_uid->second.find(_gid);
        if (found_gid != found_uid->second.end())
            _policies = found_gid->second;
    }
}

void routing_manager_stub::add_pending_security_update_handler(pending_security_update_id_t _id,
                                                               const security_update_handler_t& _handler) {

    std::scoped_lock its_lock(security_update_handlers_mutex_);
    security_update_handlers_[_id] = _handler;
}

void routing_manager_stub::add_pending_security_update_timer(pending_security_update_id_t _id) {

    std::shared_ptr<boost::asio::steady_timer> its_timer = std::make_shared<boost::asio::steady_timer>(io_);
    its_timer->expires_after(std::chrono::milliseconds(3000));

    auto its_me{shared_from_this()};
    its_timer->async_wait([its_me, _id, its_timer](const boost::system::error_code& _error) {
        its_me->on_security_update_timeout(_error, _id, its_timer);
    });

    std::scoped_lock its_lock{security_update_timers_mutex_};
    security_update_timers_[_id] = its_timer;
}

bool routing_manager_stub::send_requester_policies(const std::unordered_set<client_t>& _clients,
                                                   const std::set<std::shared_ptr<policy>>& _policies) {

    pending_security_update_id_t its_policy_id;

    // serialize the policies and send them...
    for (const auto& p : _policies) {
        std::vector<byte_t> its_policy_data;
        if (p->serialize(its_policy_data)) {
            std::vector<byte_t> its_message;
            its_message.push_back(byte_t(protocol::id_e::UPDATE_SECURITY_POLICY_INT_ID));

            // version
            its_message.push_back(0);
            its_message.push_back(0);

            // client identifier
            its_message.push_back(0);
            its_message.push_back(0);

            uint32_t its_policy_size = static_cast<uint32_t>(its_policy_data.size() + sizeof(uint32_t));

            uint8_t new_its_policy_size[4] = {0};
            bithelper::write_uint32_le(its_policy_size, new_its_policy_size);
            its_message.insert(its_message.end(), new_its_policy_size, new_its_policy_size + sizeof(new_its_policy_size));

            its_policy_id = pending_security_update_add(_clients);
            uint8_t new_its_policy_id[4] = {0};
            bithelper::write_uint32_le(its_policy_id, new_its_policy_id);
            its_message.insert(its_message.end(), new_its_policy_id, new_its_policy_id + sizeof(new_its_policy_id));
            its_message.insert(its_message.end(), its_policy_data.begin(), its_policy_data.end());

            for (const auto c : _clients) {
                if (auto its_endpoint = find_local_routing_endpoint(c); its_endpoint) {
                    send_local(its_endpoint, its_message);
                }
            }
        }
    }

    return true;
}

void routing_manager_stub::on_security_update_timeout(const boost::system::error_code& _error, pending_security_update_id_t _id,
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
        std::scoped_lock its_lock{security_update_timers_mutex_};
        security_update_timers_.erase(_id);
    }
    {
        //  print missing responses and check if some clients did not respond because they already
        //  disconnected
        if (!its_missing_clients.empty()) {
            for (auto its_client : its_missing_clients) {
                VSOMEIP_INFO_P << "Client 0x" << hex4(its_client) << " did not respond to the policy update/removal with ID: 0x"
                               << hex8(_id);
                if (!find_local_routing_endpoint(its_client)) {
                    VSOMEIP_INFO_P << "Client 0x" << hex4(its_client)
                                   << " is not connected anymore, do not expect answer for policy update/removal with ID: 0x" << hex8(_id);
                    pending_security_update_remove(_id, its_client);
                }
            }
        }

        its_missing_clients = pending_security_update_get(_id);
        if (its_missing_clients.empty()) {
            VSOMEIP_INFO_P << "Received all responses for security update/removal ID: 0x" << hex8(_id);
            its_state = security_update_state_e::SU_SUCCESS;
        }
        {
            // erase pending security update
            std::scoped_lock its_lock{pending_security_updates_mutex_};
            pending_security_updates_.erase(_id);
        }

        // call handler with error on timeout or with SUCCESS if missing clients are not connected
        std::scoped_lock its_lock(security_update_handlers_mutex_);
        const auto found_handler = security_update_handlers_.find(_id);
        if (found_handler != security_update_handlers_.end()) {
            found_handler->second(its_state);
            security_update_handlers_.erase(found_handler);
        } else {
            VSOMEIP_WARNING_P << "Callback not found for security update/removal with ID: 0x" << hex8(_id);
        }
    }
}

bool routing_manager_stub::update_security_policy_configuration(uid_t _uid, gid_t _gid, const std::shared_ptr<policy>& _policy,
                                                                const std::shared_ptr<payload>& _payload,
                                                                const security_update_handler_t& _handler) {

    bool ret(true);

    // cache security policy payload for later distribution to new registering clients
    policy_cache_add(_uid, _payload);

    // update security policy from configuration
    configuration_->get_policy_manager()->update_security_policy(_uid, _gid, _policy);

    // Build requester policies for the services offered by the new policy
    std::set<std::shared_ptr<policy>> its_requesters;
    configuration_->get_policy_manager()->get_requester_policies(_policy, its_requesters);

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
        uint32_t its_tranche = uint32_t(its_clients_to_inform.size() >= 10 ? (its_clients_to_inform.size() / 10) : 1);
        VSOMEIP_INFO_P << "Informing [" << its_clients_to_inform.size()
                       << "] currently connected clients about policy update for UID: " << _uid << " with update ID: 0x" << hex8(its_id);
        for (auto its_client : its_clients_to_inform) {
            if (!send_update_security_policy_request(its_client, its_id, _uid, _payload)) {
                VSOMEIP_INFO_P << "Couldn't send update security policy request to client 0x" << hex4(its_client) << " policy UID: " << _uid
                               << " GID: " << _gid << " with update ID: 0x" << its_id << " as client already disconnected";
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

bool routing_manager_stub::remove_security_policy_configuration(uid_t _uid, gid_t _gid, const security_update_handler_t& _handler) {

    bool ret(true);

    // remove security policy from configuration (only if there was a updateACL call before)
    if (is_policy_cached(_uid)) {
        if (!configuration_->get_policy_manager()->remove_security_policy(_uid, _gid)) {
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
                uint32_t its_tranche = uint32_t(its_clients_to_inform.size() >= 10 ? (its_clients_to_inform.size() / 10) : 1);
                VSOMEIP_INFO_P << "Informing [" << its_clients_to_inform.size()
                               << "] currently connected clients about policy removal for UID: " << _uid << " with update ID: " << its_id;
                for (auto its_client : its_clients_to_inform) {
                    if (!send_remove_security_policy_request(its_client, its_id, _uid, _gid)) {
                        VSOMEIP_INFO_P << "Couldn't send remove security policyrequest to client 0x" << hex4(its_client)
                                       << " policy UID: " << _uid << " GID: " << _gid << " with update ID: 0x" << its_id
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
    } else {
        _handler(security_update_state_e::SU_UNKNOWN_USER_ID);
        ret = false;
    }
    return ret;
}

pending_security_update_id_t routing_manager_stub::pending_security_update_add(const std::unordered_set<client_t>& _clients) {
    std::scoped_lock its_lock{pending_security_updates_mutex_};
    if (++pending_security_update_id_ == 0) {
        pending_security_update_id_++;
    }
    pending_security_updates_[pending_security_update_id_] = _clients;

    return pending_security_update_id_;
}

std::unordered_set<client_t> routing_manager_stub::pending_security_update_get(pending_security_update_id_t _id) {
    std::scoped_lock its_lock{pending_security_updates_mutex_};
    std::unordered_set<client_t> its_missing_clients;
    auto found_si = pending_security_updates_.find(_id);
    if (found_si != pending_security_updates_.end()) {
        its_missing_clients = pending_security_updates_[_id];
    }
    return its_missing_clients;
}

bool routing_manager_stub::pending_security_update_remove(pending_security_update_id_t _id, client_t _client) {
    std::scoped_lock its_lock{pending_security_updates_mutex_};
    auto found_si = pending_security_updates_.find(_id);
    if (found_si != pending_security_updates_.end()) {
        if (found_si->second.erase(_client)) {
            return true;
        }
    }
    return false;
}

bool routing_manager_stub::is_pending_security_update_finished(pending_security_update_id_t _id) {
    std::scoped_lock its_lock{pending_security_updates_mutex_};
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

void routing_manager_stub::on_security_update_response(pending_security_update_id_t _id, client_t _client) {
    if (pending_security_update_remove(_id, _client)) {
        if (is_pending_security_update_finished(_id)) {
            // cancel timeout timer
            {
                std::scoped_lock its_lock{security_update_timers_mutex_};
                auto found_timer = security_update_timers_.find(_id);
                if (found_timer != security_update_timers_.end()) {
                    found_timer->second->cancel();
                    security_update_timers_.erase(found_timer);
                } else {
                    VSOMEIP_WARNING_P << "Received all responses for security update/removal ID: 0x" << hex8(_id)
                                      << " but timeout already happened";
                }
            }

            // call handler
            {
                std::scoped_lock its_lock(security_update_handlers_mutex_);
                auto found_handler = security_update_handlers_.find(_id);
                if (found_handler != security_update_handlers_.end()) {
                    found_handler->second(security_update_state_e::SU_SUCCESS);
                    security_update_handlers_.erase(found_handler);
                    VSOMEIP_INFO_P << "Received all responses for security update/removal ID: 0x" << hex8(_id);
                } else {
                    VSOMEIP_WARNING_P << "Received all responses for security update/removal ID: 0x" << hex8(_id)
                                      << " but didn't find handler";
                }
            }
        }
    }
}
#endif // !VSOMEIP_DISABLE_SECURITY

void routing_manager_stub::send_suspend() const {

    broadcast(protocol::create_suspend_cmd(VSOMEIP_ROUTING_CLIENT));
}

std::shared_ptr<local_endpoint> routing_manager_stub::find_local_routing_endpoint(client_t _client) const {
    if (auto epm = host_->get_endpoint_manager(); epm) {
        return epm->find_routing_endpoint(_client);
    }
    return nullptr;
}

bool routing_manager_stub::send_local(std::shared_ptr<local_endpoint> const& _ep, std::vector<byte_t> const& _data) {
    if (std::numeric_limits<uint32_t>::max() < (_data.size())) {
        VSOMEIP_ERROR_P << "Failed for client: 0x" << hex4(_ep->connected_client())
                        << ", command: " << protocol::read_command_id(_data.data(), _data.size())
                        << ", as the message exceeded the max length";
        return false;
    }
    return _ep->send(&_data[0], static_cast<uint32_t>(_data.size()));
}

} // namespace vsomeip_v3
