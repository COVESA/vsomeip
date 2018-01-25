// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_E2E_PROFILE01_CHECKER_HPP
#define VSOMEIP_E2E_PROFILE01_CHECKER_HPP

#include "../profile01/profile_01.hpp"
#include "../profile_interface/checker.hpp"

namespace vsomeip {
namespace e2e {
namespace profile01 {

class profile_01_checker final : public e2e::profile_interface::checker {

  public:
    profile_01_checker(void) = delete;

    // [SWS_E2E_00389] initialize state
    explicit profile_01_checker(const profile_config &_config) :
            config_(_config) {}

    virtual void check(const e2e_buffer &_buffer,
                       e2e::profile_interface::generic_check_status &_generic_check_status) override final;

private:
    profile_config config_;
    std::mutex check_mutex_;

};

} // namespace profile01
} // namespace e2e
} // namespace vsomeip

#endif // VSOMEIP_E2E_PROFILE01_CHECKER_HPP
