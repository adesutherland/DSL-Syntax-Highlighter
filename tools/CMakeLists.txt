add_executable(parser_tester parser_tester.c)
target_link_libraries(parser_tester PRIVATE dslsyntax_editor)
target_include_directories(parser_tester PRIVATE ../codebuffer)

if(WIN32)
    target_link_libraries(parser_tester PRIVATE ws2_32)
endif()

install(TARGETS parser_tester DESTINATION bin)
