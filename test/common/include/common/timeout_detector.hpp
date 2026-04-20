// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdint>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <unistd.h>
#include <thread>

class timeout_detector {
public:
    timeout_detector(int _argc, char** _argv, uint32_t _timeout = 180) {
        command_ = "";
        build_log(_argc, _argv);
        signal(SIGALRM, alarmHandler);
        (void)!alarm(_timeout);
    }

private:
    inline static std::string command_{};

    static void alarmHandler(int _signal) {
        const char* str = command_.c_str();
        size_t len = command_.length();
        if (_signal == SIGALRM) {
            (void)!write(STDOUT_FILENO, str, len);
            std::abort();
        }
    }

    void build_log(int argc, char** argv) {
        std::string prefix = "timeout_detector blew up for '";
        std::string null_arg = "<null>";
        std::string suffix = "'! PROCESS " + std::to_string(getpid()) + " WILL SIGABRT\n";

        command_ = prefix;
        for (int i = 0; i < argc; i++) {
            if (!argv[i][0])
                command_ += null_arg;
            else
                command_ += argv[i];
            command_ += ' ';
        }
        command_ += suffix;
    }
};
