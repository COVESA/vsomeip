// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cstdlib>
#include <filesystem>
#include <functional>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#ifndef _WIN32
#include "timeout_detector.hpp"
#endif

/**
 * @brief Template for test main
 *
 * This main contains ad-hoc setup that shall be done by all test executables. In short:
 * - setup temporary directory and set VSOMEIP_BASE_PATH
 * - setup timeout (by using `timeout_detector`)
 * - execute gtest
 *
 * The caller can do arbitrary setup before invoking this function, but beware that it occurs
 * before the setup of the temporary directory. It is recommended instead to use `setup` to
 * execute some code just before `gtest` is executed
 *
 * @param _argc argc
 * @param _argv argv
 * @param _setup function that does setup before test execution
 * @param _tearDown function that does cleaning after test execution
 * @return ret code
 */
inline int test_main(int _argc, char** _argv, std::function<void()> _setup = std::function<void()>(),
                     std::function<void()> _tearDown = std::function<void()>()) {

    // setup new directory
    // .. if the caller did not already
    std::string path;
    // NOTE: windows lacks mkdtemp, basename, alarm, ..
#ifndef _WIN32
    if (const char* e = getenv("VSOMEIP_BASE_PATH"); e == nullptr || e[0] == '\0') {
        const char* tmp_dir = getenv("TMPDIR");
        std::string tmp_template = tmp_dir && tmp_dir[0] != '\0' ? std::string(tmp_dir) + "/test.XXXXXX" : "/tmp/test.XXXXXX";

        std::vector<char> temp(tmp_template.begin(), tmp_template.end());
        temp.push_back('\0');

        char* dir = ::mkdtemp(temp.data());
        if (dir == nullptr) {
            return EXIT_FAILURE;
        }

        path = std::string(dir);

        if (_argc >= 1) {
            std::cout << ::basename(_argv[0]) << " is using temporary directory " << path << std::endl;
        }

        if (setenv("VSOMEIP_BASE_PATH", path.c_str(), 0)) {
            return EXIT_FAILURE;
        }
    }

    // setup timeout
    timeout_detector timeout;
#endif

    // setup gtest
    ::testing::InitGoogleTest(&_argc, _argv);

    // call arbitrary user-supplied setup
    if (_setup) {
        _setup();
    }

    // finally, execute gtest
    int ret = RUN_ALL_TESTS();

    if (_tearDown) {
        _tearDown();
    }

    // cleanup temporary directory
    if (!path.empty()) {
        std::error_code err;
        std::filesystem::remove_all(path, err);
    }

    return ret;
}
