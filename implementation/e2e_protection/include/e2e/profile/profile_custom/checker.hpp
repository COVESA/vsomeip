// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef E2E_PROFILE_PROFILE_CUSTOM_CHECKER_HPP
#define E2E_PROFILE_PROFILE_CUSTOM_CHECKER_HPP

#include "../profile_custom/profile_custom.hpp"
#include "../profile_interface/checker.hpp"
#include <mutex>

namespace e2e {
namespace profile {
namespace profile_custom {

class profile_custom_checker final : public e2e::profile::profile_interface::checker {

  public:
    profile_custom_checker(void) = delete;

    explicit profile_custom_checker(const Config &_config) :
            config(_config) {}

    virtual void check(const buffer::e2e_buffer &_buffer,
                       e2e::profile::profile_interface::generic_check_status &_generic_check_status);

  private:
    uint32_t read_crc(const buffer::e2e_buffer &_buffer) const;

private:
    Config config;
    std::mutex check_mutex;

};
}
}
}

#endif
