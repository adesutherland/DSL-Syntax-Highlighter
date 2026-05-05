cmake_minimum_required(VERSION 3.29)
project(c_parser C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DslshRpath.cmake")

FetchContent_Declare(
        tree_sitter_c
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-c.git
        GIT_TAG b780e47fc780ddc8da13afa35a3f4ed5c157823d
)
FetchContent_GetProperties(tree_sitter_c)
if(NOT EXISTS "${tree_sitter_c_SOURCE_DIR}/src/parser.c")
    dslsh_fetchcontent_populate(tree_sitter_c)
endif()

add_library(dslsh_tree_sitter_c_grammar STATIC
        ${tree_sitter_c_SOURCE_DIR}/src/parser.c)
target_include_directories(dslsh_tree_sitter_c_grammar PRIVATE
        ${tree_sitter_SOURCE_DIR}/lib/include
        ${tree_sitter_c_SOURCE_DIR}/src)

if(EXISTS "${tree_sitter_c_SOURCE_DIR}/src/scanner.c")
    target_sources(dslsh_tree_sitter_c_grammar PRIVATE
            ${tree_sitter_c_SOURCE_DIR}/src/scanner.c)
endif()

add_library(c_highlighter STATIC c_parser.c c_parser.h)
target_include_directories(c_highlighter PRIVATE .)
target_link_libraries(c_highlighter PRIVATE
        DSLSH::tree_sitter_common
        dslsh_tree_sitter_c_grammar)

add_executable(dslsh-c main.c)
target_include_directories(dslsh-c PRIVATE .)
target_link_libraries(dslsh-c PRIVATE c_highlighter DSLSH::parser)
dslsh_set_install_rpath(dslsh-c)

if(DSLSH_BUILD_TESTS)
    add_executable(c_parser_test c_parser_test.c)
    target_include_directories(c_parser_test PRIVATE .)
    target_link_libraries(c_parser_test PRIVATE c_highlighter DSLSH::parser)
    add_test(NAME c_parser_test COMMAND c_parser_test)
endif()

if(DSLSH_INSTALL)
    install(TARGETS dslsh-c DESTINATION bin)
endif()
