# Copyright (C) 2015-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_custom_target(examples)

set(EXAMPLE_CONFIG_FILES
    "../config/vsomeip.json"
    "../config/vsomeip-local.json"
    "../config/vsomeip-tcp-client.json"
    "../config/vsomeip-tcp-service.json"
    "../config/vsomeip-udp-client.json"
    "../config/vsomeip-udp-service.json"
)

list(APPEND all_examples
    request-sample
    response-sample
    subscribe-sample
    notify-sample
    )

foreach(example ${all_examples})
    add_executable(${example} ${example}.cpp ${EXAMPLE_CONFIG_FILES})
    target_link_libraries(${example} vsomeip::vsomeip)
    set_target_properties(${example} PROPERTIES CXX_STANDARD 14)
endforeach()

add_dependencies(examples ${all_examples})

install(
    TARGETS ${all_examples}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT bin
    )
