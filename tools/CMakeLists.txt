add_executable(parser_tester parser_tester.c)
add_executable(DSLSH::parser_tester ALIAS parser_tester)
target_link_libraries(parser_tester PRIVATE DSLSH::editor)
dslsh_set_install_rpath(parser_tester)

if(WIN32)
    target_link_libraries(parser_tester PRIVATE ws2_32)
endif()

if(DSLSH_INSTALL)
    install(TARGETS parser_tester DESTINATION bin)
endif()
