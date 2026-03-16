// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../routing/include/routing_manager_host.hpp"

#include "../../security/include/security.hpp"

#include <vsomeip/enumeration_types.hpp>
#include <vsomeip/payload.hpp>
#include <vsomeip/handler.hpp>

#include <boost/asio/executor_work_guard.hpp>

#include <thread>

namespace vsomeip_v3 {

class routing_manager_impl;

class routing_application : public routing_manager_host {
public:
    routing_application(boost::asio::io_context& _io, std::shared_ptr<configuration> _configuration, std::string _name);
    ~routing_application();

    void start() const;
    void stop() const;

    void set_routing_state(routing_state_e _routing_state) const;
    bool update_service_configuration(service_t _service, instance_t _instance, std::uint16_t _port, bool _reliable,
                                      bool _magic_cookies_enabled, bool _offer) const;

#ifndef VSOMEIP_DISABLE_SECURITY
    void update_security_policy_configuration(uint32_t _uid, uint32_t _gid, std::shared_ptr<policy> _policy,
                                              std::shared_ptr<payload> _payload, const security_update_handler_t& _handler) const;
    void remove_security_policy_configuration(uint32_t _uid, uint32_t _gid, const security_update_handler_t& _handler) const;
#endif

    void register_message_acceptance_handler(const message_acceptance_handler_t& _handler) const;
    void register_sd_acceptance_handler(const sd_acceptance_handler_t& _handler) const;

    void register_reboot_notification_handler(const reboot_notification_handler_t& _handler) const;
    void set_sd_acceptance_required(const remote_info_t& _remote, const std::string& _path, bool _enable) const;

    void register_routing_ready_handler(const routing_ready_handler_t& _handler) const;
    void register_routing_state_handler(const routing_state_handler_t& _handler) const;

    connection_control_response_e change_connection_control(connection_control_request_e _control, const std::string& _guest_address) const;

private:
    // routing_manager_host interface
    client_t get_client() const override;
    void set_client(const client_t& _client) override;
    session_t get_session(bool _is_request) override;

    vsomeip_sec_client_t get_sec_client() const override;
    void set_sec_client_port(port_t _port) override;

    const std::string& get_name() const override;
    std::shared_ptr<configuration> get_configuration() const override;
    boost::asio::io_context& get_io() override;

    void on_availability(service_t _service, instance_t _instance, availability_state_e _state, major_version_t _major = DEFAULT_MAJOR,
                         minor_version_t _minor = DEFAULT_MINOR) override;
    void on_state(state_type_e _state) override;
    void on_message(std::shared_ptr<message>&& _message) override;
    void on_subscription(service_t _service, instance_t _instance, eventgroup_t _eventgroup, client_t _client,
                         const vsomeip_sec_client_t* _sec_client, const std::string& _env, bool _subscribed,
                         const std::function<void(bool)>& _accepted_cb) override;
    void on_subscription_status(service_t _service, instance_t _instance, eventgroup_t _eventgroup, event_t _event,
                                uint16_t _error) override;
    void send(std::shared_ptr<message> _message) override;
    void on_offered_services_info(std::vector<std::pair<service_t, instance_t>>& _services) override;
    bool is_routing() const override;

private:
    vsomeip_sec_client_t sec_client_;

    boost::asio::io_context& io_;
    std::mutex mtx_;
    session_t session_{0};

    // to suffice the routing_manager_host interface
    std::string const name_;
    std::shared_ptr<configuration> const configuration_;
    std::shared_ptr<routing_manager_impl> const routing_;
    bool const has_session_handling_;
};
}
