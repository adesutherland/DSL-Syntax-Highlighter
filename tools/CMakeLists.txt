add_executable(parser_tester parser_tester.c)
add_executable(DSLSH::parser_tester ALIAS parser_tester)
target_link_libraries(parser_tester PRIVATE DSLSH::editor)

if(WIN32)
    target_link_libraries(parser_tester PRIVATE ws2_32)
endif()

if(DSLSH_INSTALL)
    install(TARGETS parser_tester DESTINATION bin)
endif()
