cmake_minimum_required(VERSION 3.14)
project(toyparser C)

set(CMAKE_C_STANDARD 11)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DslshRpath.cmake")

# Build the highlighter library
add_library(highlighter STATIC parser.c parser_hightlighter.c)
target_include_directories(highlighter PUBLIC .)
target_link_libraries(highlighter PUBLIC DSLSH::parser)

add_executable(tp main.c)
# Include codebuffer in the include directories
target_include_directories(tp PRIVATE .)
target_link_libraries(tp PRIVATE highlighter)
dslsh_set_install_rpath(tp)

# Install target
if(DSLSH_INSTALL)
    install(TARGETS tp DESTINATION bin)
endif()
