# Copyright (C) 2015-2022 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

##############################################################################
# add network, unit and benchmark tests directories
##############################################################################
add_subdirectory( network_tests EXCLUDE_FROM_ALL )

if (NOT DISABLE_SECURITY)
    add_subdirectory( unit_tests EXCLUDE_FROM_ALL )

    if(benchmark_FOUND)
        add_subdirectory( benchmark_tests EXCLUDE_FROM_ALL )
    endif()
endif()

##############################################################################
# Add internal_routing_disabled_acceptance_test
##############################################################################

if(NOT ${TESTS_BAT})
    add_subdirectory(internal_routing_disabled_acceptance_test)
endif()
