// Copyright (C) 2014-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../include/runtime_impl.hpp"
#include "../../configuration/include/internal.hpp"

#ifdef WIN32
extern "C"
{
    __declspec(dllexport) std::shared_ptr<vsomeip::sd::runtime> VSOMEIP_SD_RUNTIME_SYMBOL;
}
#else
std::shared_ptr<vsomeip::sd::runtime> VSOMEIP_SD_RUNTIME_SYMBOL;
#endif

#ifdef WIN32
#define CCALL __cdecl
#pragma section(".CRT$XCU",read)
#define INITIALIZER(f) \
    static void __cdecl f(void); \
    __declspec(allocate(".CRT$XCU")) void(__cdecl*f##_)(void) = f; \
    static void __cdecl f(void)
#else
#define CCALL
#define INITIALIZER(f) \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
#endif

INITIALIZER(init_vsomeip_sd) {
    VSOMEIP_SD_RUNTIME_SYMBOL = vsomeip::sd::runtime::get();
}

namespace vsomeip {
namespace sd {

std::shared_ptr<runtime> runtime::get() {
    return runtime_impl::get();
}

} // namespace sd
} // namespace vsomeip

