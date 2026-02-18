# CMake script to run an image comparison test
#
# This script:
# 1. Runs the example executable to generate a test image
# 2. Compares the test image against the control image
# 3. Reports pass/fail based on comparison result

# Check that all required variables are set
if(NOT DEFINED EXECUTABLE)
    message(FATAL_ERROR "EXECUTABLE not defined")
endif()

if(NOT DEFINED TEST_IMAGE)
    message(FATAL_ERROR "TEST_IMAGE not defined")
endif()

if(NOT DEFINED CONTROL_IMAGE)
    message(FATAL_ERROR "CONTROL_IMAGE not defined")
endif()

if(NOT DEFINED COMPARATOR)
    message(FATAL_ERROR "COMPARATOR not defined")
endif()

if(NOT DEFINED HASH_THRESHOLD)
    set(HASH_THRESHOLD 5)  # Default from IMAGE_COMPARISON_HASH_THRESHOLD
endif()

if(NOT DEFINED RMSE_THRESHOLD)
    set(RMSE_THRESHOLD 5.0)  # Default from IMAGE_COMPARISON_RMSE_THRESHOLD
endif()

if(NOT DEFINED TEST_TIMEOUT)
    set(TEST_TIMEOUT 30)  # Default from IMAGE_COMPARISON_TEST_TIMEOUT
endif()

# Create output directory
get_filename_component(TEST_IMAGE_DIR "${TEST_IMAGE}" DIRECTORY)
file(MAKE_DIRECTORY "${TEST_IMAGE_DIR}")

# Check if control image exists
if(NOT EXISTS "${CONTROL_IMAGE}")
    message(FATAL_ERROR "Control image not found: ${CONTROL_IMAGE}")
endif()

# Run the example to generate test image
message("Running example: ${EXECUTABLE}")
execute_process(
    COMMAND "${EXECUTABLE}" "${TEST_IMAGE}"
    RESULT_VARIABLE EXEC_RESULT
    OUTPUT_VARIABLE EXEC_OUTPUT
    ERROR_VARIABLE EXEC_ERROR
    TIMEOUT ${TEST_TIMEOUT}
)

if(NOT EXEC_RESULT EQUAL 0)
    message(FATAL_ERROR "Example execution failed with code ${EXEC_RESULT}\nOutput: ${EXEC_OUTPUT}\nError: ${EXEC_ERROR}")
endif()

# Check if test image was generated
if(NOT EXISTS "${TEST_IMAGE}")
    message(FATAL_ERROR "Test image was not generated: ${TEST_IMAGE}")
endif()

# Run image comparator
message("Comparing images...")
message("  Control: ${CONTROL_IMAGE}")
message("  Test:    ${TEST_IMAGE}")
message("  Thresholds: hash=${HASH_THRESHOLD}, rmse=${RMSE_THRESHOLD}")

execute_process(
    COMMAND "${COMPARATOR}" 
            "--threshold" "${HASH_THRESHOLD}"
            "--rmse" "${RMSE_THRESHOLD}"
            "--verbose"
            "${CONTROL_IMAGE}"
            "${TEST_IMAGE}"
    RESULT_VARIABLE COMPARE_RESULT
    OUTPUT_VARIABLE COMPARE_OUTPUT
    ERROR_VARIABLE COMPARE_ERROR
)

# Print comparator output
if(COMPARE_OUTPUT)
    message("${COMPARE_OUTPUT}")
endif()
if(COMPARE_ERROR)
    message("${COMPARE_ERROR}")
endif()

# Check comparison result
if(COMPARE_RESULT EQUAL 0)
    message("TEST PASSED: Images match within threshold")
elseif(COMPARE_RESULT EQUAL 1)
    message(FATAL_ERROR "TEST FAILED: Images differ beyond threshold")
else()
    message(FATAL_ERROR "TEST ERROR: Comparator failed with code ${COMPARE_RESULT}")
endif()
