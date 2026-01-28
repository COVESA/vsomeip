// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#ifdef VSOMEIP_INTERNAL_SUPPRESS_DEPRECATED
#define VSOMEIP_DEPRECATED_UID_GID
#else
#define VSOMEIP_DEPRECATED_UID_GID [[deprecated("Use vsomeip_sec_client_t-aware functions and types instead.")]]
#endif
