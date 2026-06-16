include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DslshRpath.cmake")

add_executable(parser_tester parser_tester.c)
add_executable(DSLSH::parser_tester ALIAS parser_tester)
target_link_libraries(parser_tester PRIVATE DSLSH::editor)
dslsh_set_install_rpath(parser_tester)

if(WIN32)
    target_link_libraries(parser_tester PRIVATE ws2_32)
endif()

if(DSLSH_BUILD_TESTS AND TARGET tp)
    add_dependencies(parser_tester tp)
    file(GENERATE
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/parser_tester_reinit.txt"
            CONTENT "INIT \"$<TARGET_FILE:tp>\" ${DSL_Syntax_Highlighter_SOURCE_DIR}/toyparser/test.toy\nINIT \"$<TARGET_FILE:tp>\" ${DSL_Syntax_Highlighter_SOURCE_DIR}/toyparser/test2.toy\nSTATUS\nQUIT\n")
    add_test(NAME parser_tester_reinit
            COMMAND $<TARGET_FILE:parser_tester> -q "${CMAKE_CURRENT_BINARY_DIR}/parser_tester_reinit.txt")
    set_tests_properties(parser_tester_reinit PROPERTIES TIMEOUT 30)
endif()

if(DSLSH_INSTALL)
    install(TARGETS parser_tester DESTINATION bin)
endif()
