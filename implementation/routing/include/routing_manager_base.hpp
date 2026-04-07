// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <mutex>
#include <unordered_set>
#include <queue>
#include <condition_variable>

#include <vsomeip/constants.hpp>
#include <vsomeip/vsomeip_sec.h>

#include "types.hpp"
#include "event.hpp"
#include "serviceinfo.hpp"
#include "routing_host.hpp"
#include "eventgroupinfo.hpp"
#include "routing_manager_host.hpp"
#include "local_service_table.hpp"

#include "../../message/include/serializer.hpp"
#include "../../message/include/deserializer.hpp"
#include "../../protocol/include/protocol.hpp"
#include "../../configuration/include/configuration.hpp"
#include "../../configuration/include/debounce_filter_impl.hpp"
#include "../../endpoints/include/endpoint_manager_base.hpp"

#if defined(__QNX__)
#include "../../utility/include/qnx_helper.hpp"
#endif
#include "../../utility/include/service_instance_map.hpp"

namespace vsomeip_v3 {

namespace trace {
class connector_impl;
} // namespace trace

class serializer;
class local_endpoint;

class routing_manager_base {

public:
    routing_manager_base(routing_manager_host* _host);
    virtual ~routing_manager_base() = default;

    virtual boost::asio::io_context& get_io();
    virtual client_t get_client() const;

    virtual std::string get_client_host() const;
    virtual void set_client_host(const std::string& _client_host);
    virtual void set_client(const client_t& _client);
    virtual session_t get_session(bool _is_request);

    virtual vsomeip_sec_client_t get_sec_client() const;
    virtual void set_sec_client_port(port_t _port);

    virtual void init() = 0;

    virtual void request_service(client_t _client, service_t _service, instance_t _instance, major_version_t _major,
                                 minor_version_t _minor);

    virtual void release_service(client_t _client, service_t _service, instance_t _instance);

    virtual void subscribe(client_t _client, const vsomeip_sec_client_t* _sec_client, service_t _service, instance_t _instance,
                           eventgroup_t _eventgroup, major_version_t _major, event_t _event,
                           const std::shared_ptr<debounce_filter_impl_t>& _filter) = 0;

    virtual bool send(client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance, bool _reliable,
                      client_t _bound_client = VSOMEIP_ROUTING_CLIENT, const vsomeip_sec_client_t* _sec_client = nullptr,
                      uint8_t _status_check = 0, bool _sent_from_remote = false, bool _force = true) = 0;

    virtual void register_client_error_handler(client_t _client, const std::shared_ptr<local_endpoint>& _endpoint) = 0;

    virtual void send_get_offered_services_info(client_t _client, offer_type_e _offer_type) = 0;

    virtual std::shared_ptr<serviceinfo> find_service(service_t _service, instance_t _instance) const = 0;

    std::string const& get_name() const;

protected:
    [[nodiscard]] virtual bool is_local_client(client_t _client) const = 0;
    /**
     * \brief Log network state
     *
     * Uses /proc/net/tcp + /proc/net/udp to log network connections. This is, of course, only for IPv4
     *
     * \param _tcp if true, logs TCP connections, otherwise UDP connections
     * \param _only_external if true, logs *only* external connections (e.g., service-discovery), otherwise everything
     */
    void log_network_state(bool _tcp, bool _only_external) const;

    bool send_local(std::shared_ptr<local_endpoint>& _target, client_t _client, const byte_t* _data, uint32_t _size, instance_t _instance,
                    bool _reliable, protocol::id_e _command, uint8_t _status_check, client_t _sender) const;

    std::shared_ptr<serializer> get_serializer();
    void put_serializer(const std::shared_ptr<serializer>& _serializer);
    std::shared_ptr<deserializer> get_deserializer();
    void put_deserializer(const std::shared_ptr<deserializer>& _deserializer);

    virtual void send_subscribe(client_t _client, service_t _service, instance_t _instance, eventgroup_t _eventgroup,
                                major_version_t _major, event_t _event, const std::shared_ptr<debounce_filter_impl_t>& _filter) = 0;

protected:
    routing_manager_host* host_;
    boost::asio::io_context& io_;

    std::shared_ptr<configuration> configuration_;

    std::queue<std::shared_ptr<serializer>> serializers_;
    std::mutex serializer_mutex_;
    std::condition_variable serializer_condition_;

    std::queue<std::shared_ptr<deserializer>> deserializers_;
    std::mutex deserializer_mutex_;
    std::condition_variable deserializer_condition_;

    mutable std::mutex env_mutex_;
    std::string env_;

    std::shared_ptr<trace::connector_impl> tc_;
};

} // namespace vsomeip_v3
