# MIT licence. Focused framed endpoint and one real C parser_tester consumer.
cmake_minimum_required(VERSION 3.14)

foreach(required VM IMAGE WORK_ROOT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing ${required}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${WORK_ROOT}")
set(request_file "${WORK_ROOT}/stdio-request.txt")
set(source_file "${WORK_ROOT}/stdio-source.txt")
set(script_file "${WORK_ROOT}/parser-tester.txt")

function(add_frame variable payload)
  string(LENGTH "${payload}" length)
  math(EXPR hex "${length}" OUTPUT_FORMAT HEXADECIMAL)
  string(SUBSTRING "${hex}" 2 -1 hex)
  string(LENGTH "${hex}" digits)
  math(EXPR zeros "8 - ${digits}")
  if(zeros LESS 0)
    message(FATAL_ERROR "Test request too large")
  endif()
  string(REPEAT "0" ${zeros} padding)
  set(${variable} "${${variable}}${padding}${hex}${payload}" PARENT_SCOPE)
endfunction()

function(take_frame buffer_name frame_name)
  string(LENGTH "${${buffer_name}}" available)
  if(available LESS 8)
    message(FATAL_ERROR "Short response header")
  endif()
  string(SUBSTRING "${${buffer_name}}" 0 8 hex)
  if(NOT hex MATCHES "^[0-9a-fA-F]+$")
    message(FATAL_ERROR "Invalid response header: ${hex}")
  endif()
  math(EXPR length "0x${hex}")
  math(EXPR needed "8 + ${length}")
  if(available LESS needed)
    message(FATAL_ERROR "Short response payload")
  endif()
  string(SUBSTRING "${${buffer_name}}" 8 ${length} extracted)
  string(SUBSTRING "${${buffer_name}}" ${needed} -1 tail)
  set(${buffer_name} "${tail}" PARENT_SCOPE)
  set(${frame_name} "${extracted}" PARENT_SCOPE)
endfunction()

set(request "")
add_frame(request "C|EP")
add_frame(request "I|1|1|41|61620a|")
add_frame(request "D|2|41|1|2||1|c|0|1|0|5a")
add_frame(request "D|2|41|1|3||1|c|0|1|0|5a")
file(WRITE "${request_file}" "${request}")
execute_process(
  COMMAND "${VM}" --no-autoload "${IMAGE}"
  INPUT_FILE "${request_file}"
  OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE status
  TIMEOUT 8)
if(NOT status STREQUAL "0")
  string(SUBSTRING "${errors}" 0 512 excerpt)
  message(FATAL_ERROR "Framed endpoint exited ${status}: ${excerpt}")
endif()
take_frame(output first)
take_frame(output second)
take_frame(output third)
take_frame(output fourth)
if(NOT first STREQUAL "C|" OR NOT second MATCHES "^4\\|" OR
   NOT third MATCHES "^4\\|" OR NOT fourth STREQUAL "0" OR
   NOT output STREQUAL "")
  message(FATAL_ERROR "Wrong config/initial/delta/refusal frame sequence")
endif()
string(LENGTH "${errors}" error_length)
if(NOT errors MATCHES "DSLSH_REVISION: stale/nonconsecutive delta" OR
   error_length GREATER 128)
  message(FATAL_ERROR "Wrong or padded refusal diagnostic")
endif()

foreach(case IN ITEMS truncated invalid_header)
  if(case STREQUAL "truncated")
    file(WRITE "${request_file}" "00000008C|EP")
    set(expected "DSLSH_FRAME: truncated payload")
  else()
    file(WRITE "${request_file}" "GGGGGGGG")
    set(expected "DSLSH_FRAME: invalid or over-budget header")
  endif()
  execute_process(
    COMMAND "${VM}" --no-autoload "${IMAGE}"
    INPUT_FILE "${request_file}"
    OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE status
    TIMEOUT 8)
  string(LENGTH "${errors}" error_length)
  if(NOT status STREQUAL "2" OR NOT output STREQUAL "" OR
     NOT errors MATCHES "${expected}" OR error_length GREATER 128)
    message(FATAL_ERROR "${case} did not fail cleanly: rc=${status}")
  endif()
endforeach()

# The C client adds a logical LF to the one-line file. Its reconstructed root
# and leaf must cover those three codepoints and retain printable severity 0.
set(consumer_count 0)
if(DEFINED PARSER_TESTER AND NOT PARSER_TESTER STREQUAL "")
  file(WRITE "${source_file}" "ab")
  file(WRITE "${script_file}"
    "INIT \"${VM} --no-autoload ${IMAGE} -a\" ${source_file}\nDUMP_AST\nINSERT 0 1 Z\nSYNC\nDUMP_AST\nQUIT\n")
  execute_process(
    COMMAND "${PARSER_TESTER}" -q "${script_file}"
    OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE status
    TIMEOUT 15)
  if(NOT status STREQUAL "0" OR
     NOT output MATCHES "Node: type=71 \\(PARSE_TREE_FILE\\), pos=0, len=3, sev=48" OR
     NOT output MATCHES "Node: type=35 \\(LEXER_TOKEN\\), pos=0, len=3, sev=48" OR
     NOT output MATCHES "Node: type=71 \\(PARSE_TREE_FILE\\), pos=0, len=4, sev=48" OR
     NOT output MATCHES "Node: type=35 \\(LEXER_TOKEN\\), pos=0, len=4, sev=48" OR
     NOT output MATCHES "aZb" OR
     output MATCHES "No Parse Tree")
    string(SUBSTRING "${output}" 0 512 output_excerpt)
    string(SUBSTRING "${errors}" 0 512 error_excerpt)
    message(FATAL_ERROR "C parser_tester did not reconstruct the B result: rc=${status}; output=${output_excerpt}; errors=${error_excerpt}")
  endif()
  set(consumer_count 1)
endif()

file(REMOVE "${request_file}" "${source_file}" "${script_file}")
message(STATUS "DSLSH_STDIO|framing=3|C_consumer_initial_and_delta=${consumer_count}|failures=0")
