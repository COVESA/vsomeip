// Copyright (C) 2014 BMW Group
// Author: Lutz Bichler (lutz.bichler@bmw.de)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/runtime_impl.hpp"
#include "../../configuration/include/internal.hpp"

vsomeip::sd::runtime * VSOMEIP_SD_RUNTIME_SYMBOL;

static void init_vsomeip_sd() __attribute__((constructor));
static void init_vsomeip_sd() {
	VSOMEIP_SD_RUNTIME_SYMBOL = vsomeip::sd::runtime::get();
}

namespace vsomeip {
namespace sd {

runtime * runtime::get() {
	return runtime_impl::get();
}

} // namespace sd
} // namespace vsomeip



