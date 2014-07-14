// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_BUFFER_HPP
#define VSOMEIP_BUFFER_HPP

#include <array>
#include <memory>

#include <vsomeip/primitive_types.hpp>

namespace vsomeip {

typedef std::vector< byte_t > buffer_t;
typedef std::shared_ptr< buffer_t > buffer_ptr_t;

} // namespace vsomeip

#endif // VSOMEIP_BUFFER_HPP
