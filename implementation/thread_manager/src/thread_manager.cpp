// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <atomic>

#include "../include/thread_manager.hpp"
namespace vsomeip_v3 {

thread_manager::~thread_manager() {
    std::scoped_lock its_lock{thread_mutex_};
    for (auto& thread : thread_vector_) {
        try {
            if (thread->get_id() == std::this_thread::get_id()) {
                fprintf(stderr, "Unable to join dispatcher thread: dispatcher thread called exit after stopping application\n");
                fprintf(stderr, "Detaching dispatcher thread (WILL CAUSE LEAKS!)\n");
                thread->detach();
                continue;
            }

            thread->join();
        } catch (const std::system_error& e) {
            fprintf(stderr, "error: While trying to join a detached thread: %s\n", e.what());
        }
    }
}

thread_manager* thread_manager::get() {
    // Use flag set by a custom deleter as a safety check in case something tries to save a thread during
    // static deinitialization time, after thread_manager is gone already.
    static std::atomic_bool is_destroyed{false};
    static auto deleter = [](thread_manager* ptr) {
        is_destroyed.store(true);
        delete ptr;
    };
    static std::unique_ptr<thread_manager, decltype(deleter)> instance{new thread_manager, deleter};
    return is_destroyed.load() ? nullptr : instance.get();
}

void thread_manager::save_thread(std::shared_ptr<std::thread> _thread) {
    // Keep all dispatcher threads instead of handing over a single slot. Otherwise, if app_0 stores DT0 and then
    // waits for app_1 to finish shutdown, app_1 can deadlock while trying to replace DT0 with DT1 under the same
    // mutex. Preserving both entries lets interdependent applications complete shutdown without blocking the handover.
    std::scoped_lock its_lock{this->thread_mutex_};
    thread_vector_.push_back(_thread);
}
} // namespace vsomeip_v3
