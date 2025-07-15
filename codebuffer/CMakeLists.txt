cmake_minimum_required(VERSION 3.29)
project(codebuffer C)

set(CMAKE_C_STANDARD 11)

# Add the library for the editor support
add_library(dslsyntax_editor
        code_buffer.c comms.c utf8.h
        dslsyntax_common.c dslsyntax_common.h
        thread_utils.c thread_utils.h
        dslsyntax_editor.c dslsyntax_editor.h)

# Add the library for the highlighter (parser)
add_library(dslsyntax_parser
        code_buffer.c comms.c utf8.h
        dslsyntax_common.c dslsyntax_common.h
        thread_utils.c thread_utils.h
        dslsyntax_parser.c dslsyntax_parser.h)

# Test programs
add_executable(code_test test.c)
target_link_libraries(code_test dslsyntax_editor)
target_link_libraries(code_test dslsyntax_parser)
# Include toyparser
target_link_libraries(code_test highlighter)
target_include_directories(code_test PRIVATE . ${CMAKE_SOURCE_DIR}/toyparser)

add_executable(token_test token_test.c)
target_link_libraries(token_test dslsyntax_parser)
