cmake_minimum_required(VERSION 3.14)
project(javascript_parser C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DslshRpath.cmake")

FetchContent_Declare(
        tree_sitter_javascript
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-javascript.git
        GIT_TAG 58404d8cf191d69f2674a8fd507bd5776f46cb11
)
FetchContent_GetProperties(tree_sitter_javascript)
if(NOT EXISTS "${tree_sitter_javascript_SOURCE_DIR}/src/parser.c")
    dslsh_fetchcontent_populate(tree_sitter_javascript)
endif()

add_library(dslsh_tree_sitter_javascript_grammar STATIC
        ${tree_sitter_javascript_SOURCE_DIR}/src/parser.c)
target_include_directories(dslsh_tree_sitter_javascript_grammar PRIVATE
        ${tree_sitter_SOURCE_DIR}/lib/include
        ${tree_sitter_javascript_SOURCE_DIR}/src)

if(EXISTS "${tree_sitter_javascript_SOURCE_DIR}/src/scanner.c")
    target_sources(dslsh_tree_sitter_javascript_grammar PRIVATE
            ${tree_sitter_javascript_SOURCE_DIR}/src/scanner.c)
endif()

add_library(javascript_highlighter STATIC javascript_parser.c javascript_parser.h)
target_include_directories(javascript_highlighter PUBLIC .)
target_link_libraries(javascript_highlighter
    PUBLIC
        DSLSH::tree_sitter_common
    PRIVATE
        dslsh_tree_sitter_javascript_grammar)

add_executable(jsp main.c)
target_include_directories(jsp PRIVATE .)
target_link_libraries(jsp PRIVATE javascript_highlighter)
dslsh_set_install_rpath(jsp)

if(DSLSH_BUILD_TESTS)
    add_executable(javascript_parser_test javascript_parser_test.c)
    target_include_directories(javascript_parser_test PRIVATE .)
    target_link_libraries(javascript_parser_test PRIVATE javascript_highlighter)
    add_test(NAME javascript_parser_test COMMAND javascript_parser_test)

    add_executable(javascript_shutdown_test javascript_shutdown_test.c)
    target_include_directories(javascript_shutdown_test PRIVATE .)
    target_link_libraries(javascript_shutdown_test PRIVATE DSLSH::editor)
    add_test(NAME javascript_shutdown_test COMMAND javascript_shutdown_test $<TARGET_FILE:jsp>)
    set_tests_properties(javascript_shutdown_test PROPERTIES TIMEOUT 5)
endif()

if(DSLSH_INSTALL)
    install(TARGETS jsp DESTINATION bin)
endif()
