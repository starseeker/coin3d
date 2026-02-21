# Copyright (c) Kongsberg Oil & Gas Technologies AS
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# CMake script to run an image comparison test
cmake_policy(SET CMP0057 NEW)  # Enable IN_LIST operator in if()
#
# This script:
# 1. Runs the test executable to generate a test image (SGI RGB format)
# 2. Compares the test image against the control image (PNG)
# 3. Reports pass/fail based on the comparison result
#
# Required variables:
#   EXECUTABLE     - path to test executable
#   TEST_BASE      - base path for output (no extension); executable receives this as argv[1]
#   CONTROL_IMAGE  - path to control PNG image
#   COMPARATOR     - path to image_comparator executable
#
# Optional variables:
#   HASH_THRESHOLD - perceptual hash threshold (default 5, range 0-64)
#   RMSE_THRESHOLD - RMSE threshold (default 5.0, range 0-255)
#   TEST_TIMEOUT   - execution timeout in seconds (default 30)
#   COIN_DATA_DIR  - directory containing Coin data files

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

# On Unix systems with no display available, wrap with xvfb-run for GLX backend
set(_exec_cmd "${EXECUTABLE}" "${TEST_BASE}")
if(UNIX AND NOT APPLE)
    if(NOT DEFINED ENV{DISPLAY} OR "$ENV{DISPLAY}" STREQUAL "")
        find_program(_xvfb_run NAMES xvfb-run)
        if(_xvfb_run)
            set(_exec_cmd "${_xvfb_run}" "--auto-servernum" "--server-args=-screen 0 1024x768x24 +extension GLX" "${EXECUTABLE}" "${TEST_BASE}")
        endif()
    endif()
    set(ENV{COIN_GLX_PIXMAP_DIRECT_RENDERING} "1")
endif()

# Set data directory for tests that load .iv files
if(DEFINED COIN_DATA_DIR)
    set(ENV{COIN_DATA_DIR} "${COIN_DATA_DIR}")
endif()

# Run the test executable
message("Running test: ${EXECUTABLE}")
execute_process(
    COMMAND ${_exec_cmd}
    RESULT_VARIABLE EXEC_RESULT
    OUTPUT_VARIABLE EXEC_OUTPUT
    ERROR_VARIABLE EXEC_ERROR
    TIMEOUT ${TEST_TIMEOUT}
    WORKING_DIRECTORY "${TEST_DIR}"
)

if(NOT EXEC_RESULT EQUAL 0)
    message(FATAL_ERROR "Test execution failed (code ${EXEC_RESULT})\nOutput: ${EXEC_OUTPUT}\nError: ${EXEC_ERROR}")
endif()

# Find the generated test image
set(TEST_IMAGE "")
if(EXISTS "${TEST_BASE}.rgb")
    set(TEST_IMAGE "${TEST_BASE}.rgb")
elseif(EXISTS "${TEST_BASE}")
    set(TEST_IMAGE "${TEST_BASE}")
else()
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
    message("TEST PASSED")
elseif(COMPARE_RESULT EQUAL 1)
    message(FATAL_ERROR "TEST FAILED: ${TEST_IMAGE} differs from ${CONTROL_IMAGE}")
else()
    message(FATAL_ERROR "TEST ERROR: Comparator failed with code ${COMPARE_RESULT}")
endif()
