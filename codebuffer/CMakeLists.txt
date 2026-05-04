cmake_minimum_required(VERSION 3.29)
project(codebuffer C)

set(CMAKE_C_STANDARD 11)

if(NOT DEFINED DSLSH_INSTALL)
    if(PROJECT_IS_TOP_LEVEL)
        set(DSLSH_INSTALL ON)
    else()
        set(DSLSH_INSTALL OFF)
    endif()
endif()
if(NOT DEFINED DSLSH_BUILD_TESTS)
    if(PROJECT_IS_TOP_LEVEL)
        set(DSLSH_BUILD_TESTS ON)
    else()
        set(DSLSH_BUILD_TESTS OFF)
    endif()
endif()

# Common sources for both editor and parser
set(COMMON_SOURCES
        code_buffer.c 
        comms.c 
        utf8.h
        dslsyntax_common.c 
        dslsyntax_common.h
        dslsyntax_log.c
        dslsyntax_log.h
        serialization.c 
        serialization.h
        diff.c 
        diff.h
        emergency_parse.c
        thread_utils.c 
        thread_utils.h
)

# Add the library for the editor support
add_library(dslsyntax_editor
        ${COMMON_SOURCES}
        dslsyntax_editor.c 
        dslsyntax_editor.h)
target_include_directories(dslsyntax_editor PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/dslsyntax>)
add_library(DSLSH::editor ALIAS dslsyntax_editor)

# Add the library for the highlighter (parser)
add_library(dslsyntax_parser
        ${COMMON_SOURCES}
        dslsyntax_parser.c 
        dslsyntax_parser.h)
target_include_directories(dslsyntax_parser PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/dslsyntax>)
add_library(DSLSH::parser ALIAS dslsyntax_parser)

if(WIN32)
    target_link_libraries(dslsyntax_editor PUBLIC ws2_32)
    target_link_libraries(dslsyntax_parser PUBLIC ws2_32)
endif()

if(DSLSH_BUILD_TESTS)
    # Test programs
    if(TARGET highlighter)
        add_executable(code_test EXCLUDE_FROM_ALL test.c)
        target_link_libraries(code_test dslsyntax_editor dslsyntax_parser)
        target_link_libraries(code_test highlighter)
        target_include_directories(code_test PRIVATE . ${DSL_Syntax_Highlighter_SOURCE_DIR}/toyparser)
    endif()

    add_executable(token_test EXCLUDE_FROM_ALL token_test.c)
    target_link_libraries(token_test dslsyntax_parser)

    add_executable(serialization_test serialization_test.c)
    target_link_libraries(serialization_test dslsyntax_parser)

    add_executable(diff_test diff_test.c)
    target_link_libraries(diff_test dslsyntax_editor)

    add_executable(ep_test ep_test.c)
    target_link_libraries(ep_test dslsyntax_editor)

    add_executable(fake_editor EXCLUDE_FROM_ALL fake_editor.c)
    target_link_libraries(fake_editor dslsyntax_editor)

    add_executable(fake_parser EXCLUDE_FROM_ALL fake_parser.c)
    target_link_libraries(fake_parser dslsyntax_parser)

    add_executable(socket_test socket_test.c)
    if(WIN32)
        target_link_libraries(socket_test dslsyntax_parser ws2_32)
    else()
        target_link_libraries(socket_test dslsyntax_parser pthread)
    endif()

    add_executable(stdio_test stdio_test.c)
    target_link_libraries(stdio_test dslsyntax_editor)

    add_executable(stdio_shutdown_test stdio_shutdown_test.c)
    target_link_libraries(stdio_shutdown_test dslsyntax_editor)

    add_executable(protocol_test_parser EXCLUDE_FROM_ALL protocol_test_parser.c)
    target_link_libraries(protocol_test_parser dslsyntax_parser)

    add_executable(protocol_test protocol_test.c)
    target_link_libraries(protocol_test dslsyntax_editor)
    add_dependencies(protocol_test protocol_test_parser)

    # --- Automated Testing ---
    add_test(NAME serialization_test COMMAND serialization_test)
    add_test(NAME diff_test COMMAND diff_test)
    add_test(NAME ep_test COMMAND ep_test)
    add_test(NAME socket_test COMMAND socket_test)
    set_tests_properties(socket_test PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME stdio_test COMMAND stdio_test)
    add_test(NAME stdio_shutdown_test COMMAND stdio_shutdown_test)
    set_tests_properties(stdio_shutdown_test PROPERTIES TIMEOUT 5)
    add_test(NAME protocol_test COMMAND protocol_test $<TARGET_FILE:protocol_test_parser>)
endif()

if(DSLSH_INSTALL)
    install(TARGETS dslsyntax_editor dslsyntax_parser
            ARCHIVE DESTINATION lib
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin)
    install(FILES
            dslsyntax_common.h
            dslsyntax_editor.h
            dslsyntax_log.h
            dslsyntax_parser.h
            diff.h
            serialization.h
            thread_utils.h
            utf8.h
            DESTINATION include/dslsyntax)
endif()
