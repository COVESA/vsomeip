// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <thread>
#include <mutex>
#include <vector>

#include "vsomeip/internal/logger.hpp"

namespace vsomeip_v3 {

/**
 * This singleton was created to ensure dispatcher threads (DT) that call stop are able to finish before teardown ends. As DTs
 * contains shared_ptrs to their applications, we cannot pass DTs' ownership to singletons that are owned by the app, as that
 * would lead to a circular dependency.
 *
 * As such, a new singleton was necessary to ensure that such DTs were properly joined after terminate() was called.
 */
class thread_manager {
public:
    thread_manager() = default;
    ~thread_manager();
    static thread_manager* get();

    /**
     * Save a dispatcher thread in thread_vector_ so it can be joined during singleton teardown.
     * @param _thread The thread to be saved.
     */
    void save_thread(std::shared_ptr<std::thread> _thread);

private:
    std::mutex thread_mutex_;
    std::vector<std::shared_ptr<std::thread>> thread_vector_;
};
} // namespace vsomeip_v3
