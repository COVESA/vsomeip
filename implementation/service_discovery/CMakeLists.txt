file(GLOB vsomeip-sd_SRC src/*.cpp)

add_library(vsomeip-sd ${vsomeip-sd_SRC})
set_target_properties(vsomeip-sd PROPERTIES
    VERSION ${VSOMEIP_VERSION}
    SOVERSION ${VSOMEIP_MAJOR_VERSION}
    LINK_FLAGS "-Wl,--as-needed -Wl,--no-undefined"
    CXX_STANDARD 14
    )

if(MSVC)
    set_target_properties(vsomeip-sd PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION")
endif()

target_link_libraries(vsomeip-sd
    vsomeip::vsomeip
    Boost::system
    Boost::log
    Threads::Threads
    ${SystemD_LIBRARIES}
    )

install(TARGETS vsomeip-sd
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT shlib
    )
