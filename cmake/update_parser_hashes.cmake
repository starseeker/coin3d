# update_parser_hashes.cmake
# Rewrites the EVALUATOR_Y_HASH and EVALUATOR_L_HASH values inside
# cmake/parser_hashes.cmake to match the current contents of evaluator.y and
# evaluator.l.  Called by the update_parser_sources custom target after
# regenerating the source-tree parser files.
#
# Required variables (passed via -D on the command line):
#   EVALUATOR_Y    - absolute path to evaluator.y
#   EVALUATOR_L    - absolute path to evaluator.l
#   HASHES_FILE    - absolute path to cmake/parser_hashes.cmake

foreach(_var EVALUATOR_Y EVALUATOR_L HASHES_FILE)
  if(NOT DEFINED "${_var}")
    message(FATAL_ERROR "update_parser_hashes.cmake: ${_var} must be defined")
  endif()
endforeach()

file(SHA256 "${EVALUATOR_Y}" _y_hash)
file(SHA256 "${EVALUATOR_L}" _l_hash)

file(READ "${HASHES_FILE}" _content)

string(REGEX REPLACE
  "(set\\(EVALUATOR_Y_HASH )[^)]*(\\))"
  "\\1\"${_y_hash}\"\\2"
  _content "${_content}"
)

string(REGEX REPLACE
  "(set\\(EVALUATOR_L_HASH )[^)]*(\\))"
  "\\1\"${_l_hash}\"\\2"
  _content "${_content}"
)

file(WRITE "${HASHES_FILE}" "${_content}")

message(STATUS "Updated ${HASHES_FILE}")
message(STATUS "  evaluator.y hash: ${_y_hash}")
message(STATUS "  evaluator.l hash: ${_l_hash}")
