cmake_minimum_required(VERSION 3.24)

# Existing native host tools only. This project never fetches or builds CREXX.
set(CREXX_BIN_DIR "" CACHE PATH "Existing CREXX tools and matching Level B/L bytecode")
set(CREXX_SOURCE_DIR "" CACHE PATH "Optional CREXX source checkout owning the PHP example grammar")
set(CREXX_CLASSIC_BUILD_DIR "" CACHE PATH "Optional prepared CREXX research build owning the Classic parser artifacts")
if(CREXX_BIN_DIR STREQUAL "")
  message(FATAL_ERROR "DSLSH_BUILD_CREXX requires explicit CREXX_BIN_DIR")
endif()
if(CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "The optional cREXX component currently requires a native host build")
endif()
get_filename_component(CREXX_BIN_DIR "${CREXX_BIN_DIR}" ABSOLUTE)
set(_dslsh_suffix "")
if(CMAKE_HOST_WIN32)
  set(_dslsh_suffix ".exe")
endif()
foreach(tool IN ITEMS rxc rxlang rxas rxlink rxbvm)
  set(_dslsh_${tool} "${CREXX_BIN_DIR}/${tool}${_dslsh_suffix}")
  if(NOT EXISTS "${_dslsh_${tool}}" OR IS_DIRECTORY "${_dslsh_${tool}}")
    message(FATAL_ERROR "Missing existing CREXX tool: ${_dslsh_${tool}}")
  endif()
endforeach()
foreach(image IN ITEMS library rxfnsl)
  if(NOT EXISTS "${CREXX_BIN_DIR}/${image}.rxbin" OR IS_DIRECTORY "${CREXX_BIN_DIR}/${image}.rxbin")
    message(FATAL_ERROR "Missing matching CREXX SDK image: ${CREXX_BIN_DIR}/${image}.rxbin")
  endif()
endforeach()

set(_dslsh_modules "${CMAKE_CURRENT_BINARY_DIR}/modules")
set(_dslsh_programs "${CMAKE_CURRENT_BINARY_DIR}/programs")
set(_dslsh_work "${CMAKE_CURRENT_BINARY_DIR}/work")
file(MAKE_DIRECTORY "${_dslsh_modules}" "${_dslsh_programs}" "${_dslsh_work}")
set(_dslsh_library "${CREXX_BIN_DIR}/library.rxbin")
set(_dslsh_sdk "${CREXX_BIN_DIR}/rxfnsl.rxbin")

function(dslsh_crexx_module name source needs_sdk)
  set(dependencies "${_dslsh_library}")
  if(needs_sdk)
    list(APPEND dependencies "${_dslsh_sdk}")
  endif()
  foreach(module IN LISTS ARGN)
    list(APPEND dependencies "${_dslsh_modules}/${module}.rxbin")
  endforeach()
  set(work "${_dslsh_work}/${name}")
  set(imports "${work}/imports")
  set(base "${work}/${name}")
  set(product "${_dslsh_modules}/${name}.rxbin")
  add_custom_command(OUTPUT "${product}" BYPRODUCTS "${base}.rxas"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${imports}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${imports}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${dependencies} "${imports}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${base}.crexx"
    COMMAND "${_dslsh_rxc}" -x --no-exe-import
      -i "${imports}" -o "${base}" "${base}.crexx"
    COMMAND "${_dslsh_rxas}" "${base}"
    COMMAND "${CMAKE_COMMAND}" -E rename "${base}.rxbin" "${product}"
    DEPENDS "${source}" ${dependencies}
      "${_dslsh_rxc}" "${_dslsh_rxas}"
    VERBATIM)
  add_custom_target(dslsh_crexx_${name} DEPENDS "${product}")
endfunction()

function(dslsh_crexx_program name source needs_sdk)
  set(modules "${_dslsh_library}")
  if(needs_sdk)
    list(APPEND modules "${_dslsh_sdk}")
  endif()
  foreach(module IN LISTS ARGN)
    list(APPEND modules "${_dslsh_modules}/${module}.rxbin")
  endforeach()
  set(work "${_dslsh_work}/${name}")
  set(imports "${work}/imports")
  set(base "${work}/${name}")
  set(product "${_dslsh_programs}/${name}.rxbin")
  add_custom_command(OUTPUT "${product}" BYPRODUCTS "${base}-main.rxas" "${base}-main.rxbin"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${imports}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${imports}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${modules} "${imports}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${base}.crexx"
    COMMAND "${_dslsh_rxc}" -x --no-exe-import
      -i "${imports}" -o "${base}-main" "${base}.crexx"
    COMMAND "${_dslsh_rxas}" "${base}-main"
    COMMAND "${_dslsh_rxlink}" -o "${base}-linked" "${base}-main.rxbin" ${modules}
    COMMAND "${CMAKE_COMMAND}" -E rename "${base}-linked.rxbin" "${product}"
    DEPENDS "${source}" ${modules}
      "${_dslsh_rxc}" "${_dslsh_rxas}" "${_dslsh_rxlink}"
    VERBATIM)
  add_custom_target(dslsh_crexx_${name} DEPENDS "${product}")
endfunction()

function(dslsh_crexx_grammar name source)
  set(work "${_dslsh_work}/${name}")
  set(base "${work}/${name}")
  set(product "${_dslsh_modules}/${name}.rxbin")
  add_custom_command(OUTPUT "${product}" BYPRODUCTS "${base}.rxas"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${work}"
    COMMAND "${CMAKE_COMMAND}" "-DRXLANG=${_dslsh_rxlang}" "-DSPEC=${source}"
      "-DOUTPUT=${base}.rxas" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateGrammar.cmake"
    COMMAND "${_dslsh_rxas}" "${base}"
    COMMAND "${CMAKE_COMMAND}" -E rename "${base}.rxbin" "${product}"
    DEPENDS "${source}" "${_dslsh_rxlang}" "${_dslsh_rxas}"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateGrammar.cmake"
    VERBATIM)
  add_custom_target(dslsh_crexx_${name} DEPENDS "${product}")
endfunction()

dslsh_crexx_module(dslshmodel "${CMAKE_CURRENT_SOURCE_DIR}/lib/dslshmodel.crexx" FALSE)
dslsh_crexx_module(dslshwire "${CMAKE_CURRENT_SOURCE_DIR}/lib/dslshwire.crexx" FALSE dslshmodel)
dslsh_crexx_module(dslshsession "${CMAKE_CURRENT_SOURCE_DIR}/lib/dslshsession.crexx" FALSE dslshmodel dslshwire)
dslsh_crexx_module(dslshstdio "${CMAKE_CURRENT_SOURCE_DIR}/lib/dslshstdio.crexx" FALSE dslshmodel dslshwire dslshsession)
set(_dslsh_core dslshmodel dslshwire dslshsession dslshstdio)
set(_dslsh_core_files)
foreach(module IN LISTS _dslsh_core)
  list(APPEND _dslsh_core_files "${_dslsh_modules}/${module}.rxbin")
endforeach()
add_custom_target(dslsh_crexx_core ALL DEPENDS ${_dslsh_core_files})

dslsh_crexx_module(dslshlevel "${CMAKE_CURRENT_SOURCE_DIR}/lib/dslshlevel.crexx" TRUE dslshmodel)
add_custom_target(dslsh_crexx_adapter ALL DEPENDS "${_dslsh_modules}/dslshlevel.rxbin")
dslsh_crexx_grammar(dslsh_settings "${CMAKE_CURRENT_SOURCE_DIR}/examples/settings.levell")
dslsh_crexx_program(dslsh-settings "${CMAKE_CURRENT_SOURCE_DIR}/examples/settings.crexx"
  TRUE ${_dslsh_core} dslshlevel dslsh_settings)
set(_dslsh_example_images "${_dslsh_programs}/dslsh-settings.rxbin")
set(_dslsh_php_image "")
if(NOT CREXX_SOURCE_DIR STREQUAL "")
  get_filename_component(CREXX_SOURCE_DIR "${CREXX_SOURCE_DIR}" ABSOLUTE)
  set(_dslsh_php_grammar "${CREXX_SOURCE_DIR}/examples/php/grammar/php.levell")
  if(NOT EXISTS "${_dslsh_php_grammar}")
    message(FATAL_ERROR "CREXX_SOURCE_DIR does not contain the maintained PHP example grammar")
  endif()
  dslsh_crexx_grammar(phpgrammar "${_dslsh_php_grammar}")
  dslsh_crexx_program(dslsh-php "${CMAKE_CURRENT_SOURCE_DIR}/examples/php.crexx"
    TRUE ${_dslsh_core} dslshlevel phpgrammar)
  set(_dslsh_php_image "${_dslsh_programs}/dslsh-php.rxbin")
  list(APPEND _dslsh_example_images "${_dslsh_php_image}")
endif()

# This optional example consumes only existing parser artifacts. Never recurse
# into the CREXX build or replace the maintained levelc.rxl grammar here.
set(_dslsh_classic_image "")
set(_dslsh_classic_modules)
set(_dslsh_classic_files)
if(NOT CREXX_CLASSIC_BUILD_DIR STREQUAL "")
  get_filename_component(CREXX_CLASSIC_BUILD_DIR "${CREXX_CLASSIC_BUILD_DIR}" ABSOLUTE)
  foreach(image IN ITEMS library rxfnsl)
    set(prepared "${CREXX_CLASSIC_BUILD_DIR}/bin/${image}.rxbin")
    if(NOT EXISTS "${prepared}" OR IS_DIRECTORY "${prepared}")
      message(FATAL_ERROR "Missing matching Classic SDK image: ${prepared}")
    endif()
    file(SHA256 "${prepared}" prepared_hash)
    file(SHA256 "${CREXX_BIN_DIR}/${image}.rxbin" selected_hash)
    if(NOT prepared_hash STREQUAL selected_hash)
      message(FATAL_ERROR "CREXX_BIN_DIR ${image}.rxbin does not match CREXX_CLASSIC_BUILD_DIR")
    endif()
  endforeach()
  set(_dslsh_classic_inputs
    "${CREXX_CLASSIC_BUILD_DIR}/research/level-l/compilers/frontends/llcgrammar.rxbin"
    "${CREXX_CLASSIC_BUILD_DIR}/research/level-l/compilers/levelc/opt/llclanguage.rxbin"
    "${CREXX_CLASSIC_BUILD_DIR}/research/level-l/compilers/levelc/opt/llcsrcmap.rxbin"
    "${CREXX_CLASSIC_BUILD_DIR}/research/level-l/compilers/levelc/opt/llcparse.rxbin"
    "${CREXX_CLASSIC_BUILD_DIR}/lib/rxfnsc/members/RexxClassicUnicodeProperties/RexxClassicUnicodeProperties.rxbin")
  foreach(input IN LISTS _dslsh_classic_inputs)
    if(NOT EXISTS "${input}" OR IS_DIRECTORY "${input}")
      message(FATAL_ERROR "Missing prepared Classic parser artifact: ${input}; prepare it in CREXX first")
    endif()
    get_filename_component(module "${input}" NAME_WE)
    list(APPEND _dslsh_classic_modules "${module}")
    list(APPEND _dslsh_classic_files "${_dslsh_modules}/${module}.rxbin")
    add_custom_command(OUTPUT "${_dslsh_modules}/${module}.rxbin"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${input}" "${_dslsh_modules}/${module}.rxbin"
      DEPENDS "${input}" VERBATIM)
  endforeach()
  dslsh_crexx_module(dslshclassic "${CMAKE_CURRENT_SOURCE_DIR}/lib/dslshclassic.crexx"
    TRUE dslshmodel dslshlevel ${_dslsh_classic_modules})
  dslsh_crexx_program(dslsh-classic "${CMAKE_CURRENT_SOURCE_DIR}/examples/classic.crexx"
    TRUE ${_dslsh_core} dslshlevel dslshclassic ${_dslsh_classic_modules})
  set(_dslsh_classic_image "${_dslsh_programs}/dslsh-classic.rxbin")
  list(APPEND _dslsh_example_images "${_dslsh_classic_image}")
endif()
add_custom_target(dslsh_crexx_examples ALL DEPENDS ${_dslsh_example_images})

if(DSLSH_BUILD_TESTS)
  dslsh_crexx_program(model-contracts "${CMAKE_CURRENT_SOURCE_DIR}/tests/model-contracts.crexx"
    FALSE dslshmodel)
  dslsh_crexx_program(wire-contracts "${CMAKE_CURRENT_SOURCE_DIR}/tests/wire-contracts.crexx"
    FALSE dslshmodel dslshwire)
  dslsh_crexx_program(session-contracts "${CMAKE_CURRENT_SOURCE_DIR}/tests/session-contracts.crexx"
    FALSE dslshmodel dslshwire dslshsession)
  dslsh_crexx_program(stdio-echo "${CMAKE_CURRENT_SOURCE_DIR}/tests/stdio-echo.crexx"
    FALSE ${_dslsh_core})
  dslsh_crexx_grammar(dslsh_fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/adapter-identifier.levell")
  dslsh_crexx_program(adapter-contracts "${CMAKE_CURRENT_SOURCE_DIR}/tests/adapter-contracts.crexx"
    TRUE dslshmodel dslshlevel dslsh_fixture)
  add_custom_target(dslsh_crexx_contracts ALL DEPENDS
    "${_dslsh_programs}/model-contracts.rxbin"
    "${_dslsh_programs}/wire-contracts.rxbin"
    "${_dslsh_programs}/session-contracts.rxbin"
    "${_dslsh_programs}/stdio-echo.rxbin"
    "${_dslsh_programs}/adapter-contracts.rxbin")
  if(NOT _dslsh_classic_image STREQUAL "")
    dslsh_crexx_program(classic-contracts "${CMAKE_CURRENT_SOURCE_DIR}/tests/classic-contracts.crexx"
      TRUE dslshmodel dslshlevel dslshclassic ${_dslsh_classic_modules})
    add_dependencies(dslsh_crexx_contracts dslsh_crexx_classic-contracts)
  endif()
  if(BUILD_TESTING)
    foreach(fixture IN ITEMS model wire session)
      string(TOUPPER "${fixture}" marker)
      add_test(NAME crexx_${fixture}_contracts
        COMMAND "${_dslsh_rxbvm}" --no-autoload
          "${_dslsh_programs}/${fixture}-contracts.rxbin" -a)
      set_tests_properties(crexx_${fixture}_contracts PROPERTIES TIMEOUT 60
        PASS_REGULAR_EXPRESSION "DSLSH_${marker}[|]checks=[0-9]+[|]failures=0"
        FAIL_REGULAR_EXPRESSION "FAIL[|]|PANIC:" LABELS "crexx;parser")
    endforeach()
    add_test(NAME crexx_adapter_contracts
      COMMAND "${_dslsh_rxbvm}" --no-autoload
        "${_dslsh_programs}/adapter-contracts.rxbin" -a)
    set_tests_properties(crexx_adapter_contracts PROPERTIES TIMEOUT 120
      PASS_REGULAR_EXPRESSION "DSLSH_ADAPTER[|]checks=[0-9]+[|]failures=0"
      FAIL_REGULAR_EXPRESSION "FAIL[|]|PANIC:" LABELS "crexx;parser")
    if(NOT _dslsh_classic_image STREQUAL "")
      add_test(NAME crexx_classic_contracts
        COMMAND "${_dslsh_rxbvm}" --no-autoload
          "${_dslsh_programs}/classic-contracts.rxbin" -a)
      set_tests_properties(crexx_classic_contracts PROPERTIES TIMEOUT 120
        PASS_REGULAR_EXPRESSION "DSLSH_CLASSIC[|]checks=[0-9]+[|]failures=0"
        FAIL_REGULAR_EXPRESSION "FAIL[|]|PANIC:" LABELS "crexx;parser;classic")
      add_test(NAME crexx_classic_stdio
        COMMAND "${CMAKE_COMMAND}" "-DVM=${_dslsh_rxbvm}"
          "-DIMAGE=${_dslsh_classic_image}"
          "-DSAMPLE=${CMAKE_CURRENT_SOURCE_DIR}/examples/classic.rexx"
          "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-work/classic"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-classic.cmake")
      set_tests_properties(crexx_classic_stdio PROPERTIES TIMEOUT 120
        PASS_REGULAR_EXPRESSION "DSLSH_CLASSIC_STDIO[|].*[|]failures=0" LABELS "crexx;parser;classic")
    endif()
    set(DSLSH_CREXX_PARSER_TESTER "" CACHE FILEPATH "Optional prepared C parser_tester for B/C interoperability")
    set(_dslsh_c_consumer)
    if(TARGET parser_tester)
      set(_dslsh_c_consumer "-DPARSER_TESTER=$<TARGET_FILE:parser_tester>")
      add_dependencies(dslsh_crexx_contracts parser_tester)
    elseif(NOT DSLSH_CREXX_PARSER_TESTER STREQUAL "")
      if(NOT EXISTS "${DSLSH_CREXX_PARSER_TESTER}" OR IS_DIRECTORY "${DSLSH_CREXX_PARSER_TESTER}")
        message(FATAL_ERROR "DSLSH_CREXX_PARSER_TESTER must name a prepared executable")
      endif()
      set(_dslsh_c_consumer "-DPARSER_TESTER=${DSLSH_CREXX_PARSER_TESTER}")
    endif()
    add_test(NAME crexx_stdio
      COMMAND "${CMAKE_COMMAND}" "-DVM=${_dslsh_rxbvm}"
        "-DIMAGE=${_dslsh_programs}/stdio-echo.rxbin"
        "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-work/stdio"
        ${_dslsh_c_consumer} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-stdio.cmake")
    set_tests_properties(crexx_stdio PROPERTIES TIMEOUT 60
      PASS_REGULAR_EXPRESSION "DSLSH_STDIO[|].*[|]failures=0" LABELS "crexx;parser")
    set(_dslsh_php_test)
    if(NOT _dslsh_php_image STREQUAL "")
      set(_dslsh_php_test "-DPHP_IMAGE=${_dslsh_php_image}")
    endif()
    add_test(NAME crexx_examples
      COMMAND "${CMAKE_COMMAND}" "-DVM=${_dslsh_rxbvm}"
        "-DSETTINGS_IMAGE=${_dslsh_programs}/dslsh-settings.rxbin"
        "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-work/examples"
        ${_dslsh_php_test} -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-examples.cmake")
    set_tests_properties(crexx_examples PROPERTIES TIMEOUT 120
      PASS_REGULAR_EXPRESSION "DSLSH_EXAMPLES[|].*[|]failures=0" LABELS "crexx;parser")
  endif()
endif()

if(DSLSH_INSTALL)
  install(FILES ${_dslsh_core_files} "${_dslsh_modules}/dslshlevel.rxbin"
    DESTINATION share/dslsyntax/crexx/lib)
  install(FILES ${_dslsh_example_images} DESTINATION bin)
  if(NOT _dslsh_classic_image STREQUAL "")
    install(FILES "${_dslsh_modules}/dslshclassic.rxbin" ${_dslsh_classic_files}
      DESTINATION share/dslsyntax/crexx/lib)
  endif()
  install(DIRECTORY lib/ DESTINATION share/dslsyntax/crexx/lib
    FILES_MATCHING PATTERN "*.crexx")
  install(FILES README.md DESTINATION share/dslsyntax/crexx)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../docs/CREXX_INTEGRATION.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/../docs/FUTURE_WORK.md"
    DESTINATION share/dslsyntax/docs)
  install(DIRECTORY examples/ DESTINATION share/dslsyntax/crexx/examples
    FILES_MATCHING PATTERN "*.crexx" PATTERN "*.levell" PATTERN "*.rexx")
endif()
