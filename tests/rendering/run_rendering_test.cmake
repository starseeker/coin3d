# run_rendering_test.cmake
#
# CMake script to run a rendering-test executable that performs its own
# internal pass/fail check (i.e. returns 0 on success, non-zero on failure).
#
# On Linux without a DISPLAY, the executable is automatically wrapped with
# xvfb-run so that GLX-backend builds work headlessly.
#
# Required variables:
#   EXECUTABLE    - path to the test executable
#
# Optional variables:
#   TEST_TIMEOUT  - execution timeout in seconds (default 30)

if(NOT DEFINED EXECUTABLE)
    message(FATAL_ERROR "EXECUTABLE not defined")
endif()
if(NOT DEFINED TEST_TIMEOUT)
    set(TEST_TIMEOUT 30)
endif()

# Build the command, wrapping with xvfb-run when no display is available
set(_exec_cmd "${EXECUTABLE}")
if(UNIX AND NOT APPLE)
    if(NOT DEFINED ENV{DISPLAY} OR "$ENV{DISPLAY}" STREQUAL "")
        find_program(_xvfb_run NAMES xvfb-run)
        if(_xvfb_run)
            set(_exec_cmd
                "${_xvfb_run}"
                "--auto-servernum"
                "--server-args=-screen 0 1024x768x24 +extension GLX"
                "${EXECUTABLE}")
        endif()
    endif()
    # Standard GLX-headless environment hints
    set(ENV{COIN_GLX_PIXMAP_DIRECT_RENDERING} "1")
    set(ENV{COIN_FULL_INDIRECT_RENDERING}      "1")
    set(ENV{COIN_GLXGLUE_NO_PBUFFERS}          "1")
endif()

message("Running: ${EXECUTABLE}")
execute_process(
    COMMAND ${_exec_cmd}
    RESULT_VARIABLE EXEC_RESULT
    OUTPUT_VARIABLE EXEC_OUTPUT
    ERROR_VARIABLE  EXEC_ERROR
    TIMEOUT         ${TEST_TIMEOUT}
)

if(EXEC_OUTPUT)
    message("${EXEC_OUTPUT}")
endif()
if(EXEC_ERROR)
    message("${EXEC_ERROR}")
endif()

if(NOT EXEC_RESULT EQUAL 0)
    message(FATAL_ERROR "Test FAILED (exit code ${EXEC_RESULT})")
endif()
message("TEST PASSED")
