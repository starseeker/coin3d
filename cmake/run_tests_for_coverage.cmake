# run_tests_for_coverage.cmake
#
# Helper script invoked by the 'coverage' custom target.
# Runs ctest and intentionally ignores its exit code so that pre-existing
# test failures (e.g. Mentor examples that require a display) do not prevent
# lcov from collecting coverage data from the tests that did pass.
#
# Variables passed in via -D:
#   CTEST_COMMAND   - full path to ctest executable
#   BUILD_CONFIG    - build configuration (Debug, Release, …)
#   TEST_TIMEOUT    - per-test timeout in seconds

execute_process(
    COMMAND ${CTEST_COMMAND}
        --build-config "${BUILD_CONFIG}"
        --output-on-failure
        --timeout "${TEST_TIMEOUT}"
    RESULT_VARIABLE ctest_result
)

if(NOT ctest_result EQUAL 0)
    message(STATUS
        "ctest finished with result ${ctest_result} "
        "(some tests may have failed – continuing with coverage collection)")
endif()
