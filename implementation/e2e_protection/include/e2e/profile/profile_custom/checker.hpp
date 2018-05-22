// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_E2E_PROFILE_CUSTOM_CHECKER_HPP
#define VSOMEIP_E2E_PROFILE_CUSTOM_CHECKER_HPP

#include "../profile_custom/profile_custom.hpp"
#include "../profile_interface/checker.hpp"
#include <mutex>

namespace vsomeip {
namespace e2e {
namespace profile_custom {

class profile_custom_checker final : public vsomeip::e2e::profile_interface::checker {

  public:
    profile_custom_checker(void) = delete;

    explicit profile_custom_checker(const vsomeip::e2e::profile_custom::profile_config &_config) :
            config_(_config) {}

    virtual void check(const e2e_buffer &_buffer,
                       vsomeip::e2e::profile_interface::generic_check_status &_generic_check_status);

  private:
    uint32_t read_crc(const e2e_buffer &_buffer) const;

private:
    profile_config config_;
    std::mutex check_mutex_;

};

} // namespace profile_custom
} // namespace e2e
} // namespace vsomeip

#endif // VSOMEIP_E2E_PROFILE_CUSTOM_CHECKER_HPP
