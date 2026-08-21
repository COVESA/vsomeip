// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#ifdef USE_DLT
#include <dlt/dlt.h>
#else

// Dummy class to allow compilation without DLT, while avoiding lots of ifdefs in other code.
struct DltContext { };

#endif

namespace vsomeip_v3::trace {

// Get the immortal DltContext for the given context id and description. The context is
// registered with DLT on first use and, deliberately, never destroyed (but unregistered)
// during process lifetime, as libdlt might use it in a background thread for long messages.
// Returns nullptr if DLT is not available (USE_DLT not defined).
DltContext* get_log_context(const char* _id, const char* _desc);

} // namespace vsomeip_v3::trace
