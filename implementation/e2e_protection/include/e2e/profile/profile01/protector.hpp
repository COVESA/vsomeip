// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef E2E_PROFILE_PROFILE01_PROTECTOR_HPP
#define E2E_PROFILE_PROFILE01_PROTECTOR_HPP

#include <mutex>
#include "../profile01/profile_01.hpp"
#include "../profile_interface/protector.hpp"

namespace e2e {
namespace profile {
namespace profile01 {

class protector final : public e2e::profile::profile_interface::protector {
  public:
    protector(void) = delete;

    explicit protector(const Config &_config) : config(_config){};

    void protect(buffer::e2e_buffer &_buffer) override final;

  private:

    void write_crc(buffer::e2e_buffer &_buffer, uint8_t _computed_crc);

  private:
    Config config;
    std::mutex protect_mutex;
};
}
}
}
#endif
