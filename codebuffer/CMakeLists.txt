cmake_minimum_required(VERSION 3.29)
project(codebuffer C)

set(CMAKE_C_STANDARD 11)

# Add the code buffer library
add_library(code_buffer code_buffer.c token_buffer.c comms.c utf8.h
        comms.c thread_utils.c thread_utils.h token_buffer.h)

# Test programs
add_executable(code_test test.c)
target_link_libraries(code_test code_buffer)
target_link_libraries(code_test highlighter)
# Include toyparser in the include directories
target_include_directories(code_test PRIVATE . ${CMAKE_SOURCE_DIR}/toyparser)

add_executable(token_test token_test.c)
target_link_libraries(token_test code_buffer)
