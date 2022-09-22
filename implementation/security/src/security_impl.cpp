// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sstream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <stdlib.h>
#endif

#include <algorithm>

#include <vsomeip/internal/policy_manager.hpp>
#include "../include/security_impl.hpp"
#include "../../configuration/include/configuration_element.hpp"
#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

namespace vsomeip_v3 {

template<typename T_>
void read_data(const std::string &_in, T_ &_out) {
    std::stringstream its_converter;

    if (_in.size() > 2
            && _in[0] == '0'
            && (_in[1] == 'x' || _in[1] == 'X'))
        its_converter << std::hex << _in;
    else
        its_converter << std::dec << _in;

    its_converter >> _out;
}

security_impl::security_impl()
    : policy_enabled_(false),
      check_credentials_(false),
      check_routing_credentials_(false),
      allow_remote_clients_(true),
      check_whitelist_(false),
      is_configured_(false) {
}

void
security_impl::load(const configuration_element &_element) {
    load_policies(_element);
    load_security_update_whitelist(_element);
    load_routing_credentials(_element);

    if (policy_enabled_ && check_credentials_)
        VSOMEIP_INFO << "Security configuration is active.";

    if (policy_enabled_ && !check_credentials_)
        VSOMEIP_INFO << "Security configuration is active but in audit mode (allow all)";
}

bool
security_impl::is_enabled() const {
    return policy_enabled_;
}

bool
security_impl::is_audit() const {
    return check_credentials_;
}

bool
security_impl::check_credentials(client_t _client,
        uid_t _uid, gid_t _gid) {

    if (!policy_enabled_) {
        return true;
    }

    std::vector<std::shared_ptr<policy> > its_policies;
    bool has_id(false);
    {
        std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
        its_policies = any_client_policies_;
    }

    for (const auto &p : its_policies) {
        std::lock_guard<std::mutex> its_policy_lock(p->mutex_);

        bool has_uid, has_gid(false);

        const auto found_uid = p->credentials_.find(_uid);
        has_uid = (found_uid != p->credentials_.end());
        if (has_uid) {
            const auto found_gid = found_uid->second.find(_gid);
            has_gid = (found_gid != found_uid->second.end());
        }

        has_id = (has_uid && has_gid);

        if ((has_id && p->allow_who_) || (!has_id && !p->allow_who_)) {
            if (!store_client_to_uid_gid_mapping(_client,_uid, _gid)) {
                std::string security_mode_text = "!";
                if (!check_credentials_) {
                    security_mode_text = " but will be allowed due to audit mode is active!";
                }
                VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
                        << " with UID/GID=" << std::dec << _uid << "/" << _gid
                        << " : Check credentials failed as existing credentials would be overwritten"
                        << security_mode_text;
                return !check_credentials_;
            }
            store_uid_gid_to_client_mapping(_uid, _gid, _client);
            return true;
        }
    }

    std::string security_mode_text = " ~> Skip!";
    if (!check_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }
    VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
                 << " with UID/GID=" << std::dec << _uid << "/" << _gid
                 << " : Check credentials failed" << security_mode_text;

    return !check_credentials_;
}

bool
security_impl::is_client_allowed(uint32_t _uid, uint32_t _gid, client_t _client,
        service_t _service, instance_t _instance, method_t _method,
        bool _is_request_service) const {

    if (!policy_enabled_) {
        return true;
    }

    uint32_t its_uid(ANY_UID), its_gid(ANY_GID);
    std::vector<std::shared_ptr<policy> > its_policies;
    {
        std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
        its_policies = any_client_policies_;
    }

    if (_uid != ANY_UID && _gid != ANY_GID) {
        its_uid = _uid;
        its_gid = _gid;
    } else {
        std::string security_mode_text = " ~> Skip!";
        if (!check_credentials_) {
            security_mode_text = " but will be allowed due to audit mode is active!";
        }
        VSOMEIP_INFO << "vSomeIP Security: uid/gid " <<  std::dec << _uid << "/" << _gid
                << " for client 0x" << std::hex << _client << " is not valid"
                << ". Therefore it isn't allowed to communicate to service/instance "
                << _service << "/" << _instance
                << security_mode_text;

        return !check_credentials_;
    }

    for (const auto &p : its_policies) {
        std::lock_guard<std::mutex> its_policy_lock(p->mutex_);

        bool has_uid, has_gid(false);
        bool is_matching(false);

        const auto found_uid = p->credentials_.find(_uid);
        has_uid = (found_uid != p->credentials_.end());
        if (has_uid) {
            const auto found_gid = found_uid->second.find(_gid);
            has_gid = (found_gid != found_uid->second.end());
        }

        const auto found_service = p->requests_.find(_service);
        if (found_service != p->requests_.end()) {
            const auto found_instance = found_service->second.find(_instance);
            if (found_instance != found_service->second.end()) {
                if (!_is_request_service) {
                    const auto found_method = found_instance->second.find(_method);
                    is_matching = (found_method != found_instance->second.end());
                } else {
                    // handle VSOMEIP_REQUEST_SERVICE
                    is_matching = true;
                }
            }
        }

        if ((has_uid && has_gid && p->allow_who_) || ((!has_uid || !has_gid) && !p->allow_who_)) {
            if (p->allow_what_) {
                // allow policy
                if (is_matching) {
                    return (true);
                }
            } else {
                // deny policy
                // allow client if the service / instance / !ANY_METHOD was not found
                if ((!is_matching && (_method != ANY_METHOD))
                        // allow client if the service / instance / ANY_METHOD was not found
                        // and it is a "deny nothing" policy
                        || (!is_matching && (_method == ANY_METHOD) && p->requests_.empty())) {
                    return (true);
                }
            }
        }
    }

    std::string security_mode_text = " ~> Skip!";
    if (!check_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }

    VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
            << " with UID/GID=" << std::dec << its_uid << "/" << its_gid
            << " : Isn't allowed to communicate with service/instance/(method / event) " << std::hex
            << _service << "/" << _instance << "/" << _method
            << security_mode_text;

    return (!check_credentials_);
}

bool
security_impl::is_offer_allowed(uint32_t _uid, uint32_t _gid, client_t _client, service_t _service,
        instance_t _instance) const {
    if (!policy_enabled_) {
        return true;
    }

    uint32_t its_uid(ANY_UID), its_gid(ANY_GID);
    std::vector<std::shared_ptr<policy> > its_policies;
    {
        std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
        its_policies = any_client_policies_;
    }

    if (_uid != ANY_UID
            && _gid != ANY_GID) {
        its_uid = _uid;
        its_gid = _gid;
    } else {
        std::string security_mode_text = " ~> Skip offer!";
        if (!check_credentials_) {
            security_mode_text = " but will be allowed due to audit mode is active!";
        }
        VSOMEIP_INFO << "vSomeIP Security: uid/gid " <<  std::dec << _uid << "/" << _gid
                << " for client 0x" << std::hex << _client << " is not valid"
                << ". Therefore it isn't allowed to offer service/instance "
                << _service << "/" << _instance
                << security_mode_text;

        return !check_credentials_;
    }

    for (const auto &p : its_policies) {
        std::lock_guard<std::mutex> its_policy_lock(p->mutex_);
        bool has_uid, has_gid(false), has_offer(false);

        const auto found_uid = p->credentials_.find(_uid);
        has_uid = (found_uid != p->credentials_.end());
        if (has_uid) {
            const auto found_gid = found_uid->second.find(_gid);
            has_gid = (found_gid != found_uid->second.end());
        }

        const auto found_service = p->offers_.find(_service);
        if (found_service != p->offers_.end()) {
            const auto found_instance = found_service->second.find(_instance);
            has_offer = (found_instance != found_service->second.end());
        }

        if ((has_uid && has_gid && p->allow_who_)
                || ((!has_uid || !has_gid) && !p->allow_who_)) {
            if (p->allow_what_ == has_offer) {
                return (true);
            }
        }
    }

    std::string security_mode_text = " ~> Skip offer!";
    if (!check_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }

    VSOMEIP_INFO << "vSomeIP Security: Client 0x"
            << std::hex << _client
            << " with UID/GID="
            << std::dec << its_uid << "/" << its_gid
            << " isn't allowed to offer service/instance "
            << std::hex << _service << "/" << _instance
            << security_mode_text;

    return (!check_credentials_);
}

bool
security_impl::store_client_to_uid_gid_mapping(
        client_t _client, uint32_t _uid, uint32_t _gid) {
    {
        // store the client -> (uid, gid) mapping
        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        auto found_client = ids_.find(_client);
        if (found_client != ids_.end()) {
            if (found_client->second != std::make_pair(_uid, _gid)) {
                VSOMEIP_WARNING << "vSomeIP Security: Client 0x"
                        << std::hex << _client << " with UID/GID="
                        << std::dec << _uid << "/" << _gid << " : Overwriting existing credentials UID/GID="
                        << std::dec << std::get<0>(found_client->second) << "/"
                        << std::get<1>(found_client->second);
                found_client->second = std::make_pair(_uid, _gid);
                return true;
            }
        } else {
            ids_[_client] = std::make_pair(_uid, _gid);
        }
        return true;
    }
}

bool
security_impl::get_client_to_uid_gid_mapping(client_t _client, std::pair<uint32_t, uint32_t> &_uid_gid) {
    {
        // get the UID / GID of the client
        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        if (ids_.find(_client) != ids_.end()) {
            _uid_gid = ids_[_client];
            return true;
        }
        return false;
    }
}

bool
security_impl::remove_client_to_uid_gid_mapping(client_t _client) {
    std::pair<uint32_t, uint32_t> its_uid_gid;
    bool client_removed(false);
    bool uid_gid_removed(false);
    {
        std::lock_guard<std::mutex> its_lock(ids_mutex_);
        auto found_client = ids_.find(_client);
        if (found_client != ids_.end()) {
            its_uid_gid = found_client->second;
            ids_.erase(found_client);
            client_removed = true;
        }
    }
    {
        std::lock_guard<std::mutex> its_lock(uid_to_clients_mutex_);
        if (client_removed) {
            auto found_uid_gid = uid_to_clients_.find(its_uid_gid);
            if (found_uid_gid != uid_to_clients_.end()) {
               auto its_client = found_uid_gid->second.find(_client);
               if (its_client != found_uid_gid->second.end()) {
                   found_uid_gid->second.erase(its_client);
                   if (found_uid_gid->second.empty()) {
                       uid_to_clients_.erase(found_uid_gid);
                   }
                   uid_gid_removed = true;
               }
            }
        } else {
            for (auto its_uid_gid = uid_to_clients_.begin();
                    its_uid_gid != uid_to_clients_.end(); ++its_uid_gid) {
                auto its_client = its_uid_gid->second.find(_client);
                if (its_client != its_uid_gid->second.end()) {
                    its_uid_gid->second.erase(its_client);
                    if (its_uid_gid->second.empty()) {
                        uid_to_clients_.erase(its_uid_gid);
                    }
                    uid_gid_removed = true;
                    break;
                }
            }
        }
    }
    return (client_removed && uid_gid_removed);
}

void
security_impl::store_uid_gid_to_client_mapping(uint32_t _uid, uint32_t _gid,
        client_t _client) {
    {
        // store the uid gid to clients mapping
        std::lock_guard<std::mutex> its_lock(uid_to_clients_mutex_);
        std::set<client_t> mapped_clients;
        if (uid_to_clients_.find(std::make_pair(_uid, _gid)) != uid_to_clients_.end()) {
            mapped_clients = uid_to_clients_[std::make_pair(_uid, _gid)];
            mapped_clients.insert(_client);
            uid_to_clients_[std::make_pair(_uid, _gid)] = mapped_clients;
        } else {
            mapped_clients.insert(_client);
            uid_to_clients_[std::make_pair(_uid, _gid)] = mapped_clients;
        }
    }
}

bool
security_impl::get_uid_gid_to_client_mapping(std::pair<uint32_t, uint32_t> _uid_gid,
        std::set<client_t> &_clients) {
    {
        // get the clients corresponding to uid, gid
        std::lock_guard<std::mutex> its_lock(uid_to_clients_mutex_);
        if (uid_to_clients_.find(_uid_gid) != uid_to_clients_.end()) {
            _clients = uid_to_clients_[_uid_gid];
            return true;
        }
        return false;
    }
}

bool
security_impl::remove_security_policy(uint32_t _uid, uint32_t _gid) {
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    bool was_removed(false);
    if (!any_client_policies_.empty()) {
        std::vector<std::shared_ptr<policy>>::iterator p_it = any_client_policies_.begin();
        while (p_it != any_client_policies_.end()) {
            bool is_matching(false);
            {
                std::lock_guard<std::mutex> its_policy_lock((*p_it)->mutex_);
                bool has_uid(false), has_gid(false);
                const auto found_uid = (*p_it)->credentials_.find(_uid);
                has_uid = (found_uid != (*p_it)->credentials_.end());
                if (has_uid) {
                    const auto found_gid = found_uid->second.find(_gid);
                    has_gid = (found_gid != found_uid->second.end());
                }

                // only remove "credentials allow" policies to prevent removal of
                // blacklist configured in file
                if (has_uid && has_gid && (*p_it)->allow_who_) {
                    is_matching = true;
                }
            }
            if (is_matching) {
                was_removed = true;
                p_it = any_client_policies_.erase(p_it);
            } else {
                ++p_it;
            }
        }
    }
    return (was_removed);
}

void
security_impl::update_security_policy(uint32_t _uid, uint32_t _gid,
        const std::shared_ptr<policy> &_policy) {

    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    std::shared_ptr<policy> its_matching_policy;
    for (auto p : any_client_policies_) {
        if (p->credentials_.size() == 1) {
            const auto its_uids = *(p->credentials_.begin());
            if (its_uids.first.lower() == _uid
                    && its_uids.first.upper() == _uid) {
                if (its_uids.second.size() == 1) {
                    const auto its_gids = *(its_uids.second.begin());
                    if (its_gids.lower() == _gid
                            && its_gids.upper() == _gid) {
                        if (p->allow_who_ == _policy->allow_who_) {
                            its_matching_policy = p;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (its_matching_policy) {
        for (const auto& r : _policy->requests_) {
            service_t its_lower, its_upper;
            get_bounds(r.first, its_lower, its_upper);
            for (auto s = its_lower; s <= its_upper; s++) {
                boost::icl::discrete_interval<service_t> its_service(s, s,
                        boost::icl::interval_bounds::closed());
                its_matching_policy->requests_ += std::make_pair(its_service, r.second);
            }
        }
        for (const auto& o : _policy->offers_) {
            service_t its_lower, its_upper;
            get_bounds(o.first, its_lower, its_upper);
            for (auto s = its_lower; s <= its_upper; s++) {
                boost::icl::discrete_interval<service_t> its_service(s, s,
                        boost::icl::interval_bounds::closed());
                its_matching_policy->offers_ += std::make_pair(its_service, o.second);
            }
        }
    } else {
        any_client_policies_.push_back(_policy);
    }
}

void
security_impl::add_security_credentials(uint32_t _uid, uint32_t _gid,
        const std::shared_ptr<policy> &_policy, client_t _client) {

    bool was_found(false);
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    for (const auto &p : any_client_policies_) {
        bool has_uid(false), has_gid(false);

        std::lock_guard<std::mutex> its_policy_lock(p->mutex_);
        const auto found_uid = p->credentials_.find(_uid);
        has_uid = (found_uid != p->credentials_.end());
        if (has_uid) {
            const auto found_gid = found_uid->second.find(_gid);
            has_gid = (found_gid != found_uid->second.end());
        }

        if (has_uid && has_gid && p->allow_who_) {
            was_found = true;
            break;
        }
    }

    // Do not add the new (credentials-only-policy) if a allow
    // credentials policy with same credentials was found
    if (!was_found) {
        any_client_policies_.push_back(_policy);
        VSOMEIP_INFO << __func__ << " Added security credentials at client: 0x"
                << std::hex << _client << std::dec << " with UID: " << _uid << " GID: " << _gid;
    }
}

bool
security_impl::is_remote_client_allowed() const {
    if (!check_credentials_) {
        return true;
    }
    return allow_remote_clients_;
}

bool
security_impl::is_policy_update_allowed(uint32_t _uid, std::shared_ptr<policy> &_policy) const {

    bool is_uid_allowed(false);
    {
        std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
        const auto found_uid = uid_whitelist_.find(_uid);
        is_uid_allowed = (found_uid != uid_whitelist_.end());
    }

    if (is_uid_allowed) {
        std::lock_guard<std::mutex> its_lock(service_interface_whitelist_mutex_);
        std::lock_guard<std::mutex> its_policy_lock(_policy->mutex_);
        for (auto its_request : _policy->requests_) {
            bool has_service(false);

            service_t its_service(0);
            for (its_service = its_request.first.lower();
                    its_service <= its_request.first.upper();
                    its_service++) {

                const auto found_service = service_interface_whitelist_.find(its_service);
                has_service = (found_service != service_interface_whitelist_.end());
                if (!has_service)
                    break;
            }

            if (!has_service) {
                if (!check_whitelist_) {
                    VSOMEIP_INFO << "vSomeIP Security: Policy update requesting service ID: "
                            << std::hex << its_service
                            << " is not allowed, but will be allowed due to whitelist audit mode is active!";
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Policy update requesting service ID: "
                            << std::hex << its_service
                            << " is not allowed! -> ignore update";
                }
                return (!check_whitelist_);
            }
        }
        return (true);
    } else {
        if (!check_whitelist_) {
            VSOMEIP_INFO << "vSomeIP Security: Policy update for UID: " << std::dec << _uid
                    << " is not allowed, but will be allowed due to whitelist audit mode is active!";
        } else {
            VSOMEIP_WARNING << "vSomeIP Security: Policy update for UID: " << std::dec << _uid
                    << " is not allowed! -> ignore update";
        }
        return (!check_whitelist_);
    }
}

bool
security_impl::is_policy_removal_allowed(uint32_t _uid) const {
    std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
    for (auto its_uid_range : uid_whitelist_) {
        if (its_uid_range.lower() <= _uid && _uid <= its_uid_range.upper()) {
            return (true);
        }
    }

    if (!check_whitelist_) {
        VSOMEIP_INFO << "vSomeIP Security: Policy removal for UID: "
                << std::dec << _uid
                << " is not allowed, but will be allowed due to whitelist audit mode is active!";
    } else {
        VSOMEIP_WARNING << "vSomeIP Security: Policy removal for UID: "
                << std::dec << _uid
                << " is not allowed! -> ignore removal";
    }
    return (!check_whitelist_);
}

bool
security_impl::check_routing_credentials(client_t _client,
        uint32_t _uid, uint32_t _gid) const {

    std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
    if (routing_credentials_.first == _uid
            && routing_credentials_.second == _gid) {

        return (true);
    }

    std::string security_mode_text = "!";
    if (!check_routing_credentials_) {

        security_mode_text = " but will be allowed due to audit mode is active!";
    }

    VSOMEIP_INFO << "vSomeIP Security: Client 0x"
            << std::hex << _client << " and UID/GID="
            << std::dec << _uid << "/" << _gid
            << " : Check routing credentials failed as "
            << "configured routing manager credentials "
            << "do not match with routing manager credentials"
            << security_mode_text;

    return (!check_routing_credentials_);
}

bool
security_impl::parse_policy(const byte_t* &_buffer, uint32_t &_buffer_size,
        uint32_t &_uid, uint32_t &_gid, const std::shared_ptr<policy> &_policy) const {

    bool is_valid = _policy->deserialize(_buffer, _buffer_size);
    if (is_valid)
        is_valid = _policy->get_uid_gid(_uid, _gid);
    return is_valid;
}

///////////////////////////////////////////////////////////////////////////////
// Configuration
///////////////////////////////////////////////////////////////////////////////
void
security_impl::load_policies(const configuration_element &_element) {
#ifdef _WIN32
        return;
#endif
    try {
        auto optional = _element.tree_.get_child_optional("security");
        if (!optional) {
            return;
        }
        policy_enabled_ = true;
        auto found_policy = _element.tree_.get_child("security");
        for (auto its_security = found_policy.begin();
                its_security != found_policy.end(); ++its_security) {
            if (its_security->first == "check_credentials") {
                if (its_security->second.data() == "true") {
                    check_credentials_ = true;
                } else {
                    check_credentials_ = false;
                }
            } else if (its_security->first == "allow_remote_clients")  {
                if (its_security->second.data() == "true") {
                    allow_remote_clients_ = true;
                } else {
                    allow_remote_clients_ = false;
                }
            } else if (its_security->first == "policies") {
                for (auto its_policy = its_security->second.begin();
                        its_policy != its_security->second.end(); ++its_policy) {
                    load_policy(its_policy->second);
                }
            }
        }
    } catch (...) {
    }
}

void
security_impl::load_policy(const boost::property_tree::ptree &_tree) {

    std::shared_ptr<policy> policy(std::make_shared<policy>());
    bool allow_deny_set(false);
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        if (i->first == "credentials") {
            boost::icl::interval_set<uid_t> its_uid_interval_set;
            boost::icl::interval_set<gid_t> its_gid_interval_set;
            boost::icl::discrete_interval<uid_t> its_uid_interval;
            boost::icl::discrete_interval<gid_t> its_gid_interval;

            bool has_uid(false), has_gid(false);
            bool has_uid_range(false), has_gid_range(false);
            for (auto n = i->second.begin();
                    n != i->second.end(); ++n) {
                std::string its_key(n->first);
                std::string its_value(n->second.data());
                if (its_key == "uid") {
                    if(n->second.data().empty()) {
                        load_interval_set(n->second, its_uid_interval_set);
                        has_uid_range = true;
                    } else {
                        if (its_value != "any") {
                            uint32_t its_uid;
                            read_data(its_value, its_uid);
                            its_uid_interval = boost::icl::construct<
                                boost::icl::discrete_interval<uid_t> >(
                                        its_uid, its_uid,
                                        boost::icl::interval_bounds::closed());
                        } else {
                            its_uid_interval = boost::icl::construct<
                                boost::icl::discrete_interval<uid_t> >(
                                        std::numeric_limits<uid_t>::min(),
                                        std::numeric_limits<uid_t>::max(),
                                        boost::icl::interval_bounds::closed());
                        }
                        has_uid = true;
                    }
                } else if (its_key == "gid") {
                    if(n->second.data().empty()) {
                        load_interval_set(n->second, its_gid_interval_set);
                        has_gid_range = true;
                    } else {
                        if (its_value != "any") {
                            uint32_t its_gid;
                            read_data(its_value, its_gid);
                            its_gid_interval = boost::icl::construct<
                                boost::icl::discrete_interval<gid_t> >(
                                        its_gid, its_gid,
                                        boost::icl::interval_bounds::closed());
                        } else {
                            its_gid_interval = boost::icl::construct<
                                boost::icl::discrete_interval<gid_t> >(
                                        std::numeric_limits<gid_t>::min(),
                                        std::numeric_limits<gid_t>::max(),
                                        boost::icl::interval_bounds::closed());
                        }
                        has_gid = true;
                    }
                } else if (its_key == "allow" || its_key == "deny") {
                    policy->allow_who_ = (its_key == "allow");
                    load_credential(n->second, policy->credentials_);
                }
            }

            if (has_uid && has_gid) {
                its_gid_interval_set.insert(its_gid_interval);

                policy->credentials_ += std::make_pair(its_uid_interval, its_gid_interval_set);
                policy->allow_who_ = true;
            }
            if (has_uid_range && has_gid_range) {
                for (const auto& u : its_uid_interval_set)
                    policy->credentials_ += std::make_pair(u, its_gid_interval_set);
                policy->allow_who_ = true;
            }
        } else if (i->first == "allow") {
            if (allow_deny_set) {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: \"allow\" tag overrides "
                        << "already set \"deny\" tag. "
                        << "Either \"deny\" or \"allow\" is allowed.";
            }
            allow_deny_set = true;
            policy->allow_what_ = true;
            load_policy_body(policy, i);
        } else if (i->first == "deny") {
            if (allow_deny_set) {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: \"deny\" tag overrides "
                        << "already set \"allow\" tag. "
                        << "Either \"deny\" or \"allow\" is allowed.";
            }
            allow_deny_set = true;
            policy->allow_what_ = false;
            load_policy_body(policy, i);
        }
    }
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    any_client_policies_.push_back(policy);
}

void
security_impl::load_policy_body(std::shared_ptr<policy> &_policy,
        const boost::property_tree::ptree::const_iterator &_tree) {

    for (auto l = _tree->second.begin(); l != _tree->second.end(); ++l) {
        if (l->first == "requests") {
            for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                service_t its_service = 0x0;
                instance_t its_instance = 0x0;
                boost::icl::interval_map<instance_t,
                    boost::icl::interval_set<method_t> > its_instance_method_intervals;
                for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                    if (k->first == "service") {
                        read_data(k->second.data(), its_service);
                    } else if (k->first == "instance") { // legacy definition for instances
                        boost::icl::interval_set<instance_t> its_instance_interval_set;
                        boost::icl::interval_set<method_t> its_method_interval_set;
                        boost::icl::discrete_interval<instance_t> all_instances(0x01, 0xFFFF,
                                boost::icl::interval_bounds::closed());
                        boost::icl::discrete_interval<method_t> all_methods(0x01, 0xFFFF,
                                boost::icl::interval_bounds::closed());

                        std::string its_value(k->second.data());
                        if (its_value != "any") {
                            read_data(its_value, its_instance);
                            if (its_instance != 0x0) {
                                its_instance_interval_set.insert(its_instance);
                                its_method_interval_set.insert(all_methods);
                            }
                        } else {
                            its_instance_interval_set.insert(all_instances);
                            its_method_interval_set.insert(all_methods);
                        }
                        for (const auto& i : its_instance_interval_set) {
                            its_instance_method_intervals
                                += std::make_pair(i, its_method_interval_set);
                        }
                    } else if (k->first == "instances") { // new instances definition
                        for (auto p = k->second.begin(); p != k->second.end(); ++p) {
                            boost::icl::interval_set<instance_t> its_instance_interval_set;
                            boost::icl::interval_set<method_t> its_method_interval_set;
                            boost::icl::discrete_interval<method_t> all_methods(0x01, 0xFFFF,
                                    boost::icl::interval_bounds::closed());
                            for (auto m = p->second.begin(); m != p->second.end(); ++m) {
                                if (m->first == "ids") {
                                    load_interval_set(m->second, its_instance_interval_set);
                                } else if (m->first == "methods") {
                                    load_interval_set(m->second, its_method_interval_set);
                                }
                            }
                            if (its_method_interval_set.empty())
                                its_method_interval_set.insert(all_methods);
                            for (const auto& i : its_instance_interval_set) {
                                its_instance_method_intervals
                                    += std::make_pair(i, its_method_interval_set);
                            }
                        }

                        if (its_instance_method_intervals.empty()) {
                            boost::icl::interval_set<instance_t> its_legacy_instance_interval_set;
                            boost::icl::interval_set<method_t> its_legacy_method_interval_set;
                            boost::icl::discrete_interval<method_t> all_methods(0x01, 0xFFFF,
                                    boost::icl::interval_bounds::closed());
                            its_legacy_method_interval_set.insert(all_methods);

                            // try to only load instance ranges with any method to be allowed
                            load_interval_set(k->second, its_legacy_instance_interval_set);
                            for (const auto& i : its_legacy_instance_interval_set) {
                                its_instance_method_intervals
                                    += std::make_pair(i, its_legacy_method_interval_set);
                            }
                        }
                    }
                }
                if (its_service != 0x0 && !its_instance_method_intervals.empty()) {
                    _policy->requests_ += std::make_pair(
                            boost::icl::discrete_interval<service_t>(
                                    its_service, its_service,
                                    boost::icl::interval_bounds::closed()),
                            its_instance_method_intervals);
                }
            }
        } else if (l->first == "offers") {
            for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                service_t its_service(0x0);
                instance_t its_instance(0x0);
                boost::icl::interval_set<instance_t> its_instance_interval_set;
                for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                    if (k->first == "service") {
                        read_data(k->second.data(), its_service);
                    } else if (k->first == "instance") { // legacy definition for instances
                        std::string its_value(k->second.data());
                        if (its_value != "any") {
                            read_data(its_value, its_instance);
                            if (its_instance != 0x0) {
                                its_instance_interval_set.insert(its_instance);
                            }
                        } else {
                            its_instance_interval_set.insert(
                                    boost::icl::discrete_interval<instance_t>(
                                        0x0001, 0xFFFF));
                        }
                    } else if (k->first == "instances") { // new instances definition
                        load_interval_set(k->second, its_instance_interval_set);
                    }
                }
                if (its_service != 0x0 && !its_instance_interval_set.empty()) {
                    _policy->offers_
                        += std::make_pair(
                                boost::icl::discrete_interval<service_t>(
                                        its_service, its_service,
                                        boost::icl::interval_bounds::closed()),
                                its_instance_interval_set);
                }
            }
        }
    }
}


void
security_impl::load_credential(
        const boost::property_tree::ptree &_tree,
        boost::icl::interval_map<uid_t,
            boost::icl::interval_set<gid_t> > &_credentials) {

    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        boost::icl::interval_set<uid_t> its_uid_interval_set;
        boost::icl::interval_set<gid_t> its_gid_interval_set;

        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            std::string its_key(j->first);
            if (its_key == "uid") {
                load_interval_set(j->second, its_uid_interval_set);
            } else if (its_key == "gid") {
                load_interval_set(j->second, its_gid_interval_set);
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                        << "Malformed credential (contains illegal key \""
                        << its_key << "\")";
            }
        }

        for (const auto& its_uid_interval : its_uid_interval_set) {
            _credentials
                += std::make_pair(its_uid_interval, its_gid_interval_set);
        }
    }
}

bool
security_impl::load_routing_credentials(const configuration_element &_element) {
    try {
        auto its_routing_cred = _element.tree_.get_child("routing-credentials");
        if (is_configured_) {
            VSOMEIP_WARNING << "vSomeIP Security: Multiple definitions of routing-credentials."
                    << " Ignoring definition from " << _element.name_;
        } else {
            for (auto i = its_routing_cred.begin();
                    i != its_routing_cred.end();
                    ++i) {
                std::string its_key(i->first);
                std::string its_value(i->second.data());
                if (its_key == "uid") {
                    uint32_t its_uid(0);
                    read_data(its_value, its_uid);
                    std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
                    std::get<0>(routing_credentials_) = its_uid;
                } else if (its_key == "gid") {
                    uint32_t its_gid(0);
                    read_data(its_value, its_gid);
                    std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
                    std::get<1>(routing_credentials_) = its_gid;
                }
            }
            check_routing_credentials_ = true;
            is_configured_ = true;
        }
    } catch (...) {
        return false;
    }
    return true;
}


void
security_impl::load_security_update_whitelist(const configuration_element &_element) {
#ifdef _WIN32
        return;
#endif
    try {
        auto optional = _element.tree_.get_child_optional("security-update-whitelist");
        if (!optional) {
            return;
        }
        auto found_whitelist = _element.tree_.get_child("security-update-whitelist");
        for (auto its_whitelist = found_whitelist.begin();
                its_whitelist != found_whitelist.end(); ++its_whitelist) {

            if (its_whitelist->first == "uids") {
                {
                    std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
                    load_interval_set(its_whitelist->second, uid_whitelist_);
                }
            } else if (its_whitelist->first == "services") {
                {
                    std::lock_guard<std::mutex> its_lock(service_interface_whitelist_mutex_);
                    load_interval_set(its_whitelist->second, service_interface_whitelist_);
                }
            } else if (its_whitelist->first == "check-whitelist") {
                if (its_whitelist->second.data() == "true") {
                    check_whitelist_ = true;
                } else {
                    check_whitelist_ = false;
                }
            }
        }
    } catch (...) {
    }
}

template<typename T_>
void security_impl::load_interval_set(
        const boost::property_tree::ptree &_tree,
        boost::icl::interval_set<T_> &_intervals, bool _exclude_margins) {

    boost::icl::interval_set<T_> its_intervals;
    T_ its_min = std::numeric_limits<T_>::min();
    T_ its_max = std::numeric_limits<T_>::max();

    if (_exclude_margins) {
        its_min++;
        its_max--;
    }

    const std::string its_key(_tree.data());
    if (its_key == "any") {
        its_intervals.insert(boost::icl::discrete_interval<T_>::closed(
                its_min, its_max));
    } else {
        for (auto i = _tree.begin(); i != _tree.end(); ++i) {
            auto its_data = i->second;
            if (!its_data.data().empty()) {
                T_ its_id;
                read_data(its_data.data(), its_id);
                if (its_id >= its_min && its_id <= its_max)
                    its_intervals.insert(its_id);
            } else {
                T_ its_first, its_last;
                bool has_first(false), has_last(false);
                for (auto j = its_data.begin(); j != its_data.end(); ++j) {
                    std::string its_key(j->first);
                    std::string its_value(j->second.data());
                    if (its_key == "first") {
                        if (its_value == "min") {
                            its_first = its_min;
                        } else {
                            read_data(its_value, its_first);
                        }
                        has_first = true;
                    } else if (its_key == "last") {
                        if (its_value == "max") {
                            its_last = its_max;
                        } else {
                            read_data(its_value, its_last);
                        }
                        has_last = true;
                    } else {
                        VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                                << " Malformed range. Contains illegal key ("
                                << its_key << ")";
                    }
                }
                if (has_first && has_last && its_first <= its_last) {
                    its_intervals.insert(
                        boost::icl::discrete_interval<T_>::closed(its_first, its_last));
                }
            }
        }
    }

    _intervals = its_intervals;
}

void
security_impl::get_requester_policies(const std::shared_ptr<policy> _policy,
        std::set<std::shared_ptr<policy> > &_requesters) const {

    std::vector<std::shared_ptr<policy> > its_policies;
    {
        std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
        its_policies = any_client_policies_;
    }

    std::lock_guard<std::mutex> its_lock(_policy->mutex_);
    for (const auto& o : _policy->offers_) {
        for (const auto& p : its_policies) {
            if (p == _policy)
                continue;

            std::lock_guard<std::mutex> its_lock(p->mutex_);

            auto its_policy = std::make_shared<policy>();
            its_policy->credentials_ = p->credentials_;

            for (const auto& r : p->requests_) {
                // o represents an offer by a service interval and its instances
                // (a set of intervals)
                // r represents a request by a service interval and its instances
                // and methods (instance intervals mapped to interval sets of methods)
                //
                // Thus, r matches o if their service identifiers as well as their
                // instances overlap. If r and o match, a new policy must be
                // created that contains the overlapping services/instances mapping
                // of r and o together with the methods from r
                service_t its_o_lower, its_o_upper, its_r_lower, its_r_upper;
                get_bounds(o.first, its_o_lower, its_o_upper);
                get_bounds(r.first, its_r_lower, its_r_upper);

                if (its_o_lower <= its_r_upper && its_r_lower <= its_o_upper) {
                    auto its_service_min = std::max(its_o_lower, its_r_lower);
                    auto its_service_max = std::min(its_r_upper, its_o_upper);

                    for (const auto& i : o.second) {
                        for (const auto& j : r.second) {
                            for (const auto& k : j.second) {
                                instance_t its_i_lower, its_i_upper, its_k_lower, its_k_upper;
                                get_bounds(i, its_i_lower, its_i_upper);
                                get_bounds(k, its_k_lower, its_k_upper);

                                if (its_i_lower <= its_k_upper && its_k_lower <= its_i_upper) {
                                    auto its_instance_min = std::max(its_i_lower, its_k_lower);
                                    auto its_instance_max = std::min(its_i_upper, its_k_upper);

                                    boost::icl::interval_map<instance_t,
                                        boost::icl::interval_set<method_t> > its_instances_methods;
                                    its_instances_methods += std::make_pair(
                                            boost::icl::interval<instance_t>::closed(
                                                    its_instance_min, its_instance_max),
                                            j.second);

                                    its_policy->requests_ += std::make_pair(
                                            boost::icl::interval<instance_t>::closed(
                                                    its_service_min, its_service_max),
                                            its_instances_methods);
                                }
                            }
                        }
                    }
                }
            }

            if (!its_policy->requests_.empty()) {
                _requesters.insert(its_policy);
                its_policy->print();
            }
        }
    }
}

void
security_impl::get_clients(uid_t _uid, gid_t _gid,
        std::unordered_set<client_t> &_clients) const {

    std::lock_guard<std::mutex> its_lock(ids_mutex_);
    for (const auto& i : ids_) {
        if (i.second.first == _uid && i.second.second == _gid)
            _clients.insert(i.first);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Manage the security object
////////////////////////////////////////////////////////////////////////////////
static std::shared_ptr<security_impl> *the_security_ptr__(nullptr);
static std::mutex the_security_mutex__;

std::shared_ptr<security_impl>
security_impl::get() {
#ifndef _WIN32
    std::lock_guard<std::mutex> its_lock(the_security_mutex__);
#endif
    if(the_security_ptr__ == nullptr) {
        the_security_ptr__ = new std::shared_ptr<security_impl>();
    }
    if (the_security_ptr__ != nullptr) {
        if (!(*the_security_ptr__)) {
            *the_security_ptr__ = std::make_shared<security_impl>();
        }
        return (*the_security_ptr__);
    }
    return (nullptr);
}

#ifndef _WIN32
static void security_teardown(void) __attribute__((destructor));
static void security_teardown(void)
{
    if (the_security_ptr__ != nullptr) {
        std::lock_guard<std::mutex> its_lock(the_security_mutex__);
        the_security_ptr__->reset();
        delete the_security_ptr__;
        the_security_ptr__ = nullptr;
    }
}
#endif

} // namespace vsomeip_v3
