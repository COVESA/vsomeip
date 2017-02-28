// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/logger_impl.hpp"

namespace vsomeip {

std::shared_ptr<logger> logger::get() {
    return logger_impl::get();
}

} // namespace vsomeip
