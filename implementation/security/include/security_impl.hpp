// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_SECURITY_IMPL_HPP_
#define VSOMEIP_V3_SECURITY_IMPL_HPP_

#include <map>
#include <mutex>
#include <vector>

#include <boost/property_tree/ptree.hpp>

#include "../include/policy.hpp"
#include "../include/security.hpp"

namespace vsomeip_v3 {

class security_impl :
        public security {
public:
    static std::shared_ptr<security_impl> get();

    security_impl();

    void load(const configuration_element &_element);

    bool is_enabled() const;
    bool is_audit() const;

    bool check_credentials(client_t _client, uid_t _uid, gid_t _gid);
    bool check_routing_credentials(client_t _client, uint32_t _uid, uint32_t _gid) const;

    bool is_client_allowed(uint32_t _uid, uint32_t _gid, client_t _client,
            service_t _service, instance_t _instance, method_t _method,
            bool _is_request_service = false) const;
    bool is_offer_allowed(uint32_t _uid, uint32_t _gid, client_t _client,
            service_t _service, instance_t _instance) const;

    void update_security_policy(uint32_t _uid, uint32_t _gid, const std::shared_ptr<policy>& _policy);
    bool remove_security_policy(uint32_t _uid, uint32_t _gid);

    void add_security_credentials(uint32_t _uid, uint32_t _gid,
            const std::shared_ptr<policy>& _credentials_policy, client_t _client);

    bool is_remote_client_allowed() const;

    bool is_policy_update_allowed(uint32_t _uid, std::shared_ptr<policy> &_policy) const;

    bool is_policy_removal_allowed(uint32_t _uid) const;

    bool parse_policy(const byte_t* &_buffer, uint32_t &_buffer_size,
            uint32_t &_uid, uint32_t &_gid, const std::shared_ptr<policy> &_policy) const;

    bool get_uid_gid_to_client_mapping(std::pair<uint32_t, uint32_t> _uid_gid, std::set<client_t> &_clients);
    bool remove_client_to_uid_gid_mapping(client_t _client);

    bool get_client_to_uid_gid_mapping(client_t _client, std::pair<uint32_t, uint32_t> &_uid_gid);
    bool store_client_to_uid_gid_mapping(client_t _client, uint32_t _uid, uint32_t _gid);
    void store_uid_gid_to_client_mapping(uint32_t _uid, uint32_t _gid, client_t _client);

    void get_requester_policies(const std::shared_ptr<policy> _policy,
            std::set<std::shared_ptr<policy> > &_requesters) const;
    void get_clients(uid_t _uid, gid_t _gid, std::unordered_set<client_t> &_clients) const;

private:

    // Configuration
    void load_policies(const configuration_element &_element);
    void load_policy(const boost::property_tree::ptree &_tree);
    void load_policy_body(std::shared_ptr<policy> &_policy,
            const boost::property_tree::ptree::const_iterator &_tree);
    void load_credential(const boost::property_tree::ptree &_tree,
            boost::icl::interval_map<uid_t, boost::icl::interval_set<gid_t> > &_ids);
    bool load_routing_credentials(const configuration_element &_element);
    template<typename T_>
    void load_interval_set(const boost::property_tree::ptree &_tree,
            boost::icl::interval_set<T_> &_range, bool _exclude_margins = false);
    void load_security_update_whitelist(const configuration_element &_element);

private:
    client_t routing_client_;

    mutable std::mutex ids_mutex_;
    mutable std::mutex uid_to_clients_mutex_;

    std::vector<std::shared_ptr<policy> > any_client_policies_;

    mutable std::mutex  any_client_policies_mutex_;
    std::map<client_t, std::pair<uint32_t, uint32_t> > ids_;
    std::map<std::pair<uint32_t, uint32_t>, std::set<client_t> > uid_to_clients_;

    bool policy_enabled_;
    bool check_credentials_;
    bool check_routing_credentials_;
    bool allow_remote_clients_;
    bool check_whitelist_;

    mutable std::mutex service_interface_whitelist_mutex_;
    boost::icl::interval_set<service_t> service_interface_whitelist_;

    mutable std::mutex uid_whitelist_mutex_;
    boost::icl::interval_set<uint32_t> uid_whitelist_;

    mutable std::mutex routing_credentials_mutex_;
    std::pair<uint32_t, uint32_t> routing_credentials_;

    bool is_configured_;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_SECURITY_IMPL_HPP_
