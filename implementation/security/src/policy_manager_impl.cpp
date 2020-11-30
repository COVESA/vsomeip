// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/policy.hpp"
#include "../include/policy_manager_impl.hpp"
#include "../include/security_impl.hpp"

namespace vsomeip_v3 {

policy_manager_impl::~policy_manager_impl() {
}

std::shared_ptr<policy>
policy_manager_impl::create_policy() const {
    return std::make_shared<policy>();
}

void
policy_manager_impl::print_policy(const std::shared_ptr<policy> &_policy) const {

    if (_policy)
        _policy->print();
}

bool
policy_manager_impl::parse_uid_gid(const byte_t* &_buffer,
        uint32_t &_buffer_size, uint32_t &_uid, uint32_t &_gid) const {

   const auto its_policy = std::make_shared<policy>();
   return (its_policy
           && its_policy->deserialize_uid_gid(_buffer, _buffer_size, _uid, _gid));
}

bool
policy_manager_impl::is_policy_update_allowed(uint32_t _uid, std::shared_ptr<policy> &_policy) const {

    auto its_security = security_impl::get();
    return (its_security
            && its_security->is_policy_update_allowed(_uid, _policy));
}

bool
policy_manager_impl::is_policy_removal_allowed(uint32_t _uid) const {

    auto its_security = security_impl::get();
    return (its_security
            && its_security->is_policy_removal_allowed(_uid));
}

bool
policy_manager_impl::parse_policy(const byte_t* &_buffer, uint32_t &_buffer_size,
        uint32_t &_uid, uint32_t &_gid, const std::shared_ptr<policy> &_policy) const {

    auto its_security = security_impl::get();
    return (its_security
            && its_security->parse_policy(_buffer, _buffer_size, _uid, _gid, _policy));
}

} // namespace vsomeip_v3
