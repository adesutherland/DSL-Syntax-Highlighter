cmake_minimum_required(VERSION 3.29)
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

set(BUILD_TOYEDITOR OFF)

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
  add_subdirectory(toyeditor)
endif()

add_subdirectory(toyparser)
add_subdirectory(codebuffer)
add_subdirectory(tools)

install(FILES docs/PARSER_TESTER.md DESTINATION share/doc/dslsyntax)
