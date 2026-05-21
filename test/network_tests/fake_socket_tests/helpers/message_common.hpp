// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <type_traits>
#include <vector>

namespace vsomeip_v3::testing {

/** Default type, not a vector. */
template<typename Type>
struct is_std_vector : std::false_type { };

/** vector type.*/
template<typename Type, typename Alloc>
struct is_std_vector<std::vector<Type, Alloc>> : std::true_type { };

}
