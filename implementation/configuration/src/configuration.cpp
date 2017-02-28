// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/configuration_impl.hpp"

#ifdef _WIN32
extern "C"
{
    __declspec(dllexport) std::shared_ptr<vsomeip::configuration> VSOMEIP_CFG_RUNTIME_SYMBOL;
}
#else
std::shared_ptr<vsomeip::configuration> VSOMEIP_CFG_RUNTIME_SYMBOL(vsomeip::configuration::get());
#endif

#ifdef _WIN32
#define CCALL __cdecl
#pragma section(".CRT$XCU",read)
#define INITIALIZER(f) \
    static void __cdecl f(void); \
    __declspec(allocate(".CRT$XCU")) void(__cdecl*f##_)(void) = f; \
    static void __cdecl f(void)

INITIALIZER(init_vsomeip_cfg) {
    VSOMEIP_CFG_RUNTIME_SYMBOL = vsomeip::configuration::get();
}
#endif

namespace vsomeip {

std::shared_ptr<configuration> configuration::get() {
    return cfg::configuration_impl::get();
}

} // namespace vsomeip
