cmake_minimum_required(VERSION 3.29)
project(toyparser C)

set(CMAKE_C_STANDARD 11)

# Build the highlighter library
add_library(highlighter parser.c parser_hightlighter.c)
# Include codebuffer in the include directories
target_include_directories(highlighter PRIVATE . ${CMAKE_SOURCE_DIR}/codebuffer)

add_executable(tp main.c)
# Include codebuffer in the include directories
target_include_directories(tp PRIVATE . ${CMAKE_SOURCE_DIR}/codebuffer)
# Link the parser to the highlighter
target_link_libraries(tp highlighter)

# Link codebuffer to the parser
target_link_libraries(tp code_buffer)

