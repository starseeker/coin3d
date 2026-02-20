# CMake script to run an image comparison test
cmake_policy(SET CMP0057 NEW)  # Enable IN_LIST operator in if()
#
# This script:
# 1. Runs the example executable to generate test images (SGI RGB format)
# 2. Compares the primary test image against the primary control image
# 3. For multi-frame/multi-view examples, also compares every generated
#    TEST_BASE_<suffix>.rgb against its matching <example>_<suffix>_control.png
#    (if the per-frame control image exists in the same directory)
# 4. Reports pass/fail based on all comparisons - any single frame mismatch
#    fails the test
#
# The executable receives TEST_BASE (no .rgb extension) as argv[1] so that:
#   - Single-output examples writing  argv[1]+".rgb"  produce TEST_BASE.rgb
#   - Multi-output examples writing   argv[1]+"_view.rgb" produce TEST_BASE_view.rgb
#   - Direct-write examples using argv[1] as full path produce TEST_BASE (no ext)
# After execution we search for the primary test image:
#   TEST_BASE.rgb  (most examples)
#   TEST_BASE      (direct-write examples like HelloCone)
#   TEST_BASE_*.rgb (first alphabetically, fallback for multi-view)

if(NOT DEFINED EXECUTABLE)
    message(FATAL_ERROR "EXECUTABLE not defined")
endif()
if(NOT DEFINED TEST_BASE)
    message(FATAL_ERROR "TEST_BASE not defined")
endif()
if(NOT DEFINED CONTROL_IMAGE)
    message(FATAL_ERROR "CONTROL_IMAGE not defined")
endif()
if(NOT DEFINED COMPARATOR)
    message(FATAL_ERROR "COMPARATOR not defined")
endif()
if(NOT DEFINED HASH_THRESHOLD)
    set(HASH_THRESHOLD 5)
endif()
if(NOT DEFINED RMSE_THRESHOLD)
    set(RMSE_THRESHOLD 5.0)
endif()
if(NOT DEFINED TEST_TIMEOUT)
    set(TEST_TIMEOUT 30)
endif()

# Create output directory
get_filename_component(TEST_DIR "${TEST_BASE}" DIRECTORY)
file(MAKE_DIRECTORY "${TEST_DIR}")

if(NOT EXISTS "${CONTROL_IMAGE}")
    message(FATAL_ERROR "Control image not found: ${CONTROL_IMAGE}")
endif()

# On Unix systems with no display available, wrap the executable with xvfb-run
# so that GLX offscreen rendering can create a software-rendering context.
set(_exec_cmd "${EXECUTABLE}" "${TEST_BASE}")
if(UNIX AND NOT APPLE)
    if(NOT DEFINED ENV{DISPLAY} OR "$ENV{DISPLAY}" STREQUAL "")
        find_program(_xvfb_run NAMES xvfb-run)
        if(_xvfb_run)
            set(_exec_cmd "${_xvfb_run}" "--auto-servernum" "--server-args=-screen 0 1024x768x24 +extension GLX" "${EXECUTABLE}" "${TEST_BASE}")
        endif()
    endif()
    # Enable full indirect rendering for Xvfb compatibility
    # Enable direct rendering for GLX pixmaps (required on modern X servers)
    set(ENV{COIN_FULL_INDIRECT_RENDERING} "1")
    set(ENV{COIN_GLX_PIXMAP_DIRECT_RENDERING} "1")
endif()

# Run the example with the base name as argv[1]
message("Running example: ${EXECUTABLE}")
execute_process(
    COMMAND ${_exec_cmd}
    RESULT_VARIABLE EXEC_RESULT
    OUTPUT_VARIABLE EXEC_OUTPUT
    ERROR_VARIABLE EXEC_ERROR
    TIMEOUT ${TEST_TIMEOUT}
    WORKING_DIRECTORY "${TEST_DIR}"
)

if(NOT EXEC_RESULT EQUAL 0)
    message(FATAL_ERROR "Example execution failed (code ${EXEC_RESULT})\nOutput: ${EXEC_OUTPUT}\nError: ${EXEC_ERROR}")
endif()

# Find the generated test image:
#  1. TEST_BASE.rgb   (append-.rgb examples: 09.1.Print, 11.2.ReadString, etc.)
#  2. TEST_BASE       (direct-write examples: 02.1.HelloCone)
#  3. First TEST_BASE_*.rgb (multi-view examples: fallback)
set(TEST_IMAGE "")
get_filename_component(TEST_BASENAME "${TEST_BASE}" NAME)
get_filename_component(TEST_DIR_ABS "${TEST_BASE}" DIRECTORY)

if(EXISTS "${TEST_BASE}.rgb")
    set(TEST_IMAGE "${TEST_BASE}.rgb")
elseif(EXISTS "${TEST_BASE}")
    set(TEST_IMAGE "${TEST_BASE}")
else()
    # Look for TEST_BASE_*.rgb
    file(GLOB _candidates "${TEST_BASE}_*.rgb")
    if(_candidates)
        list(SORT _candidates)
        list(GET _candidates 0 TEST_IMAGE)
    endif()
endif()

if(NOT TEST_IMAGE OR NOT EXISTS "${TEST_IMAGE}")
    message(FATAL_ERROR "Test image was not generated (looked for ${TEST_BASE}.rgb, ${TEST_BASE}, ${TEST_BASE}_*.rgb)")
endif()

# Run image comparator
message("Comparing images...")
message("  Control: ${CONTROL_IMAGE}")
message("  Test:    ${TEST_IMAGE}")
message("  Thresholds: hash=${HASH_THRESHOLD}, rmse=${RMSE_THRESHOLD}")

set(_any_frame_failed FALSE)

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

if(COMPARE_OUTPUT)
    message("${COMPARE_OUTPUT}")
endif()
if(COMPARE_ERROR)
    message("${COMPARE_ERROR}")
endif()

if(COMPARE_RESULT EQUAL 0)
    message("PASS: ${TEST_IMAGE}")
elseif(COMPARE_RESULT EQUAL 1)
    message(SEND_ERROR "FAIL: ${TEST_IMAGE} differs from ${CONTROL_IMAGE}")
    set(_any_frame_failed TRUE)
else()
    message(FATAL_ERROR "TEST ERROR: Comparator failed with code ${COMPARE_RESULT}")
endif()

# -----------------------------------------------------------------------
# Per-frame comparisons for multi-frame/multi-view examples
#
# Derive the control images directory and example name from CONTROL_IMAGE:
#   CONTROL_IMAGE = <dir>/<example>_control.png
# then for each TEST_BASE_<suffix>.rgb, look for <dir>/<example>_<suffix>_control.png
# -----------------------------------------------------------------------
get_filename_component(CONTROL_DIR "${CONTROL_IMAGE}" DIRECTORY)
get_filename_component(_ctrl_name_we "${CONTROL_IMAGE}" NAME_WE)
# Strip trailing "_control" to recover the bare example name
string(REGEX REPLACE "_control$" "" EXAMPLE_NAME "${_ctrl_name_we}")

# Collect all additional view/frame images generated by the example
file(GLOB _all_generated "${TEST_BASE}_*.rgb")
list(SORT _all_generated)

# Also collect any sub-directory outputs (e.g. output/<name>_*.rgb written by 17.2.GLCallback)
file(GLOB_RECURSE _subdir_generated "${TEST_DIR}/*.rgb")
foreach(_sg ${_subdir_generated})
    # Only add if it matches TEST_BASE prefix or output/ subdir pattern
    get_filename_component(_sg_name "${_sg}" NAME_WE)
    string(FIND "${_sg_name}" "${EXAMPLE_NAME}" _sg_pos)
    if(_sg_pos EQUAL 0 AND NOT "${_sg}" IN_LIST _all_generated AND NOT "${_sg}" STREQUAL "${TEST_IMAGE}")
        list(APPEND _all_generated "${_sg}")
    endif()
endforeach()

set(_frame_count 0)
foreach(_frame_img ${_all_generated})
    # Compute the view suffix by stripping TEST_BASE from the generated file path (no ext)
    get_filename_component(_frame_name_we "${_frame_img}" NAME_WE)
    # _frame_name_we is like "10.1.addEventCB_test_frame00_initial"
    # TEST_BASENAME is like "10.1.addEventCB_test"
    string(REPLACE "${TEST_BASENAME}" "" _frame_suffix "${_frame_name_we}")
    # _frame_suffix is now "_frame00_initial"

    # Build path to corresponding control image
    set(_frame_ctrl "${CONTROL_DIR}/${EXAMPLE_NAME}${_frame_suffix}_control.png")

    if(NOT EXISTS "${_frame_ctrl}")
        # No control image for this frame/view - skip silently
        continue()
    endif()

    # Skip if this is the same image already checked as the primary
    if("${_frame_img}" STREQUAL "${TEST_IMAGE}" AND "${_frame_ctrl}" STREQUAL "${CONTROL_IMAGE}")
        continue()
    endif()

    message("Comparing frame: ${_frame_suffix}")
    message("  Control: ${_frame_ctrl}")
    message("  Test:    ${_frame_img}")

    execute_process(
        COMMAND "${COMPARATOR}"
                "--threshold" "${HASH_THRESHOLD}"
                "--rmse" "${RMSE_THRESHOLD}"
                "--verbose"
                "${_frame_ctrl}"
                "${_frame_img}"
        RESULT_VARIABLE _frame_result
        OUTPUT_VARIABLE _frame_output
        ERROR_VARIABLE  _frame_error
    )

    if(_frame_output)
        message("${_frame_output}")
    endif()
    if(_frame_error)
        message("${_frame_error}")
    endif()

    if(_frame_result EQUAL 0)
        message("PASS: ${_frame_img}")
        math(EXPR _frame_count "${_frame_count} + 1")
    elseif(_frame_result EQUAL 1)
        message(SEND_ERROR "FAIL: ${_frame_img} differs from ${_frame_ctrl}")
        set(_any_frame_failed TRUE)
    else()
        message(FATAL_ERROR "TEST ERROR: Comparator failed (code ${_frame_result}) for ${_frame_img}")
    endif()
endforeach()

if(_frame_count GREATER 0)
    message("Compared ${_frame_count} additional frame(s)")
endif()

if(_any_frame_failed)
    message(FATAL_ERROR "TEST FAILED: one or more frames differ beyond threshold")
else()
    message("TEST PASSED: all images match within threshold")
endif()
