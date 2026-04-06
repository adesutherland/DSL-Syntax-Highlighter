cmake_minimum_required(VERSION 3.29)
project(toyeditor C)

set(CMAKE_C_STANDARD 11)

# Find the Curses package
find_package(Curses REQUIRED)
message(STATUS "Found version of ncurses: ${CURSES_LIBRARY}")

add_executable(te editor.c)
target_include_directories(te PRIVATE "${CURSES_INCLUDE_DIR}")
target_include_directories(te PRIVATE "${DSL_Syntax_Highlighter_SOURCE_DIR}/codebuffer")
target_include_directories(te PRIVATE "${DSL_Syntax_Highlighter_SOURCE_DIR}/toyparser")
target_link_libraries(te ${CURSES_LIBRARY})
target_link_libraries(te dslsyntax_editor)
target_link_libraries(te dslsyntax_parser)
target_link_libraries(te highlighter)
