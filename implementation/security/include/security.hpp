// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SECURITY_SECURITY_HPP_
#define VSOMEIP_V3_SECURITY_SECURITY_HPP_

#include <memory>
#include <unordered_set>

#include <vsomeip/payload.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip_v3 {

struct configuration_element;

class security {
public:
    VSOMEIP_EXPORT static std::shared_ptr<security> get();

    virtual ~security() {};

    virtual void load(const configuration_element &_element) = 0;

    virtual bool is_enabled() const = 0;
    virtual bool is_audit() const = 0;

    virtual bool check_credentials(client_t _client, uid_t _uid, gid_t _gid) = 0;
    virtual bool check_routing_credentials(client_t _client,
            uint32_t _uid, uint32_t _gid) const = 0;

    virtual bool is_client_allowed(uint32_t _uid, uint32_t _gid, client_t _client,
            service_t _service, instance_t _instance, method_t _method,
            bool _is_request_service = false) const = 0;
    virtual bool is_remote_client_allowed() const = 0;
    virtual bool is_offer_allowed(uint32_t _uid, uint32_t _gid, client_t _client,
            service_t _service, instance_t _instance) const = 0;

    virtual void update_security_policy(uint32_t _uid, uint32_t _gid,
            const std::shared_ptr<policy>& _policy) = 0;
    virtual bool remove_security_policy(uint32_t _uid, uint32_t _gid) = 0;

    virtual bool get_uid_gid_to_client_mapping(std::pair<uint32_t, uint32_t> _uid_gid,
            std::set<client_t> &_clients) = 0;
    virtual bool remove_client_to_uid_gid_mapping(client_t _client) = 0;

    virtual bool get_client_to_uid_gid_mapping(client_t _client,
            std::pair<uint32_t, uint32_t> &_uid_gid) = 0;

    virtual bool store_client_to_uid_gid_mapping(client_t _client,
        uint32_t _uid, uint32_t _gid) = 0;
    virtual void store_uid_gid_to_client_mapping(uint32_t _uid, uint32_t _gid,
        client_t _client) = 0;

    virtual void get_requester_policies(const std::shared_ptr<policy> _policy,
            std::set<std::shared_ptr<policy> > &_requesters) const = 0;
    virtual void get_clients(uid_t _uid, gid_t _gid,
            std::unordered_set<client_t> &_clients) const = 0;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_SECURITY_SECURITY_HPP_
