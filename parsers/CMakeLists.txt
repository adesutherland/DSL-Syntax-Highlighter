function(dslsh_fetchcontent_populate dependency)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(${dependency})

    foreach(field SOURCE_DIR BINARY_DIR POPULATED)
        if(DEFINED ${dependency}_${field})
            set(${dependency}_${field} "${${dependency}_${field}}" PARENT_SCOPE)
        endif()
    endforeach()
endfunction()

if(DSLSH_BUILD_C OR DSLSH_BUILD_PYTHON OR DSLSH_BUILD_JAVASCRIPT)
    add_subdirectory(tree_sitter_common)
endif()

if(DSLSH_BUILD_C)
    add_subdirectory(c)
endif()

if(DSLSH_BUILD_PYTHON)
    add_subdirectory(python)
endif()

if(DSLSH_BUILD_JAVASCRIPT)
    add_subdirectory(javascript)
endif()

if(DSLSH_BUILD_MARKDOWN)
    add_subdirectory(markdown)
endif()
