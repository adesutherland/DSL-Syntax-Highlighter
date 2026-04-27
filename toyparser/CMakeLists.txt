cmake_minimum_required(VERSION 3.29)
project(toyparser C)

set(CMAKE_C_STANDARD 11)

# Build the highlighter library
add_library(highlighter parser.c parser_hightlighter.c)
# Include codebuffer in the include directories
target_include_directories(highlighter PRIVATE .)
target_link_libraries(highlighter PRIVATE DSLSH::parser)

add_executable(tp main.c)
# Include codebuffer in the include directories
target_include_directories(tp PRIVATE .)
# Link the parser to the highlighter
target_link_libraries(tp highlighter)

# Link codebuffer to the parser
target_link_libraries(tp DSLSH::parser)

# Install target
if(DSLSH_INSTALL)
    install(TARGETS tp DESTINATION bin)
endif()
