// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <stdlib.h>
    #define bswap_16(x) _byteswap_ushort(x)
    #define bswap_32(x) _byteswap_ulong(x)
#else
    #include <byteswap.h>
#endif

#include "../include/security_impl.hpp"
#include "../../configuration/include/configuration_element.hpp"
#ifdef ANDROID
#include "../../configuration/include/internal_android.hpp"
#else
#include "../../configuration/include/internal.hpp"
#endif // ANDROID

namespace vsomeip_v3 {

static const std::uint8_t uid_width_ = sizeof(std::uint32_t);
static const std::uint8_t gid_width_ = sizeof(std::uint32_t);
static const std::uint8_t id_width_ = sizeof(std::uint16_t);
static const std::uint8_t range_width_ = sizeof(std::uint32_t);

static const std::uint8_t skip_union_length_ = sizeof(std::uint32_t);
static const std::uint8_t skip_union_type_ = sizeof(std::uint32_t);
static const std::uint8_t skip_union_length_type_ = sizeof(std::uint32_t) + sizeof(std::uint32_t);
static const std::uint8_t skip_struct_length_ = sizeof(std::uint32_t);
static const std::uint8_t skip_array_length_ = sizeof(std::uint32_t);

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
security_impl::check_credentials(client_t _client, uid_t _uid,
        gid_t _gid) {
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
        for (auto its_credential : p->ids_) {
            bool has_uid(false), has_gid(false);
            for (auto its_range : std::get<0>(its_credential)) {
                if (std::get<0>(its_range) <= _uid && _uid <= std::get<1>(its_range)) {
                    has_uid = true;
                    break;
                }
            }
            for (auto its_range : std::get<1>(its_credential)) {
                if (std::get<0>(its_range) <= _gid && _gid <= std::get<1>(its_range)) {
                    has_gid = true;
                    break;
                }
            }

            if (has_uid && has_gid) {
                has_id = true;
                break;
            }
        }

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
security_impl::is_client_allowed(uint32_t _uid, uint32_t _gid, client_t _client, service_t _service,
        instance_t _instance, method_t _method, bool _is_request_service) const {
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
        bool has_uid(false), has_gid(false), has_service(false), has_instance_id(false), has_method_id(false);
        for (auto its_credential : p->ids_) {
            has_uid = has_gid = false;
            for (auto its_range : std::get<0>(its_credential)) {
                if (std::get<0>(its_range) <= its_uid && its_uid <= std::get<1>(its_range)) {
                    has_uid = true;
                    break;
                }
            }
            for (auto its_range : std::get<1>(its_credential)) {
                if (std::get<0>(its_range) <= its_gid && its_gid <= std::get<1>(its_range)) {
                    has_gid = true;
                    break;
                }
            }

            if (has_uid && has_gid)
                break;
        }

        auto its_service = p->services_.find(_service);
        if (its_service != p->services_.end()) {
            for (auto its_ids : its_service->second) {
                has_service = has_instance_id = has_method_id = false;
                for (auto its_instance_range : std::get<0>(its_ids)) {
                    if (std::get<0>(its_instance_range) <= _instance && _instance <= std::get<1>(its_instance_range)) {
                        has_instance_id = true;
                        break;
                    }
                }
                if (!_is_request_service) {
                    for (auto its_method_range : std::get<1>(its_ids)) {
                        if (std::get<0>(its_method_range) <= _method && _method <= std::get<1>(its_method_range)) {
                            has_method_id = true;
                            break;
                        }
                    }
                } else {
                    // handle VSOMEIP_REQUEST_SERVICE
                    has_method_id = true;
                }

                if (has_instance_id && has_method_id) {
                    has_service = true;
                    break;
                }
            }
        }

        if ((has_uid && has_gid && p->allow_who_) || ((!has_uid || !has_gid) && !p->allow_who_)) {
            if (p->allow_what_) {
                // allow policy
                if (has_service) {
                    return true;
                }
            } else {
                // deny policy
                // allow client if the service / instance / !ANY_METHOD was not found
                if ((!has_service && (_method != ANY_METHOD))
                        // allow client if the service / instance / ANY_METHOD was not found
                        // and it is a "deny nothing" policy
                        || (!has_service && (_method == ANY_METHOD) && p->services_.empty())) {
                    return true;
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

    return !check_credentials_;
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
        bool has_uid(false), has_gid(false), has_offer(false);
        for (auto its_credential : p->ids_) {
            has_uid = has_gid = false;
            for (auto its_range : std::get<0>(its_credential)) {
                if (std::get<0>(its_range) <= its_uid && its_uid <= std::get<1>(its_range)) {
                    has_uid = true;
                    break;
                }
            }
            for (auto its_range : std::get<1>(its_credential)) {
                if (std::get<0>(its_range) <= its_gid && its_gid <= std::get<1>(its_range)) {
                    has_gid = true;
                    break;
                }
            }

            if (has_uid && has_gid)
                break;
        }

        auto find_service = p->offers_.find(_service);
        if (find_service != p->offers_.end()) {
            for (auto its_instance_range : find_service->second) {
                if (std::get<0>(its_instance_range) <= _instance
                        && _instance <= std::get<1>(its_instance_range)) {
                    has_offer = true;
                    break;
                }
            }
        }

        if ((has_uid && has_gid && p->allow_who_)
                || ((!has_uid || !has_gid) && !p->allow_who_)) {
            if (p->allow_what_ == has_offer) {
                return true;
            }
        }
    }

    std::string security_mode_text = " ~> Skip offer!";
    if (!check_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }

    VSOMEIP_INFO << "vSomeIP Security: Client 0x" << std::hex << _client
            << " with UID/GID=" << std::dec << its_uid << "/" << its_gid
            << " isn't allowed to offer service/instance " << std::hex
            << _service << "/" << _instance
            << security_mode_text;

    return !check_credentials_;
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
            std::lock_guard<std::mutex> its_policy_lock((*p_it)->mutex_);
            bool has_uid(false), has_gid(false);
            for (auto its_credential : p_it->get()->ids_) {
                has_uid = has_gid = false;
                for (auto its_range : std::get<0>(its_credential)) {
                    if (std::get<0>(its_range) <= _uid && _uid <= std::get<1>(its_range)) {
                        has_uid = true;
                        break;
                    }
                }
                for (auto its_range : std::get<1>(its_credential)) {
                    if (std::get<0>(its_range) <= _gid && _gid <= std::get<1>(its_range)) {
                        has_gid = true;
                        break;
                    }
                }
                // only remove "credentials allow" policies to prevent removal of
                // blacklist configured in file
                if (has_uid && has_gid && p_it->get()->allow_who_) {
                    was_removed = true;
                    break;
                }
            }
            if (was_removed) {
                p_it = any_client_policies_.erase(p_it);
                break;
            } else {
                ++p_it;
            }
        }
    }
    return was_removed;
}

void
security_impl::update_security_policy(uint32_t _uid, uint32_t _gid, const std::shared_ptr<policy> &_policy) {
    remove_security_policy(_uid, _gid);
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    any_client_policies_.push_back(_policy);
}

void
security_impl::add_security_credentials(uint32_t _uid, uint32_t _gid,
        const std::shared_ptr<policy> &_policy, client_t _client) {

    bool was_found(false);
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    for (const auto &p : any_client_policies_) {
        std::lock_guard<std::mutex> its_policy_lock(p->mutex_);
        bool has_uid(false), has_gid(false);
        for (auto its_credential : p->ids_) {
            has_uid = has_gid = false;
            for (auto its_range : std::get<0>(its_credential)) {
                if (std::get<0>(its_range) <= _uid && _uid <= std::get<1>(its_range)) {
                    has_uid = true;
                    break;
                }
            }
            for (auto its_range : std::get<1>(its_credential)) {
                if (std::get<0>(its_range) <= _gid && _gid <= std::get<1>(its_range)) {
                    has_gid = true;
                    break;
                }
            }
            if (has_uid && has_gid && p->allow_who_) {
                was_found = true;
                break;
            }
        }
        if (was_found) {
            break;
        }
    }
    // Do not add the new (credentials-only-policy) if a allow credentials policy with same credentials was found
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
security_impl::parse_uid_gid(const byte_t* &_buffer, uint32_t &_buffer_size,
        uint32_t &_uid, uint32_t &_gid) const {

    uint32_t its_uid = ANY_UID;
    uint32_t its_gid = ANY_GID;

    if (_buffer_size >= sizeof(uint32_t) * 2) {
        std::memcpy(&its_uid, _buffer, sizeof(uint32_t));
        _uid = bswap_32(its_uid);

        std::memcpy(&its_gid, _buffer + sizeof(uint32_t), sizeof(uint32_t));
        _gid = bswap_32(its_gid);

        _buffer_size -= (uid_width_ + gid_width_);
        _buffer += (uid_width_ + gid_width_);
        return true;
    }
    return false;
}

bool
security_impl::is_policy_update_allowed(uint32_t _uid, std::shared_ptr<policy> &_policy) const {
    bool uid_allowed(false);
    {
        std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
        for (auto its_uid_range : uid_whitelist_) {
            if (std::get<0>(its_uid_range) <= _uid && _uid <= std::get<1>(its_uid_range)) {
                uid_allowed = true;
                break;
            }
        }
    }

    if (uid_allowed) {
        std::lock_guard<std::mutex> its_lock(service_interface_whitelist_mutex_);
        std::lock_guard<std::mutex> its_policy_lock(_policy->mutex_);
        for (auto its_request : _policy->services_) {
            auto its_requested_service = std::get<0>(its_request);
            bool has_service(false);
            for (auto its_service_range : service_interface_whitelist_) {
                if (std::get<0>(its_service_range) <= its_requested_service
                        && its_requested_service <= std::get<1>(its_service_range)) {
                    has_service = true;
                    break;
                }
            }
            if (!has_service) {
                if (!check_whitelist_) {
                    VSOMEIP_INFO << "vSomeIP Security: Policy update requesting service ID: "
                            << std::hex << its_requested_service
                            << " is not allowed, but will be allowed due to whitelist audit mode is active!";
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Policy update requesting service ID: "
                            << std::hex << its_requested_service
                            << " is not allowed! -> ignore update";
                }
                return !check_whitelist_;
            }
        }
        return true;
    } else {
        if (!check_whitelist_) {
            VSOMEIP_INFO << "vSomeIP Security: Policy update for UID: " << std::dec << _uid
                    << " is not allowed, but will be allowed due to whitelist audit mode is active!";
        } else {
            VSOMEIP_WARNING << "vSomeIP Security: Policy update for UID: " << std::dec << _uid
                    << " is not allowed! -> ignore update";
        }
        return !check_whitelist_;
    }
}

bool
security_impl::is_policy_removal_allowed(uint32_t _uid) const {
    std::lock_guard<std::mutex> its_lock(uid_whitelist_mutex_);
    for (auto its_uid_range : uid_whitelist_) {
        if (std::get<0>(its_uid_range) <= _uid && _uid <= std::get<1>(its_uid_range)) {
            return true;
        }
    }

    if (!check_whitelist_) {
        VSOMEIP_INFO << "vSomeIP Security: Policy removal for UID: " << std::dec << _uid
                << " is not allowed, but will be allowed due to whitelist audit mode is active!";
    } else {
        VSOMEIP_WARNING << "vSomeIP Security: Policy removal for UID: " << std::dec << _uid
                << " is not allowed! -> ignore removal";
    }
    return !check_whitelist_;
}

bool
security_impl::check_routing_credentials(client_t _client, uint32_t _uid, uint32_t _gid) const {
    std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
    if ( std::get<0>(routing_credentials_) == _uid
            && std::get<1>(routing_credentials_) == _gid) {
        return true;
    }

    std::string security_mode_text = "!";
    if (!check_routing_credentials_) {
        security_mode_text = " but will be allowed due to audit mode is active!";
    }
    VSOMEIP_INFO << "vSomeIP Security: Client 0x"
            << std::hex << _client << " and UID/GID=" << std::dec << _uid
            << "/" << _gid << " : Check routing credentials failed as "
            << "configured routing manager credentials "
            << "do not match with routing manager credentials"
            << security_mode_text;

    return !check_routing_credentials_;
}

bool
security_impl::parse_policy(const byte_t* &_buffer, uint32_t &_buffer_size,
        uint32_t &_uid, uint32_t &_gid, const std::shared_ptr<policy> &_policy) const {

    uint32_t its_uid = ANY_UID;
    uint32_t its_gid = ANY_GID;
    bool has_error(false);

    // get user ID String
    if (parse_uid_gid(_buffer, _buffer_size, its_uid, its_gid)) {
        std::lock_guard<std::mutex> its_policy_lock(_policy->mutex_);

        _uid = its_uid;
        _gid = its_gid;

        // policy elements
        std::pair<uint32_t, uint32_t> its_uid_range, its_gid_range;
        std::set<std::pair<uint32_t, uint32_t>> its_uids, its_gids;

        // fill uid and gid range
        std::get<0>(its_uid_range) = its_uid;
        std::get<1>(its_uid_range) = its_uid;
        std::get<0>(its_gid_range) = its_gid;
        std::get<1>(its_gid_range) = its_gid;
        its_uids.insert(its_uid_range);
        its_gids.insert(its_gid_range);

        _policy->ids_.insert(std::make_pair(its_uids, its_gids));
        _policy->allow_who_ = true;
        _policy->allow_what_ = true;

        // get struct AclUpdate
        uint32_t acl_length = 0;
        if (get_struct_length(_buffer, _buffer_size, acl_length)) {
            // get requests array length
            uint32_t requests_array_length = 0;
            if (get_array_length(_buffer, _buffer_size, requests_array_length)) {
                // loop through requests array consisting of n x "struct Request"
                uint32_t parsed_req_bytes = 0;
                while (parsed_req_bytes + skip_struct_length_ <= requests_array_length) {
                    // get request struct length
                    uint32_t req_length = 0;
                    if (get_struct_length(_buffer, _buffer_size, req_length)) {
                        if (req_length != 0)
                            parsed_req_bytes += skip_struct_length_;

                        uint16_t its_service_id = 0;
                        ids_t its_instance_method_ranges;
                        // get serviceID
                        if (!parse_id(_buffer, _buffer_size, its_service_id)) {
                            has_error = true;
                        } else {
                            if (its_service_id == 0x00
                                    || its_service_id == 0xFFFF) {
                                VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy with service ID: 0x"
                                        << its_service_id << " is not allowed!";
                                return false;
                            }
                            // add length of serviceID
                            parsed_req_bytes += id_width_;
                        }

                        // get instances array length
                        uint32_t instances_array_length = 0;
                        if (get_array_length(_buffer, _buffer_size, instances_array_length)) {
                            // loop trough instances array consisting of n x "struct Instance"
                            uint32_t parsed_inst_bytes = 0;
                            while (parsed_inst_bytes + skip_struct_length_ <= instances_array_length) {
                                // get instance struct length
                                uint32_t inst_length = 0;
                                if (get_struct_length(_buffer, _buffer_size, inst_length)) {
                                    if (inst_length != 0)
                                        parsed_inst_bytes += skip_struct_length_;

                                    ranges_t its_instance_ranges;
                                    ranges_t its_method_ranges;
                                    // get "IdItem[] ids" array length
                                    uint32_t ids_array_length = 0;
                                    if (get_array_length(_buffer, _buffer_size, ids_array_length)) {
                                        uint32_t parsed_ids_bytes = 0;
                                        while (parsed_ids_bytes + skip_struct_length_ <= ids_array_length) {
                                            if (!parse_id_item(_buffer, parsed_ids_bytes, its_instance_ranges, _buffer_size)) {
                                                return false;
                                            }
                                        }
                                        parsed_inst_bytes += (skip_array_length_ + ids_array_length);
                                    }
                                    // get "IdItem[] methods" array length
                                    uint32_t methods_array_length = 0;
                                    if (get_array_length(_buffer, _buffer_size, methods_array_length)) {
                                        uint32_t parsed_method_bytes = 0;
                                        while (parsed_method_bytes + skip_struct_length_ <= methods_array_length) {
                                            if (!parse_id_item(_buffer, parsed_method_bytes, its_method_ranges, _buffer_size)) {
                                                return false;
                                            }
                                        }
                                        if (!its_instance_ranges.empty() && !its_method_ranges.empty()) {
                                            its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                                        }
                                        parsed_inst_bytes += (skip_array_length_ + methods_array_length);
                                    }
                                }
                            }
                            parsed_req_bytes += (skip_array_length_ + instances_array_length);
                        }
                        if (!its_instance_method_ranges.empty()) {
                            auto find_service = _policy->services_.find(its_service_id);
                            if (find_service != _policy->services_.end()) {
                                find_service->second.insert(its_instance_method_ranges.begin(),
                                        its_instance_method_ranges.end());
                            } else {
                                _policy->services_.insert(
                                    std::make_pair(its_service_id, its_instance_method_ranges));
                            }
                        }
                    }
                }
            }
            // get offers array length
            uint32_t offers_array_length = 0;
            if (get_array_length(_buffer, _buffer_size, offers_array_length)){
                // loop through offers array
                uint32_t parsed_offers_bytes = 0;
                while (parsed_offers_bytes + skip_struct_length_ <= offers_array_length) {
                    // get service ID
                    uint16_t its_service_id = 0;
                    ranges_t its_instance_ranges;
                    // get serviceID
                    if (!parse_id(_buffer, _buffer_size, its_service_id)) {
                        has_error = true;
                    } else {
                        if (its_service_id == 0x00
                                || its_service_id == 0xFFFF) {
                            VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy with service ID: 0x"
                                    << its_service_id << " is not allowed!";
                            return false;
                        }
                        // add length of serviceID
                        parsed_offers_bytes += id_width_;
                    }

                    // get "IdItem[] ids" array length
                    uint32_t ids_array_length = 0;
                    if (get_array_length(_buffer, _buffer_size, ids_array_length)) {
                        uint32_t parsed_ids_bytes = 0;
                        while (parsed_ids_bytes + skip_struct_length_ <= ids_array_length) {
                            if (!parse_id_item(_buffer, parsed_ids_bytes, its_instance_ranges, _buffer_size)) {
                                return false;
                            }
                        }
                        parsed_offers_bytes += (skip_array_length_ + ids_array_length);
                    }
                    if (!its_instance_ranges.empty()) {
                        auto find_service = _policy->offers_.find(its_service_id);
                        if (find_service != _policy->offers_.end()) {
                            find_service->second.insert(its_instance_ranges.begin(),
                                    its_instance_ranges.end());
                        } else {
                            _policy->offers_.insert(
                                    std::make_pair(its_service_id, its_instance_ranges));
                        }
                   }
                }
            }
        } else {
            VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy with empty request / offer section is not allowed!";
            has_error = true;
        }
    } else {
        VSOMEIP_WARNING << std::hex << "vSomeIP Security: Policy without UID / GID is not allowed!";
        has_error = true;
    }

    if (!has_error)
        return true;
    else
        return false;
}

bool
security_impl::get_struct_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length) const {
    uint32_t its_length = 0;
    bool length_field_deployed(false);
    // [TR_SOMEIP_00080] d If the length of the length field is not specified, a length of 0
    // has to be assumed and no length field is in the message.
    if (length_field_deployed) {
        if (_buffer_size >= sizeof(uint32_t)) {
            std::memcpy(&its_length, _buffer, sizeof(uint32_t));
            _length = bswap_32(its_length);
            _buffer_size -= skip_struct_length_;
            _buffer += skip_struct_length_;
            return true;
        }
    } else {
        _length = 0;
        return true;
    }

    return false;
}

bool
security_impl::get_union_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length) const {
    uint32_t its_length = 0;

    // [TR_SOMEIP_00125] d If the Interface Specification does not specify the length of the
    // length field for a union, 32 bit length of the length field shall be used.
    if (_buffer_size >= sizeof(uint32_t)) {
        std::memcpy(&its_length, _buffer, sizeof(uint32_t));
        _length = bswap_32(its_length);
        _buffer_size -= skip_union_length_;
        _buffer += skip_union_length_;
        return true;
    }
    return false;
}

bool
security_impl::get_array_length(const byte_t* &_buffer, uint32_t &_buffer_size, uint32_t &_length) const {

    uint32_t its_length = 0;

    // [TR_SOMEIP_00106] d The layout of arrays with dynamic length basically is based on
    // the layout of fixed length arrays. To determine the size of the array the serialization
    // adds a length field (default length 32 bit) in front of the data, which counts the bytes
    // of the array. The length does not include the size of the length field. Thus, when
    // transporting an array with zero elements the length is set to zero.
    if (_buffer_size >= sizeof(uint32_t)) {
        std::memcpy(&its_length, _buffer, sizeof(uint32_t));
        _length = bswap_32(its_length);
        _buffer_size -= skip_array_length_;
        _buffer += skip_array_length_;
        return true;
    }
    return false;
}

bool
security_impl::is_range(const byte_t* &_buffer, uint32_t &_buffer_size) const {

    uint32_t its_type = 0;

    // [TR_SOMEIP_00128] If the Interface Specification does not specify the length of the
    // type field of a union, 32 bit length of the type field shall be used.
    if (_buffer_size >= sizeof(uint32_t)) {
        std::memcpy(&its_type, _buffer, sizeof(uint32_t));
        its_type = bswap_32(its_type);
        _buffer_size -= skip_union_type_;
        _buffer += skip_union_type_;
        if (its_type == 0x02) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

bool
security_impl::parse_id_item(const byte_t* &_buffer, uint32_t& parsed_ids_bytes,
        ranges_t& its_ranges, uint32_t &_buffer_size) const {

    // get "union IdItem" length
    uint32_t iditem_length = 0;
    if (get_union_length(_buffer, _buffer_size, iditem_length)) {
        // determine type of union
        uint16_t its_first = 0;
        uint16_t its_last = 0;
        if (is_range(_buffer, _buffer_size)) {
            // get range of instance IDs "struct IdRange" length
            uint32_t range_length = 0;
            if (get_struct_length(_buffer, _buffer_size, range_length)) {
                // read first and last instance range
                if (parse_range(_buffer, _buffer_size, its_first, its_last)) {
                    its_ranges.insert(std::make_pair(its_first, its_last));
                } else {
                    return false;
                }
            }
        } else {
            // a single instance ID
            if (parse_id(_buffer, _buffer_size, its_first)) {
                if (its_first != ANY_METHOD) {
                    if (its_first != 0x00) {
                        its_last = its_first;
                        its_ranges.insert(std::make_pair(its_first, its_last));
                    } else {
                        return false;
                    }
                } else {
                    its_first = 0x01;
                    its_last = 0xFFFE;
                    its_ranges.insert(std::make_pair(its_first, its_last));
                }
            }
        }
        parsed_ids_bytes += (skip_union_length_type_ + iditem_length);
    }
    return true;
}

bool
security_impl::parse_range(const byte_t* &_buffer, uint32_t &_buffer_size,
        uint16_t &_first, uint16_t &_last) const {

    uint16_t its_first = 0;
    uint16_t its_last = 0;

    if (_buffer_size >= sizeof(uint16_t) * 2) {
        if (parse_id(_buffer, _buffer_size, its_first)) {
            _first = its_first;
        }
        if (parse_id(_buffer, _buffer_size, its_last)) {
            _last = its_last;
        }
        if (_first != _last
                && (_first == ANY_METHOD || _last == ANY_METHOD)) {
            return false;
        }
        if (_first != 0x0 && _last != 0x00
                && _first <= _last) {
            if (_first == ANY_METHOD &&
                    _last == ANY_METHOD) {
                _first = 0x01;
                _last = 0xFFFE;
            }
            return true;
        } else {
            return false;
        }
    }
    return false;
}

bool
security_impl::parse_id(const byte_t* &_buffer, uint32_t &_buffer_size, uint16_t &_id) const {
    uint16_t its_id = 0;
    if (_buffer_size >= sizeof(uint16_t)) {
        std::memcpy(&its_id, _buffer, sizeof(uint16_t));
        _id = bswap_16(its_id);
        _buffer_size -= id_width_;
        _buffer += id_width_;
        return true;
    }
    return false;
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
            std::pair<uint32_t, uint32_t> its_uid_range, its_gid_range;
            ranges_t its_uid_ranges, its_gid_ranges;

            bool has_uid(false), has_gid(false);
            bool has_uid_range(false), has_gid_range(false);
            for (auto n = i->second.begin();
                    n != i->second.end(); ++n) {
                std::string its_key(n->first);
                std::string its_value(n->second.data());
                if (its_key == "uid") {
                    if(n->second.data().empty()) {
                        load_ranges(n->second, its_uid_ranges);
                        has_uid_range = true;
                    } else {
                        if (its_value != "any") {
                            uint32_t its_uid;
                            std::stringstream its_converter;
                            its_converter << std::dec << its_value;
                            its_converter >> its_uid;
                            std::get<0>(its_uid_range) = its_uid;
                            std::get<1>(its_uid_range) = its_uid;
                        } else {
                            std::get<0>(its_uid_range) = 0;
                            std::get<1>(its_uid_range) = 0xFFFFFFFF;
                        }
                        has_uid = true;
                    }
                } else if (its_key == "gid") {
                    if(n->second.data().empty()) {
                        load_ranges(n->second, its_gid_ranges);
                        has_gid_range = true;
                    } else {
                        if (its_value != "any") {
                            uint32_t its_gid;
                            std::stringstream its_converter;
                            its_converter << std::dec << its_value;
                            its_converter >> its_gid;
                            std::get<0>(its_gid_range) = its_gid;
                            std::get<1>(its_gid_range) = its_gid;
                        } else {
                            std::get<0>(its_gid_range) = 0;
                            std::get<1>(its_gid_range) = 0xFFFFFFFF;
                        }
                        has_gid = true;
                    }
                } else if (its_key == "allow" || its_key == "deny") {
                    policy->allow_who_ = (its_key == "allow");
                    load_credential(n->second, policy->ids_);
                }
            }

            if (has_uid && has_gid) {
                std::set<std::pair<uint32_t, uint32_t>> its_uids, its_gids;

                its_uids.insert(its_uid_range);
                its_gids.insert(its_gid_range);

                policy->allow_who_ = true;
                policy->ids_.insert(std::make_pair(its_uids, its_gids));
            }
            if (has_uid_range && has_gid_range) {
                policy->allow_who_ = true;
                policy->ids_.insert(std::make_pair(its_uid_ranges, its_gid_ranges));
            }
        } else if (i->first == "allow") {
            if (allow_deny_set) {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: \"allow\" tag overrides "
                        << "already set \"deny\" tag. "
                        << "Either \"deny\" or \"allow\" is allowed.";
            }
            allow_deny_set = true;
            policy->allow_what_ = true;
            for (auto l = i->second.begin(); l != i->second.end(); ++l) {
                if (l->first == "requests") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ids_t its_instance_method_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                ranges_t its_instance_ranges;
                                ranges_t its_method_ranges;
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                        its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                                its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                            } else if (k->first == "instances") { // new instances definition
                                for (auto p = k->second.begin(); p != k->second.end(); ++p) {
                                    ranges_t its_instance_ranges;
                                    ranges_t its_method_ranges;
                                    for (auto m = p->second.begin(); m != p->second.end(); ++m) {
                                        if (m->first == "ids") {
                                            load_instance_ranges(m->second, its_instance_ranges);
                                        } else if (m->first == "methods") {
                                            load_instance_ranges(m->second, its_method_ranges);
                                        }
                                        if (!its_instance_ranges.empty() && !its_method_ranges.empty()) {
                                            its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                                        }
                                    }
                                }
                                if (its_instance_method_ranges.empty()) {
                                    ranges_t its_legacy_instance_ranges;
                                    ranges_t its_legacy_method_ranges;
                                    its_legacy_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    // try to only load instance ranges with any method to be allowed
                                    load_instance_ranges(k->second, its_legacy_instance_ranges);
                                    if (!its_legacy_instance_ranges.empty() && !its_legacy_method_ranges.empty()) {
                                        its_instance_method_ranges.insert(std::make_pair(its_legacy_instance_ranges,
                                            its_legacy_method_ranges));
                                    }
                                }
                            }
                        }
                        if (service != 0x0 && !its_instance_method_ranges.empty()) {
                            auto find_policy = policy->services_.find(service);
                            if (find_policy != policy->services_.end()) {
                                find_policy->second.insert(its_instance_method_ranges.begin(),
                                        its_instance_method_ranges.end());
                            } else {
                                policy->services_.insert(
                                    std::make_pair(service, its_instance_method_ranges));
                            }
                        }
                    }
                } else if (l->first == "offers") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ranges_t its_instance_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                            } else if (k->first == "instances") { // new instances definition
                                load_instance_ranges(k->second, its_instance_ranges);
                            }
                        }
                        if (service != 0x0 && !its_instance_ranges.empty()) {
                            auto find_service = policy->offers_.find(service);
                            if (find_service != policy->offers_.end()) {
                                find_service->second.insert(its_instance_ranges.begin(),
                                        its_instance_ranges.end());
                            } else {
                                policy->offers_.insert(
                                        std::make_pair(service, its_instance_ranges));
                            }
                        }
                    }
                }
            }
        } else if (i->first == "deny") {
            if (allow_deny_set) {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: \"deny\" tag overrides "
                        << "already set \"allow\" tag. "
                        << "Either \"deny\" or \"allow\" is allowed.";
            }
            allow_deny_set = true;
            policy->allow_what_ = false;
            for (auto l = i->second.begin(); l != i->second.end(); ++l) {
                if (l->first == "requests") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ids_t its_instance_method_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                ranges_t its_instance_ranges;
                                ranges_t its_method_ranges;
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                        its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    its_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                                its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                            } else if (k->first == "instances") { // new instances definition
                                for (auto p = k->second.begin(); p != k->second.end(); ++p) {
                                    ranges_t its_instance_ranges;
                                    ranges_t its_method_ranges;
                                    for (auto m = p->second.begin(); m != p->second.end(); ++m) {
                                        if (m->first == "ids") {
                                            load_instance_ranges(m->second, its_instance_ranges);
                                        } else if (m->first == "methods") {
                                            load_instance_ranges(m->second, its_method_ranges);
                                        }
                                        if (!its_instance_ranges.empty() && !its_method_ranges.empty()) {
                                            its_instance_method_ranges.insert(std::make_pair(its_instance_ranges, its_method_ranges));
                                        }
                                    }
                                }
                                if (its_instance_method_ranges.empty()) {
                                    ranges_t its_legacy_instance_ranges;
                                    ranges_t its_legacy_method_ranges;
                                    its_legacy_method_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                    // try to only load instance ranges with any method to be allowed
                                    load_instance_ranges(k->second, its_legacy_instance_ranges);
                                    if (!its_legacy_instance_ranges.empty() && !its_legacy_method_ranges.empty()) {
                                        its_instance_method_ranges.insert(std::make_pair(its_legacy_instance_ranges,
                                            its_legacy_method_ranges));
                                    }
                                }
                            }
                        }
                        if (service != 0x0 && !its_instance_method_ranges.empty()) {
                            auto find_policy = policy->services_.find(service);
                            if (find_policy != policy->services_.end()) {
                                find_policy->second.insert(its_instance_method_ranges.begin(),
                                        its_instance_method_ranges.end());
                            } else {
                                policy->services_.insert(
                                    std::make_pair(service, its_instance_method_ranges));
                            }
                        }
                    }
                }
                if (l->first == "offers") {
                    for (auto n = l->second.begin(); n != l->second.end(); ++n) {
                        service_t service = 0x0;
                        instance_t instance = 0x0;
                        ranges_t its_instance_ranges;
                        for (auto k = n->second.begin(); k != n->second.end(); ++k) {
                            std::stringstream its_converter;
                            if (k->first == "service") {
                                std::string value = k->second.data();
                                its_converter << std::hex << value;
                                its_converter >> service;
                            } else if (k->first == "instance") { // legacy definition for instances
                                std::string value = k->second.data();
                                if (value != "any") {
                                    its_converter << std::hex << value;
                                    its_converter >> instance;
                                    if (instance != 0x0) {
                                        its_instance_ranges.insert(std::make_pair(instance, instance));
                                    }
                                } else {
                                    its_instance_ranges.insert(std::make_pair(0x01, 0xFFFF));
                                }
                            } else if (k->first == "instances") { // new instances definition
                                load_instance_ranges(k->second, its_instance_ranges);
                            }
                        }
                        if (service != 0x0 && !its_instance_ranges.empty()) {
                            auto find_service = policy->offers_.find(service);
                            if (find_service != policy->offers_.end()) {
                                find_service->second.insert(its_instance_ranges.begin(),
                                        its_instance_ranges.end());
                            } else {
                                policy->offers_.insert(
                                        std::make_pair(service, its_instance_ranges));
                            }
                        }
                    }
                }
            }
        }
    }
    std::lock_guard<std::mutex> its_lock(any_client_policies_mutex_);
    any_client_policies_.push_back(policy);
}

void
security_impl::load_credential(
        const boost::property_tree::ptree &_tree, ids_t &_ids) {
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        ranges_t its_uid_ranges, its_gid_ranges;
        for (auto j = i->second.begin(); j != i->second.end(); ++j) {
            std::string its_key(j->first);
            if (its_key == "uid") {
                load_ranges(j->second, its_uid_ranges);
            } else if (its_key == "gid") {
                load_ranges(j->second, its_gid_ranges);
            } else {
                VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                        << "Malformed credential (contains illegal key \""
                        << its_key << "\"";
            }
        }

        _ids.insert(std::make_pair(its_uid_ranges, its_gid_ranges));
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
                std::stringstream its_converter;
                if (its_key == "uid") {
                    uint32_t its_uid(0);
                    if (its_value.find("0x") == 0) {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_uid;
                    std::lock_guard<std::mutex> its_lock(routing_credentials_mutex_);
                    std::get<0>(routing_credentials_) = its_uid;
                } else if (its_key == "gid") {
                    uint32_t its_gid(0);
                    if (its_value.find("0x") == 0) {
                        its_converter << std::hex << its_value;
                    } else {
                        its_converter << std::dec << its_value;
                    }
                    its_converter >> its_gid;
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
                    load_ranges(its_whitelist->second, uid_whitelist_);
                }
            } else if (its_whitelist->first == "services") {
                {
                    std::lock_guard<std::mutex> its_lock(service_interface_whitelist_mutex_);
                    load_service_ranges(its_whitelist->second, service_interface_whitelist_);
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

void
security_impl::load_ranges(
        const boost::property_tree::ptree &_tree, ranges_t &_range) {
    ranges_t its_ranges;
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        auto its_data = i->second;
        if (!its_data.data().empty()) {
            uint32_t its_id;
            std::stringstream its_converter;
            its_converter << std::dec << its_data.data();
            its_converter >> its_id;
            its_ranges.insert(std::make_pair(its_id, its_id));
        } else {
            uint32_t its_first, its_last;
            bool has_first(false), has_last(false);
            for (auto j = its_data.begin(); j != its_data.end(); ++j) {
                std::string its_key(j->first);
                std::string its_value(j->second.data());
                if (its_key == "first") {
                    if (its_value == "max") {
                        its_first = 0xFFFFFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::dec << j->second.data();
                        its_converter >> its_first;
                    }
                    has_first = true;
                } else if (its_key == "last") {
                    if (its_value == "max") {
                        its_last = 0xFFFFFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::dec << j->second.data();
                        its_converter >> its_last;
                    }
                    has_last = true;
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                            << " Malformed range. Contains illegal key ("
                            << its_key << ")";
                }
            }

            if (has_first && has_last) {
                its_ranges.insert(std::make_pair(its_first, its_last));
            }
        }
    }

    _range = its_ranges;
}

void
security_impl::load_instance_ranges(
        const boost::property_tree::ptree &_tree, ranges_t &_range) {
    ranges_t its_ranges;
    const std::string& key(_tree.data());
    if (key == "any") {
        its_ranges.insert(std::make_pair(0x01, 0xFFFF));
        _range = its_ranges;
        return;
    }
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        auto its_data = i->second;
        if (!its_data.data().empty()) {
            uint32_t its_id = 0x0;
            std::stringstream its_converter;
            its_converter << std::hex << its_data.data();
            its_converter >> its_id;
            if (its_id != 0x0) {
                its_ranges.insert(std::make_pair(its_id, its_id));
            }
        } else {
            uint32_t its_first, its_last;
            bool has_first(false), has_last(false);
            for (auto j = its_data.begin(); j != its_data.end(); ++j) {
                std::string its_key(j->first);
                std::string its_value(j->second.data());
                if (its_key == "first") {
                    if (its_value == "max") {
                        its_first = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_first;
                    }
                    has_first = true;
                } else if (its_key == "last") {
                    if (its_value == "max") {
                        its_last = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_last;
                    }
                    has_last = true;
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Security configuration: "
                            << " Malformed range. Contains illegal key ("
                            << its_key << ")";
                }
            }

            if (has_first && has_last) {
                if( its_last > its_first) {
                    its_ranges.insert(std::make_pair(its_first, its_last));
                }
            }
        }
    }

    _range = its_ranges;
}

void
security_impl::load_service_ranges(
        const boost::property_tree::ptree &_tree, std::set<std::pair<service_t, service_t>> &_ranges) {
    std::set<std::pair<service_t, service_t>> its_ranges;
    const std::string& key(_tree.data());
    if (key == "any") {
        its_ranges.insert(std::make_pair(0x01, 0xFFFF));
        _ranges = its_ranges;
        return;
    }
    for (auto i = _tree.begin(); i != _tree.end(); ++i) {
        auto its_data = i->second;
        if (!its_data.data().empty()) {
            service_t its_id = 0x0;
            std::stringstream its_converter;
            its_converter << std::hex << its_data.data();
            its_converter >> its_id;
            if (its_id != 0x0) {
                its_ranges.insert(std::make_pair(its_id, its_id));
            }
        } else {
            service_t its_first, its_last;
            bool has_first(false), has_last(false);
            for (auto j = its_data.begin(); j != its_data.end(); ++j) {
                std::string its_key(j->first);
                std::string its_value(j->second.data());
                if (its_key == "first") {
                    if (its_value == "max") {
                        its_first = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_first;
                    }
                    has_first = true;
                } else if (its_key == "last") {
                    if (its_value == "max") {
                        its_last = 0xFFFF;
                    } else {
                        std::stringstream its_converter;
                        its_converter << std::hex << j->second.data();
                        its_converter >> its_last;
                    }
                    has_last = true;
                } else {
                    VSOMEIP_WARNING << "vSomeIP Security: Security interface whitelist configuration: "
                            << " Malformed range. Contains illegal key ("
                            << its_key << ")";
                }
            }

            if (has_first && has_last) {
                if( its_last >= its_first) {
                    its_ranges.insert(std::make_pair(its_first, its_last));
                }
            }
        }
    }

    _ranges = its_ranges;
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
