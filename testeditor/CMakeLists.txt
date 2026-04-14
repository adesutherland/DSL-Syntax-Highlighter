cmake_minimum_required(VERSION 3.29)
project(testeditor C)

set(CMAKE_C_STANDARD 11)

add_executable(te editor.c)
target_include_directories(te PRIVATE "${DSL_Syntax_Highlighter_SOURCE_DIR}/codebuffer")
target_include_directories(te PRIVATE "${DSL_Syntax_Highlighter_SOURCE_DIR}/toyparser")
target_link_libraries(te PRIVATE dslsyntax_editor dslsyntax_parser highlighter)

if(WIN32)
  set(PDCURSESMOD_SOURCE_DIR
      "${DSL_Syntax_Highlighter_SOURCE_DIR}/third_party/PDCursesMod"
      CACHE PATH
      "Path to a vendored PDCursesMod checkout")

  if(NOT EXISTS "${PDCURSESMOD_SOURCE_DIR}/curses.h")
    message(FATAL_ERROR
      "Windows builds require a vendored PDCursesMod checkout at "
      "${PDCURSESMOD_SOURCE_DIR}. "
      "Download a release such as v4.5.4 and place it there.")
  endif()

  file(GLOB PDCURSESMOD_CORE_SOURCES CONFIGURE_DEPENDS
      "${PDCURSESMOD_SOURCE_DIR}/pdcurses/*.c")
  file(GLOB PDCURSESMOD_WINCON_SOURCES CONFIGURE_DEPENDS
      "${PDCURSESMOD_SOURCE_DIR}/wincon/*.c")

  add_library(pdcursesmod_wincon STATIC
      ${PDCURSESMOD_CORE_SOURCES}
      ${PDCURSESMOD_WINCON_SOURCES})

  target_include_directories(pdcursesmod_wincon PUBLIC
      "${PDCURSESMOD_SOURCE_DIR}"
      "${PDCURSESMOD_SOURCE_DIR}/pdcurses"
      "${PDCURSESMOD_SOURCE_DIR}/wincon")

  # Wide-character support plus forced UTF-8 best matches Unix terminal usage.
  target_compile_definitions(pdcursesmod_wincon PUBLIC
      PDC_WIDE
      PDC_FORCE_UTF8)

  target_link_libraries(pdcursesmod_wincon PUBLIC winmm)

  target_link_libraries(te PRIVATE pdcursesmod_wincon)
else()
  find_package(Curses REQUIRED)
  message(STATUS "Found curses library: ${CURSES_LIBRARIES}")
  target_include_directories(te PRIVATE ${CURSES_INCLUDE_DIRS})
  target_compile_options(te PRIVATE ${CURSES_CFLAGS})
  target_link_libraries(te PRIVATE ${CURSES_LIBRARIES})
endif()

# Install target
install(TARGETS te DESTINATION bin)
