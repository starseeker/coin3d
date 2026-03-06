# check_grammar_hash.cmake
# Called at build time (via cmake -P) after bison/flex regenerate parser
# outputs.  Computes the SHA-256 of the grammar source file and compares it
# to the hash stored in cmake/parser_hashes.cmake.  A non-fatal warning is
# printed only when the hashes differ, indicating that the grammar has changed
# since the committed generated files were last produced.
#
# Required variables (passed via -D on the command line):
#   GRAMMAR_FILE   - absolute path to the .y or .l source file
#   STORED_HASH    - SHA-256 hash recorded in cmake/parser_hashes.cmake

if(NOT DEFINED GRAMMAR_FILE OR NOT DEFINED STORED_HASH)
  message(FATAL_ERROR
    "check_grammar_hash.cmake: GRAMMAR_FILE and STORED_HASH must both be defined")
endif()

if(NOT EXISTS "${GRAMMAR_FILE}")
  message(WARNING "check_grammar_hash.cmake: grammar file not found: ${GRAMMAR_FILE}")
  return()
endif()

file(SHA256 "${GRAMMAR_FILE}" _current_hash)

if(NOT "${_current_hash}" STREQUAL "${STORED_HASH}")
  message(WARNING
    "Grammar source has changed since the parser files were last committed:\n"
    "  File        : ${GRAMMAR_FILE}\n"
    "  Stored hash : ${STORED_HASH}\n"
    "  Current hash: ${_current_hash}\n"
    "From your build directory, run:\n"
    "  cmake --build . --target update_parser_sources\n"
    "to regenerate evaluator_tab.cpp / so_eval.ic in the source tree and\n"
    "record the updated hashes in cmake/parser_hashes.cmake."
  )
endif()
