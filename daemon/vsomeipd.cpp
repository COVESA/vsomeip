// Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include <iostream>

#include <vsomeip/vsomeip.hpp>
#include "../implementation/configuration/include/internal.hpp"
#include "../implementation/logging/include/logger.hpp"

#ifdef USE_DLT
#include <dlt/dlt.h>
#include "../implementation/logging/include/defines.hpp"
#endif

static std::shared_ptr<vsomeip::application> its_application;

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
/*
 * Handle signal to stop the daemon
 */
void vsomeipd_stop(int _signal) {
    if (_signal == SIGINT || _signal == SIGTERM) {
        its_application->stop();
    }
    if (_signal == SIGUSR1) {
        VSOMEIP_INFO << "Suspending service discovery";
        its_application->set_routing_state(vsomeip::routing_state_e::RS_SUSPENDED);
    }
    if (_signal == SIGUSR2) {
        VSOMEIP_INFO << "Resuming service discovery";
        its_application->set_routing_state(vsomeip::routing_state_e::RS_RESUMED);
    }
}
#endif

/*
 * Create a vsomeip application object and start it.
 */
int vsomeipd_process(bool _is_quiet) {
#ifdef USE_DLT
    if (!_is_quiet)
        DLT_REGISTER_APP(VSOMEIP_LOG_DEFAULT_APPLICATION_ID, VSOMEIP_LOG_DEFAULT_APPLICATION_NAME);
#else
    (void)_is_quiet;
#endif

    std::shared_ptr<vsomeip::runtime> its_runtime
        = vsomeip::runtime::get();

    if (!its_runtime) {
        return -1;
    }

    // Create the application object
    its_application = its_runtime->create_application(VSOMEIP_ROUTING);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    // Handle signals
    signal(SIGINT, vsomeipd_stop);
    signal(SIGTERM, vsomeipd_stop);
    signal(SIGUSR1, vsomeipd_stop);
    signal(SIGUSR2, vsomeipd_stop);
#endif
    if (its_application->init()) {
        if (its_application->is_routing()) {
            its_application->start();
            return 0;
        }
        VSOMEIP_ERROR << "vsomeipd has not been configured as routing - abort";
    }
    return -1;
}

/*
 * Parse command line options
 * -h | --help          print usage information
 * -d | --daemonize     start background processing by forking the process
 * -q | --quiet         do _not_ use dlt logging
 *
 * and start processing.
 */
int main(int argc, char **argv) {
    bool must_daemonize(false);
    bool is_quiet(false);
    if (argc > 1) {
        for (int i = 0; i < argc; i++) {
            std::string its_argument(argv[i]);
            if (its_argument == "-d" || its_argument == "--daemonize") {
                must_daemonize = true;
            } else if (its_argument == "-q" || its_argument == "--quiet") {
                is_quiet = true;
            } else if (its_argument == "-h" || its_argument == "--help") {
                std::cout << "usage: "
                        << argv[0] << " [-h|--help][-d|--daemonize][-q|--quiet]"
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

    return vsomeipd_process(is_quiet);
}
