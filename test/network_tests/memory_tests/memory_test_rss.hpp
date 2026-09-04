// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <thread>

#include <malloc.h>
#include <unistd.h>

namespace memory_test {

inline std::uint64_t read_rss_kib() {
    std::FILE* statm = std::fopen("/proc/self/statm", "r");
    if (statm == nullptr) {
        return 0;
    }

    unsigned long total_pages = 0;
    unsigned long resident_pages = 0;
    const int fields_read = std::fscanf(statm, "%lu %lu", &total_pages, &resident_pages);
    std::fclose(statm);
    if (fields_read != 2) {
        return 0;
    }

    const long page_size = getpagesize();
    if (page_size <= 0) {
        return 0;
    }
    return static_cast<std::uint64_t>(resident_pages) * static_cast<std::uint64_t>(page_size) / 1024;
}

inline std::uint64_t trim_and_read_rss_kib() {
    // RSS includes unused pages retained by the allocator. Return those pages
    // before sampling so that the measurement reflects live test data.
    malloc_trim(0);
    return read_rss_kib();
}

inline std::uint64_t wait_for_rss_at_most_kib(std::uint64_t baseline_kib, std::uint64_t growth_limit_kib, std::chrono::milliseconds timeout,
                                              std::chrono::milliseconds poll_interval = std::chrono::milliseconds(50)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::uint64_t minimum_kib = std::numeric_limits<std::uint64_t>::max();

    do {
        const auto rss_kib = trim_and_read_rss_kib();
        minimum_kib = std::min(minimum_kib, rss_kib);
        if (rss_kib != 0 && rss_kib <= baseline_kib + growth_limit_kib) {
            return rss_kib;
        }
        std::this_thread::sleep_for(poll_interval);
    } while (std::chrono::steady_clock::now() < deadline);

    return minimum_kib;
}

} // namespace memory_test
