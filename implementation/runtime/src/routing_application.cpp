// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/routing_application.hpp"

#include "../../endpoints/include/boardnet_endpoint.hpp"
#include "../../routing/include/routing_manager_impl.hpp"
#include "../../tracing/include/connector_impl.hpp"
#include "../../utility/include/utility.hpp"

#include "logger_ext.hpp"

#if defined(__linux__)
#include <dlfcn.h>
#include <sys/syscall.h>
#endif

#define VSOMEIP_LOG_PREFIX "rapp"

namespace vsomeip_v3 {

routing_application::routing_application(boost::asio::io_context& _io, std::shared_ptr<configuration> _configuration, std::string _name) :
    io_(_io), name_(std::move(_name)), configuration_(std::move(_configuration)), routing_(std::make_shared<routing_manager_impl>(this)),
    has_session_handling_(configuration_->has_session_handling(name_)) {

    if (configuration_->is_local_routing()) {
        sec_client_.port = VSOMEIP_SEC_PORT_UNUSED;
#ifdef __unix__
        sec_client_.user = getuid();
        sec_client_.group = getgid();
#else
        sec_client_.user = ANY_UID;
        sec_client_.group = ANY_GID;
#endif
    } else {
        if (auto its_guest_address = configuration_->get_routing_guest_address(); its_guest_address.is_v4()) {
            sec_client_.host = htonl(its_guest_address.to_v4().to_uint());
        }
        sec_client_.port = VSOMEIP_SEC_PORT_UNSET;
    }

    routing_->init();
}

routing_application::~routing_application() = default;

void routing_application::start() const {
    routing_->start();
}

void routing_application::stop() const {
    routing_->stop();
}

void routing_application::set_routing_state(routing_state_e _routing_state) const {
    routing_->set_routing_state(_routing_state);
}

bool routing_application::update_service_configuration(service_t _service, instance_t _instance, std::uint16_t _port, bool _reliable,
                                                       bool _magic_cookies_enabled, bool _offer) const {
    bool ret{false};
    if (_offer) {
        ret = routing_->offer_service_remotely(_service, _instance, _port, _reliable, _magic_cookies_enabled);
    } else {
        ret = routing_->stop_offer_service_remotely(_service, _instance, _port, _reliable, _magic_cookies_enabled);
    }
    return ret;
}
#ifndef VSOMEIP_DISABLE_SECURITY
void routing_application::update_security_policy_configuration(uint32_t _uid, uint32_t _gid, ::std::shared_ptr<policy> _policy,
                                                               std::shared_ptr<payload> _payload,
                                                               const security_update_handler_t& _handler) const {
    routing_->update_security_policy_configuration(_uid, _gid, _policy, _payload, _handler);
}

void routing_application::remove_security_policy_configuration(uint32_t _uid, uint32_t _gid,
                                                               const security_update_handler_t& _handler) const {
    routing_->remove_security_policy_configuration(_uid, _gid, _handler);
}
#endif // VSOMEIP_DISABLE_SECURITY

void routing_application::register_message_acceptance_handler(const message_acceptance_handler_t& _handler) const {
    routing_->register_message_acceptance_handler(_handler);
}

void routing_application::register_sd_acceptance_handler(const sd_acceptance_handler_t& _handler) const {
    routing_->register_sd_acceptance_handler(_handler);
}

void routing_application::register_reboot_notification_handler(const reboot_notification_handler_t& _handler) const {
    routing_->register_reboot_notification_handler(_handler);
}

void routing_application::set_sd_acceptance_required(const remote_info_t& _remote, const std::string& _path, bool _enable) const {

    const boost::asio::ip::address its_address(
            _remote.ip_.is_v4_ ? static_cast<boost::asio::ip::address>(boost::asio::ip::address_v4(_remote.ip_.address_.v4_))
                               : static_cast<boost::asio::ip::address>(boost::asio::ip::address_v6(_remote.ip_.address_.v6_)));

    if (_remote.first_ == std::numeric_limits<std::uint16_t>::max() && _remote.last_ == 0) {
        // special case to (de)activate rules per IP
        configuration_->set_sd_acceptance_rules_active(its_address, _enable);
        return;
    }

    port_range_t its_range{_remote.first_, _remote.last_};
    configuration_->set_sd_acceptance_rule(its_address, its_range, port_type_e::PT_UNKNOWN, _path, _remote.is_reliable_, _enable, true);

    if (_enable) {
        routing_->sd_acceptance_enabled(its_address, its_range, _remote.is_reliable_);
    }
}
void routing_application::register_routing_ready_handler(const routing_ready_handler_t& _handler) const {
    routing_->register_routing_ready_handler(_handler);
}

void routing_application::register_routing_state_handler(const routing_state_handler_t& _handler) const {
    routing_->register_routing_state_handler(_handler);
}

connection_control_response_e routing_application::change_connection_control(connection_control_request_e _control,
                                                                             const std::string& _guest_address) const {
    boost::asio::ip::address its_addr;
    try {
        its_addr = boost::asio::ip::make_address(_guest_address);
    } catch (...) {
        VSOMEIP_ERROR_P << "could not parse address '" << _guest_address << "'";
        return connection_control_response_e::CCR_ERROR_INVALID_PARAMETER;
    }

    if (_control != connection_control_request_e::CCR_ACCEPT && _control != connection_control_request_e::CCR_RESET_AND_BLOCK) {
        VSOMEIP_ERROR_P << "control parameter was neither CCR_ACCEPT, nor CCR_RESET_AND_BLOCK for address '" << _guest_address << "'";
        return connection_control_response_e::CCR_ERROR_INVALID_PARAMETER;
    }

    VSOMEIP_INFO_P << "changing connection control to '"
                   << ((_control == connection_control_request_e::CCR_ACCEPT) ? "CCR_ACCEPT" : "CCR_RESET_AND_BLOCK") << "' for address '"
                   << _guest_address << "'";

    return routing_->change_connection_control(_control, its_addr);
}

bool routing_application::is_routing() const {
    return true;
}

vsomeip_sec_client_t routing_application::get_sec_client() const {
    return sec_client_;
}

void routing_application::set_sec_client_port(port_t _port) {
    sec_client_.port = htons(_port);
}

const std::string& routing_application::get_name() const {
    return name_;
}

std::shared_ptr<configuration> routing_application::get_configuration() const {
    return configuration_;
}

boost::asio::io_context& routing_application::get_io() {
    return io_;
}

client_t routing_application::get_client() const {
    return VSOMEIP_ROUTING_CLIENT;
}

void routing_application::set_client([[maybe_unused]] const client_t& _client) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}

session_t routing_application::get_session([[maybe_unused]] bool _is_request) {
    if (!has_session_handling_ && !_is_request) {
        return 0;
    }

    std::scoped_lock its_lock{mtx_};
    if (0 == ++session_) {
        // Smallest allowed session identifier
        session_ = 1;
    }

    return session_;
}

void routing_application::on_availability(service_t _service, instance_t _instance, [[maybe_unused]] availability_state_e _state,
                                          [[maybe_unused]] major_version_t _major, [[maybe_unused]] minor_version_t _minor) {
    VSOMEIP_ERROR_P << "Not supposed to be called: " << hex4(_service) << "." << hex4(_instance);
}

void routing_application::on_state([[maybe_unused]] state_type_e _state) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}

void routing_application::on_message([[maybe_unused]] std::shared_ptr<message>&& _message) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}

void routing_application::on_subscription([[maybe_unused]] service_t _service, [[maybe_unused]] instance_t _instance,
                                          [[maybe_unused]] eventgroup_t _eventgroup, [[maybe_unused]] client_t _client,
                                          [[maybe_unused]] const vsomeip_sec_client_t* _sec_client,
                                          [[maybe_unused]] const std::string& _env, [[maybe_unused]] bool _subscribed,
                                          [[maybe_unused]] const std::function<void(bool)>& _accepted_cb) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}

void routing_application::on_subscription_status([[maybe_unused]] service_t _service, [[maybe_unused]] instance_t _instance,
                                                 [[maybe_unused]] eventgroup_t _eventgroup, [[maybe_unused]] event_t _event,
                                                 [[maybe_unused]] uint16_t _error) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}

void routing_application::send([[maybe_unused]] std::shared_ptr<message> _message) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}

void routing_application::on_offered_services_info([[maybe_unused]] std::vector<std::pair<service_t, instance_t>>& _services) {
    VSOMEIP_ERROR_P << "Not supposed to be called";
}
}
