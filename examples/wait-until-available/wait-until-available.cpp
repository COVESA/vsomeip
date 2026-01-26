// Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cstdio>
#include <iostream>
#include <iomanip>
#include <thread>
#include <future>

#include <vsomeip/vsomeip.hpp>

void print_help() {
    std::cout << "Usage: wait-until-available <service-id> [-h|--help][-t|--timeout <s>]" << std::endl;
    std::cout << "  <service-id>           Service to wait for, in hex" << std::endl;
    std::cout << "  -t, --timeout <s>     Timeout in seconds (default: 30)" << std::endl;
    std::cout << "  -h, --help             Show this help message" << std::endl;
}

int main(int argc, char** argv) {
    vsomeip::service_t service_id = 0;
    std::chrono::seconds timeout = std::chrono::seconds(10);

    if (argc < 2) {
        print_help();
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "-h" || arg == "--help") {
            print_help();
            return EXIT_SUCCESS;
        } else if (i == 1) {
            // parse service-id
            try {
                service_id = static_cast<vsomeip::service_t>(std::stoul(arg, nullptr, 16));
            } catch (...) {
                std::cerr << "Error: could not parse service-id '" << arg << "'" << std::endl;
                print_help();
                return EXIT_FAILURE;
            }
        } else if (arg == "-t" || arg == "--timeout") {
            if (i + 1 < argc) {
                timeout = std::chrono::seconds(std::stoul(argv[++i]));
            } else {
                std::cerr << "Error: --timeout requires a value" << std::endl;
                print_help();
                return EXIT_FAILURE;
            }
        } else {
            std::cerr << "Error: unknown argument '" << arg << "'" << std::endl;
            print_help();
            return EXIT_FAILURE;
        }
    }

    std::shared_ptr<vsomeip::application> app = vsomeip::runtime::get()->create_application("wait-until-available");
    if (!app->init()) {
        std::cerr << "Couldn't initialize application" << std::endl;
        return EXIT_FAILURE;
    }

    std::promise<bool> availability_promise;
    std::future<bool> availability_future = availability_promise.get_future();
    app->register_availability_handler(
            service_id, vsomeip::ANY_INSTANCE,
            [&availability_promise](vsomeip::service_t /*_service*/, vsomeip::instance_t /*_instance*/, bool _is_available) {
                if (_is_available) {
                    try {
                        availability_promise.set_value(true);
                    } catch (...) {
                        // in case there is a "double" available; avoids the "promise already satisfied" exception
                    }
                }
            });
    app->request_service(service_id, vsomeip::ANY_INSTANCE);

    std::thread app_thread([app]() { app->start(); });

    bool available = (availability_future.wait_for(timeout) == std::future_status::ready);
    std::cout << "Service [" << std::hex << std::setfill('0') << std::setw(4) << service_id << "] is "
              << (available ? "available" : "NOT available") << std::endl;

    app->stop();
    app_thread.join();
    return available ? EXIT_SUCCESS : EXIT_FAILURE;
}
