// Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>

#include <vsomeip/vsomeip.hpp>
#include "../implementation/configuration/include/internal.hpp"

/*
 * Create a vsomeip application object and start it.
 */
int process(void) {
    std::shared_ptr<vsomeip::runtime> its_runtime
        = vsomeip::runtime::get();

    if (!its_runtime) {
        return -1;
    }

    std::shared_ptr<vsomeip::application> its_application
        = its_runtime->create_application(VSOMEIP_ROUTING);

    if (its_application->init()) {
        its_application->start();
        return 0;
    } else {
        return -1;
    }
}

/*
 * Parse command line options
 * -h | --help          print usage information
 * -d | --daemonize     start background processing by forking the process
 *
 * and start processing.
 */
int main(int argc, char **argv) {
    bool must_daemonize(false);
    if (argc > 1) {
        for (int i = 0; i < argc; i++) {
            std::string its_argument(argv[i]);
            if (its_argument == "-d" || its_argument == "--daemonize") {
                must_daemonize = true;
            } else if (its_argument == "-h" || its_argument == "--help") {
                std::cout << "usage: "
                        << argv[0] << " [-h|--help][-d|--daemonize]"
                        << std::endl;
                return 0;
            }
        }
    }

    /* Fork the process if processing shall be done in the background */
    if (must_daemonize) {
        pid_t its_process, its_signature;

        its_process = fork();

        if (its_process < 0) {
            return EXIT_FAILURE;
        }

        if (its_process > 0) {
            return EXIT_SUCCESS;
        }

        umask(0);

        its_signature = setsid();
        if (its_signature < 0) {
            return EXIT_FAILURE;
        }
    }

    return process();
}
