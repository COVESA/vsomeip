// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_BUFFER_HPP
#define VSOMEIP_BUFFER_HPP

#include <array>
#include <memory>

#include <vsomeip/defines.hpp>
#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

typedef std::vector<byte_t> message_buffer_t;
typedef std::shared_ptr<message_buffer_t> message_buffer_ptr_t;

} // namespace vsomeip

#endif // VSOMEIP_BUFFER_HPP
