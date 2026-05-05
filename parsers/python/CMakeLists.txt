cmake_minimum_required(VERSION 3.29)
project(python_parser C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DslshRpath.cmake")

FetchContent_Declare(
        tree_sitter_python
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-python.git
        GIT_TAG v0.25.0
        GIT_SHALLOW TRUE
)
FetchContent_GetProperties(tree_sitter_python)
if(NOT EXISTS "${tree_sitter_python_SOURCE_DIR}/src/parser.c")
    dslsh_fetchcontent_populate(tree_sitter_python)
endif()

add_library(dslsh_tree_sitter_python_grammar STATIC
        ${tree_sitter_python_SOURCE_DIR}/src/parser.c)
target_include_directories(dslsh_tree_sitter_python_grammar PRIVATE
        ${tree_sitter_SOURCE_DIR}/lib/include
        ${tree_sitter_python_SOURCE_DIR}/src)

if(EXISTS "${tree_sitter_python_SOURCE_DIR}/src/scanner.c")
    target_sources(dslsh_tree_sitter_python_grammar PRIVATE
            ${tree_sitter_python_SOURCE_DIR}/src/scanner.c)
endif()

add_library(python_highlighter STATIC python_parser.c python_parser.h)
target_include_directories(python_highlighter PRIVATE .)
target_link_libraries(python_highlighter PRIVATE
        DSLSH::tree_sitter_common
        dslsh_tree_sitter_python_grammar)

add_executable(pyp main.c)
target_include_directories(pyp PRIVATE .)
target_link_libraries(pyp PRIVATE python_highlighter DSLSH::parser)
dslsh_set_install_rpath(pyp)

if(DSLSH_BUILD_TESTS)
    add_executable(python_parser_test python_parser_test.c)
    target_include_directories(python_parser_test PRIVATE .)
    target_link_libraries(python_parser_test PRIVATE python_highlighter DSLSH::parser)
    add_test(NAME python_parser_test COMMAND python_parser_test)
endif()

if(DSLSH_INSTALL)
    install(TARGETS pyp DESTINATION bin)
endif()
