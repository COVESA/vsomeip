// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#if defined(__linux__)
#include <unistd.h>
#include <sys/syscall.h>
#endif

#include "../include/auxiliary_context.hpp"
#include "../../utility/include/utility.hpp"

#include <vsomeip/internal/logger.hpp>

vsomeip_v3::auxiliary_context::auxiliary_context(int thread_niceness) : thread_niceness_(thread_niceness) { }

vsomeip_v3::auxiliary_context::~auxiliary_context() {
    stop();
}

boost::asio::io_context& vsomeip_v3::auxiliary_context::get_context() {
    return context_;
}

void vsomeip_v3::auxiliary_context::start() {
    context_.restart();

    thread_ = std::thread([this]() mutable {
#if defined(__linux__) || defined(__QNX__)
        pthread_setname_np(pthread_self(), "m_auxiliary");
#endif
        utility::set_thread_niceness(thread_niceness_);
        VSOMEIP_INFO << "ac::auxiliary_context: Started thread m_auxiliary " << std::hex << std::this_thread::get_id()
#if defined(__linux__)
                     << ", tid " << std::dec << static_cast<int>(syscall(SYS_gettid))
#endif
                ;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(context_.get_executor());
        context_.run();

        VSOMEIP_INFO << "ac::auxiliary_context: Stopped thread m_auxiliary " << std::hex << std::this_thread::get_id()
#if defined(__linux__)
                     << ", tid " << std::dec << static_cast<int>(syscall(SYS_gettid))
#endif
                ;
    });
}

void vsomeip_v3::auxiliary_context::stop() {
    context_.stop();

    try {
        auto thread_id = thread_.get_id();
        if (std::this_thread::get_id() != thread_id && thread_.joinable()) {
            thread_.join();
            VSOMEIP_INFO << "ac::auxiliary_context: Joined thread m_auxiliary " << std::hex << thread_id;
        }
    } catch (...) {
        // Ignore exception if thread already joined
    }
}
