file(GLOB vsomeip-cfg_SRC src/*.cpp)

add_library(vsomeip-cfg ${vsomeip-cfg_SRC})
set_target_properties(vsomeip-cfg PROPERTIES
    VERSION ${VSOMEIP_VERSION}
    SOVERSION ${VSOMEIP_MAJOR_VERSION}
    LINK_FLAGS "-Wl,--as-needed -Wl,--no-undefined"
    CXX_STANDARD 14
    )
target_compile_definitions(vsomeip-cfg PUBLIC ${OS_COMPILER_DEFINITIONS})
target_compile_options(vsomeip-cfg PUBLIC ${OS_COMPILER_FLAGS})
target_link_libraries(vsomeip-cfg
    PUBLIC
        vsomeip::vsomeip
    PRIVATE
        Boost::system
        Boost::log
        Boost::regex
        Threads::Threads
        ${Boost_LIBRARIES}
        ${SystemD_LIBRARIES}
    )

if(MSVC)
    set_target_properties(vsomeip-cfg PROPERTIES COMPILE_DEFINITIONS "VSOMEIP_DLL_COMPILATION_CONFIG")
endif()

install(TARGETS vsomeip-cfg
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT shlib
    )
