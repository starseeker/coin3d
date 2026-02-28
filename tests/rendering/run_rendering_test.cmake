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

# Optional: test name for the consolidated obol_test invocation.
# When TEST_NAME is set, the executable is obol_test and receives:
#   obol_test render_test <TEST_NAME>
# When TEST_NAME is not set (legacy / NanoRT standalone), the executable is
# called with no arguments (it runs its own main()).
if(DEFINED TEST_NAME AND NOT TEST_NAME STREQUAL "")
    set(_exec_args "${EXECUTABLE}" "render_test" "${TEST_NAME}")
else()
    set(_exec_args "${EXECUTABLE}")
endif()
set(_exec_cmd ${_exec_args})
if(UNIX AND NOT APPLE)
    # Wrap with xvfb-run when no display is available.
    # Do NOT set OBOL_FULL_INDIRECT_RENDERING: it disables FBO support in
    # SoGLContext_has_framebuffer_objects() which breaks CoinOffscreenGLCanvas.
    if(NOT DEFINED ENV{DISPLAY} OR "$ENV{DISPLAY}" STREQUAL "")
        find_program(_xvfb_run NAMES xvfb-run)
        if(_xvfb_run)
            set(_exec_cmd
                "${_xvfb_run}"
                "--auto-servernum"
                "--server-args=-screen 0 1024x768x24 +extension GLX"
                ${_exec_args})
        endif()
    endif()
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
