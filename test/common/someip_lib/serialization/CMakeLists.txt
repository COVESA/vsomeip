project (serialization)

# ----------------------------------------------------------------------------
# Include all source files (cpp/hpp)
# ----------------------------------------------------------------------------
file (GLOB SRC src/*.cpp src/*.hpp)
file (GLOB INC include/*.hpp include/serialization/*.hpp)

# ----------------------------------------------------------------------------
# Declare the library
# ----------------------------------------------------------------------------
add_library (
    ${PROJECT_NAME} SHARED
    ${SRC}
    ${INC}
)

target_link_libraries (
    ${PROJECT_NAME}
    PUBLIC
        ${BOOST_LIBS}
        utils
)

# ----------------------------------------------------------------------------
# Specify here the include directories exported
# by this library
# ----------------------------------------------------------------------------
target_include_directories (
    ${PROJECT_NAME}
    PUBLIC include
    PRIVATE src
)
