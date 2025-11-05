// Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_V3_IS_VALUE_HPP_
#define VSOMEIP_V3_IS_VALUE_HPP_

namespace vsomeip_v3 {

template<typename T>
struct is_value {
    constexpr explicit is_value(T _t) : t_(_t) { }

    template<typename... Args>
    [[nodiscard]] bool any_of(Args... args) const&& {
        return ((t_ == args) || ...);
    }

    template<typename... Args>
    [[nodiscard]] bool none_of(Args... args) const&& {
        return ((t_ != args) && ...);
    }

    T t_;
};

}

#endif
