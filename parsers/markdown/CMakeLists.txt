cmake_minimum_required(VERSION 3.29)
project(markdown_parser C)

set(CMAKE_C_STANDARD 11)

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/DslshRpath.cmake")

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
set(CMARK_LIB_FUZZER OFF CACHE BOOL "" FORCE)
set(CMARK_FUZZ_QUADRATIC OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        cmark_gfm
        GIT_REPOSITORY https://github.com/github/cmark-gfm.git
        GIT_TAG 0.29.0.gfm.13
)
FetchContent_MakeAvailable(cmark_gfm)

add_library(markdown_highlighter STATIC markdown_parser.c markdown_parser.h)
target_include_directories(markdown_highlighter PRIVATE
        .
        ${cmark_gfm_SOURCE_DIR}/src
        ${cmark_gfm_BINARY_DIR}/src
        ${cmark_gfm_SOURCE_DIR}/extensions
        ${cmark_gfm_BINARY_DIR}/extensions)
target_compile_definitions(markdown_highlighter PRIVATE
        CMARK_GFM_STATIC_DEFINE
        CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
target_link_libraries(markdown_highlighter PRIVATE
        DSLSH::parser
        libcmark-gfm_static
        libcmark-gfm-extensions_static)

add_executable(mdp main.c)
target_include_directories(mdp PRIVATE .)
target_link_libraries(mdp PRIVATE markdown_highlighter DSLSH::parser)
dslsh_set_install_rpath(mdp)

if(DSLSH_BUILD_TESTS)
    add_executable(markdown_parser_test markdown_parser_test.c)
    target_include_directories(markdown_parser_test PRIVATE .)
    target_link_libraries(markdown_parser_test PRIVATE markdown_highlighter DSLSH::parser)
    add_test(NAME markdown_parser_test COMMAND markdown_parser_test)
endif()

if(DSLSH_INSTALL)
    install(TARGETS mdp DESTINATION bin)
endif()
