// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.


#ifndef E2E_PROFILE_INTERFACE_CHECKER_HPP
#define E2E_PROFILE_INTERFACE_CHECKER_HPP

#include "../profile_interface/profile_interface.hpp"
#include "../../../buffer/buffer.hpp"
#include <mutex>


namespace e2e {
namespace profile {
namespace profile_interface {

class checker : public profile_interface {
  public:
    virtual void check(const buffer::e2e_buffer &_buffer,
                       e2e::profile::profile_interface::generic_check_status &_generic_check_status) = 0;
};
}
}
}

#endif
