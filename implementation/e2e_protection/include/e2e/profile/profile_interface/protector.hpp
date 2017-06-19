// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef E2E_PROFILE_INTERFACE_PROTECTOR_HPP
#define E2E_PROFILE_INTERFACE_PROTECTOR_HPP

#include "../../../buffer/buffer.hpp"
#include "../profile_interface/profile_interface.hpp"

namespace e2e {
namespace profile {
namespace profile_interface {

class protector : public profile_interface {
  public:
    virtual void protect(buffer::e2e_buffer &_buffer) = 0;
};
}
}
}

#endif
