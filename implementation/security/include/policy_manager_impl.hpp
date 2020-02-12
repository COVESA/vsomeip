// Copyright (C) 2019 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_POLICY_MANAGER_IMPL_HPP_
#define VSOMEIP_V3_POLICY_MANAGER_IMPL_HPP_

#include <memory>
#include <mutex>

#include <vsomeip/primitive_types.hpp>
#include <vsomeip/internal/policy_manager.hpp>

#include "../include/policy.hpp"
#include "../../configuration/include/configuration_element.hpp"

namespace vsomeip_v3 {

class policy_manager_impl
    : public policy_manager {
public:
    static std::shared_ptr<policy_manager> get();

    virtual ~policy_manager_impl();

    std::shared_ptr<policy> create_policy() const;
    void print_policy(const std::shared_ptr<policy> &_policy) const;

    bool parse_uid_gid(const byte_t* &_buffer, uint32_t &_buffer_size,
            uint32_t &_uid, uint32_t &_gid) const;
    bool parse_policy(const byte_t* &_buffer, uint32_t &_buffer_size,
            uint32_t &_uid, uint32_t &_gid,
            const std::shared_ptr<policy> &_policy) const;

    bool is_policy_update_allowed(uint32_t _uid,
            std::shared_ptr<policy> &_policy) const;
    bool is_policy_removal_allowed(uint32_t _uid) const;
};

} // namespace vsomeip_v3

#endif // VSOMEIP_V3_POLICY_MANAGER_IMPL_HPP_
