// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// NOTE: this is a *PUBLIC HEADER*! therefore any extensions to the logger that only make sense to the libvsomeip codebase must happen
// outside of it!
#include <vsomeip/internal/logger.hpp>

#include <thread>

#define VSOMEIP_LOG_WITH_PREFIX(level_macro) \
    level_macro << VSOMEIP_LOG_PREFIX << "::" << __func__ << ": "

#define VSOMEIP_FATAL_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_FATAL)
#define VSOMEIP_ERROR_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_ERROR)
#define VSOMEIP_WARNING_P VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_WARNING)
#define VSOMEIP_INFO_P    VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_INFO)
#define VSOMEIP_DEBUG_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_DEBUG)
#define VSOMEIP_TRACE_P   VSOMEIP_LOG_WITH_PREFIX(VSOMEIP_TRACE)

/**
 * @brief Flush DLTs and abort process
 *
 * Write also to stderr - guarantees that _something_ appears, even if the DLT
 * infrastructure is somewhat broken
 */
#define VSOMEIP_TERMINATE(reason)                             \
    do {                                                      \
        auto r = (reason);                                    \
        VSOMEIP_FATAL << "TERMINATING DUE TO '" << r << "'";  \
        fprintf(stderr, "TERMINATING DUE TO '%s'\n", r);        \
        fflush(stderr);                                       \
        ; /* no better way to flush DLTs than to wait */      \
        std::this_thread::sleep_for(std::chrono::seconds(2)); \
        VSOMEIP_FATAL << "TERMINATING";                       \
        fprintf(stderr, "TERMINATING\n");                       \
        fflush(stderr);                                       \
        std::abort();                                         \
    } while (0)
