cmake_minimum_required(VERSION 3.14)
project(DSL_Syntax_Highlighter LANGUAGES C)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  # Fallback to local user bin to avoid sudo permission errors during testing
  if(WIN32)
    set(CMAKE_INSTALL_PREFIX "$ENV{USERPROFILE}/.local" CACHE PATH "Default install prefix" FORCE)
  else()
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Default install prefix" FORCE)
  endif()
endif()

include(CTest)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory -fPIE -pie -fno-omit-frame-pointer -g -fno-optimize-sibling-calls -O1")

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(DSLSH_DEFAULT_FULL_BUILD ON)
else()
  set(DSLSH_DEFAULT_FULL_BUILD OFF)
endif()

option(DSLSH_BUILD_CORE "Build the DSLSH middleware libraries" ON)
option(DSLSH_BUILD_TOOLS "Build DSLSH middleware tools" ${DSLSH_DEFAULT_FULL_BUILD})
option(DSLSH_BUILD_EXAMPLES "Build DSLSH example editor and parser" ${DSLSH_DEFAULT_FULL_BUILD})
option(DSLSH_BUILD_PARSERS "Build bundled DSLSH parser adapters" ${DSLSH_DEFAULT_FULL_BUILD})
option(DSLSH_BUILD_TESTS "Build DSLSH tests" ${DSLSH_DEFAULT_FULL_BUILD})
option(DSLSH_BUILD_MARKDOWN "Build the bundled Markdown parser adapter" ${DSLSH_BUILD_PARSERS})
option(DSLSH_BUILD_C "Build the bundled C parser adapter" ${DSLSH_BUILD_PARSERS})
option(DSLSH_BUILD_PYTHON "Build the bundled Python parser adapter" ${DSLSH_BUILD_PARSERS})
option(DSLSH_BUILD_JAVASCRIPT "Build the bundled JavaScript parser adapter" ${DSLSH_BUILD_PARSERS})
option(DSLSH_INSTALL "Install DSLSH build products" ${DSLSH_DEFAULT_FULL_BUILD})

include(cmake/DslshRpath.cmake)

if((DSLSH_BUILD_TOOLS OR DSLSH_BUILD_EXAMPLES OR DSLSH_BUILD_PARSERS OR DSLSH_BUILD_TESTS)
    AND NOT DSLSH_BUILD_CORE)
  message(FATAL_ERROR "DSLSH tools, examples, parsers, and tests require DSLSH_BUILD_CORE=ON")
endif()

if(DSLSH_BUILD_CORE)
  add_subdirectory(codebuffer)
endif()

if(DSLSH_BUILD_EXAMPLES)
  set(BUILD_TOYEDITOR OFF)

  add_subdirectory(toyparser)

  if(WIN32)
    set(PDCURSESMOD_SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/third_party/PDCursesMod"
        CACHE PATH
        "Path to a vendored PDCursesMod checkout")

    if(EXISTS "${PDCURSESMOD_SOURCE_DIR}/curses.h")
      set(BUILD_TOYEDITOR ON)
      message(STATUS "Building toyeditor with PDCursesMod from ${PDCURSESMOD_SOURCE_DIR}")
    else()
      message(STATUS "Skipping toyeditor: PDCursesMod not found at ${PDCURSESMOD_SOURCE_DIR}")
    endif()
  else()
    find_package(Curses QUIET)
    if(Curses_FOUND)
      set(BUILD_TOYEDITOR ON)
      message(STATUS "Building toyeditor with curses library: ${CURSES_LIBRARIES}")
    else()
      message(STATUS "Skipping toyeditor: curses library not found")
    endif()
  endif()

  if(BUILD_TOYEDITOR)
    add_subdirectory(testeditor)
  endif()
endif()

if(DSLSH_BUILD_PARSERS)
  add_subdirectory(parsers)
endif()

if(DSLSH_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(DSLSH_INSTALL)
  install(FILES docs/PARSER_TESTER.md DESTINATION share/doc/dslsyntax)
endif()
