cmake_minimum_required(VERSION 3.29)
project(codebuffer C)

set(CMAKE_C_STANDARD 11)

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

# Add the library for the highlighter (parser)
add_library(dslsyntax_parser
        ${COMMON_SOURCES}
        dslsyntax_parser.c 
        dslsyntax_parser.h)

# Test programs
add_executable(code_test test.c)
target_link_libraries(code_test dslsyntax_editor dslsyntax_parser)
target_link_libraries(code_test highlighter)
target_include_directories(code_test PRIVATE . ${CMAKE_SOURCE_DIR}/toyparser)

add_executable(token_test token_test.c)
target_link_libraries(token_test dslsyntax_parser)

add_executable(serialization_test serialization_test.c)
target_link_libraries(serialization_test dslsyntax_parser)

add_executable(socket_test socket_test.c)
target_link_libraries(socket_test dslsyntax_parser pthread)

add_executable(diff_test diff_test.c)
target_link_libraries(diff_test dslsyntax_editor)

add_executable(fake_editor fake_editor.c)
target_link_libraries(fake_editor dslsyntax_editor)

add_executable(fake_parser fake_parser.c)
target_link_libraries(fake_parser dslsyntax_parser)

# --- Automated Testing ---
enable_testing()

add_test(NAME serialization_test COMMAND serialization_test)
add_test(NAME socket_test COMMAND socket_test)
add_test(NAME diff_test COMMAND diff_test)
