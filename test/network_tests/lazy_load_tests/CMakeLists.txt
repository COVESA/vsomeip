# Copyright (C) 2014-2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

project(lazy_load_tests LANGUAGES CXX)

if (TEST_SECURITY)

    # Configure necessary files into build folder.
    set(configuration_files
        lazy_load_test_start.sh
        vsomeip/lazy_load_test_config.json
        vsomeip/vsomeip_policy_extensions.json
        vsomeip/vsomeip_ext/0_0/vsomeip_security.json
        vsomeip/vsomeip_ext/android/1_1/vsomeip_security.json
    )
    configure_files("${configuration_files}")

    add_executable(lazy_load_test_service
        lazy_load_test_service.cpp
    )

    add_executable(lazy_load_test_client
        lazy_load_test_client.cpp
    )

    add_executable(lazy_load_test_lazy_client
        lazy_load_test_lazy_client.cpp
    )

    add_library(gethostname SHARED
        gethostname.cpp
    )

    # Add build dependencies and link libraries to executables.
    set(executables
        lazy_load_test_service
        lazy_load_test_client
        lazy_load_test_lazy_client
    )
    targets_link_default_libraries("${executables}")
    targets_add_default_dependencies("${executables}")

    add_dependencies(build_network_tests gethostname)
    # logs won't be generated for this test as there is one application which
    # will run as uid:1 gid:1, which is not allowed to write to the log file
    list(FILTER TEST_ENTRYPOINT EXCLUDE REGEX "^--log-file=")

    add_custom_test(
        NAME lazy_load_test
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/lazy_load_test_start.sh
        SEQUENTIAL
    )

endif()
