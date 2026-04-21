// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#include <csignal>
#if defined(__linux__) || defined(__QNX__)
#include <pthread.h>
#endif
#endif
#include <thread>
#include <vsomeip/vsomeip.hpp>
#include "hello_world_client.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#if defined(__linux__) || defined(__QNX__)
    sigset_t its_signals;
    sigemptyset(&its_signals);
    sigaddset(&its_signals, SIGINT);
    sigaddset(&its_signals, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &its_signals, nullptr);
#endif
#endif

    hello_world_client hw_cl;
    if (hw_cl.init()) {
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#if defined(__linux__) || defined(__QNX__)
        std::thread signal_watcher([&hw_cl]() {
            sigset_t its_wait_set;
            sigemptyset(&its_wait_set);
            sigaddset(&its_wait_set, SIGINT);
            sigaddset(&its_wait_set, SIGTERM);

            int its_signal = 0;
            while (sigwait(&its_wait_set, &its_signal) == 0) {
                if (its_signal == SIGINT || its_signal == SIGTERM) {
                    hw_cl.stop();
                    return;
                }
            }
        });
#endif
#endif
        hw_cl.start();
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#if defined(__linux__) || defined(__QNX__)
        if (signal_watcher.joinable()) {
            pthread_cancel(signal_watcher.native_handle());
            signal_watcher.join();
        }
#endif
#endif
        return 0;
    } else {
        return 1;
    }
}
