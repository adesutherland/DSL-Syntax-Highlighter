include(FetchContent)

FetchContent_Declare(
        tree_sitter
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
        GIT_TAG v0.26.8
        GIT_SHALLOW TRUE
)
FetchContent_GetProperties(tree_sitter)
if(NOT EXISTS "${tree_sitter_SOURCE_DIR}/lib/src/lib.c")
    dslsh_fetchcontent_populate(tree_sitter)
endif()

add_library(dslsh_tree_sitter_runtime STATIC
        ${tree_sitter_SOURCE_DIR}/lib/src/lib.c)
target_include_directories(dslsh_tree_sitter_runtime PUBLIC
        ${tree_sitter_SOURCE_DIR}/lib/include)
target_include_directories(dslsh_tree_sitter_runtime PRIVATE
        ${tree_sitter_SOURCE_DIR}/lib/src)
add_library(DSLSH::tree_sitter_runtime ALIAS dslsh_tree_sitter_runtime)

add_library(dslsh_tree_sitter_common STATIC
        tree_sitter_adapter.c
        tree_sitter_adapter.h)
target_include_directories(dslsh_tree_sitter_common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dslsh_tree_sitter_common PUBLIC
        DSLSH::parser
        DSLSH::tree_sitter_runtime)
add_library(DSLSH::tree_sitter_common ALIAS dslsh_tree_sitter_common)
