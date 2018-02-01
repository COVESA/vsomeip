###############################################################################
## build documentation
###############################################################################
add_custom_target(doc)

find_package(Doxygen)
if(NOT DOXYGEN_FOUND)
    message(WARNING "Doxygen is not installed. Documentation can not be built.")
else()
    # set configuration variables for doxygen.in
    set(PROJECT "vsomeip")
    set(DOCDIR documentation)
    set(SRCDIR .)
    set(GENERATE_HTML YES)
    set(GENERATE_HTMLHELP NO)
    set(GENERATE_CHI NO)
    set(GENERATE_LATEX NO)
    set(GENERATE_PDF NO)
    set(GENERATE_RTF NO)
    set(GENERATE_MAN NO)
    set(GENERATE_XML NO)
    set(HAVE_DOT YES)

    if(HAVE_DOT)
        # Note: the @DOT_PATH@ variable won't be used in doxygen.in as doxygen
        # somehow manages to strip the last slash from the path and therfore no
        # graphs are generated. Therefore dot should be available in your $PATH
        find_program(DOT_PATH dot)
        if("${DOT_PATH}" STREQUAL "DOT_PATH-NOTFOUND")
            message(WARNING "dot (graphviz) is not installed. Graphs in documentation can't be generated.")
        else()
            message("dot found")
        endif()
    endif()

    configure_file(doxygen.in ${PROJECT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doxygen-doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/Doxyfile
        SOURCES ${PROJECT_BINARY_DIR}/Doxyfile
        )

    add_dependencies(doc doxygen-doc)
endif()

find_program(ASCIIDOC_PATH asciidoc)
find_program(SOURCE_HIGHLIGHT_PATH source-highlight)
if("${ASCIIDOC_PATH}" STREQUAL "ASCIIDOC_PATH-NOTFOUND")
    message(WARNING "asciidoc is not installed. Readme can not be built.")
elseif("${SOURCE_HIGHLIGHT_PATH}" STREQUAL "SOURCE_HIGHLIGHT_PATH-NOTFOUND")
    message(WARNING "source-highlight is not installed. Readme can not be built.")
else()
    message("asciidoc found")
    message("source-highlight found")
    add_custom_command(TARGET doc
        POST_BUILD
        COMMAND asciidoc
        -a version=${VSOMEIP_VERSION}
        -b html
        -o vsomeipUserGuide.html
        ${CMAKE_CURRENT_SOURCE_DIR}/vsomeipUserGuide
        )
endif()


