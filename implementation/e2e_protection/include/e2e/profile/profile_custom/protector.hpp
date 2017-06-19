// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef E2E_PROFILE_PROFILE_CUSTOM_PROTECTOR_HPP
#define E2E_PROFILE_PROFILE_CUSTOM_PROTECTOR_HPP

#include <mutex>
#include "../profile_custom/profile_custom.hpp"
#include "../profile_interface/protector.hpp"

namespace e2e {
namespace profile {
namespace profile_custom {

class protector final : public e2e::profile::profile_interface::protector {
  public:
    protector(void) = delete;

    explicit protector(const Config &_config) : config(_config){};

    void protect(buffer::e2e_buffer &_buffer) override final;

  private:

    void write_crc(buffer::e2e_buffer &_buffer, uint32_t _computed_crc);

  private:
    Config config;
    std::mutex protect_mutex;
};
}
}
}
#endif
