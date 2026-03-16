# =============================================================================
# Obol Test Suite
#
# Directory structure:
#   tests/
#   ├── CMakeLists.txt           this file
#   ├── test_utils.h/.cpp        SimpleTest framework (lightweight, no deps)
#   ├── run_image_test.cmake     CTest driver for visual regression tests
#   ├── generate_control_images.sh
#   ├── control_images/          reference PNG images for visual tests
#   ├── utils/
#   │   ├── image_comparator.cpp standalone image comparison tool
#   │   ├── rgb_to_png.c         SGI RGB -> PNG converter
#   │   ├── headless_utils.h     headless rendering helpers
#   │   ├── scene_graph_utils.h/.cpp  scene-graph test helpers
#   │   └── stb_image.h          public-domain PNG loader (fallback)
#   ├── testlib/                 libObolEx: shared scene catalog + test registry
#   │                            (used by both tests/ and examples/)
#   ├── rendering/               visual regression tests (OSMesa / GLX)
#   ├── tools/                   unit tests for utility subsystems
#   └── obol_test.cpp            unified CLI test runner
#
# Adding new tests
# ----------------
# Non-visual unit tests:
#   1. Add a REGISTER_TEST entry in tests/testlib/unit_test_wrappers.cpp
#      (see test_registry.h for the API).
#
# Visual regression tests:
#   1. Write a test executable in tests/rendering/ that renders a scene and
#      writes an SGI RGB file to argv[1]+".rgb".
#   2. Build it and run generate_control_images.sh to create the control PNG.
#   3. Call add_image_test() in this file to register it with CTest.
#
# =============================================================================

# -----------------------------------------------------------------------
# Common test utilities shared by all test executables
# -----------------------------------------------------------------------
add_library(simple_test_utils STATIC
    test_utils.cpp
    utils/scene_graph_utils.cpp
)
target_link_libraries(simple_test_utils ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
target_include_directories(simple_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
# headless_utils.h needs the OSMesa submodule headers for pure osmesa builds
if(OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
    target_include_directories(simple_test_utils PUBLIC
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
endif()

# -----------------------------------------------------------------------
# Image comparison thresholds (defined before subdirectories so that
# tests/rendering/ can reference them during the first configure pass)
# -----------------------------------------------------------------------
set(IMAGE_COMPARISON_HASH_THRESHOLD "5"   CACHE STRING "Perceptual hash threshold (0-64)")
set(IMAGE_COMPARISON_RMSE_THRESHOLD "5.0" CACHE STRING "RMSE threshold (0-255)")
set(IMAGE_COMPARISON_TEST_TIMEOUT   "30"  CACHE STRING "Per-test timeout in seconds")

# -----------------------------------------------------------------------
# Test library — libObolEx (shared registry, scene factories, unit-test wrappers)
# Must come before any target that links ObolEx / obol_testlib, but after
# simple_test_utils is defined above.
# -----------------------------------------------------------------------
add_subdirectory(testlib)

# -----------------------------------------------------------------------
# Unified CLI runner (legacy – kept for backward compatibility)
# -----------------------------------------------------------------------
add_executable(obol_test obol_test.cpp)
# --whole-archive ensures the static-initialiser REGISTER_TEST blocks in
# unit_test_wrappers.cpp (and test_scenes.cpp) are not discarded by the
# linker even though obol_test.cpp has no direct symbol references to them.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(obol_test
        -Wl,--whole-archive ObolEx -Wl,--no-whole-archive
        ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES}
    )
else()
    target_link_libraries(obol_test ObolEx ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
endif()
target_include_directories(obol_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(obol_test pthread)
endif()
add_test(NAME obol_test_all COMMAND obol_test)

# -----------------------------------------------------------------------
# Non-graphical unit test runner  (obol_unittest)
# Runs only tests with a run_unit function; no GL context required.
# -----------------------------------------------------------------------
add_executable(obol_unittest obol_unittest.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(obol_unittest PRIVATE
        -Wl,--whole-archive ObolEx -Wl,--no-whole-archive
        ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES}
    )
else()
    target_link_libraries(obol_unittest PRIVATE ObolEx ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
endif()
target_include_directories(obol_unittest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(obol_unittest PRIVATE pthread)
endif()
add_test(NAME obol_unittest_all COMMAND obol_unittest)

# -----------------------------------------------------------------------
# Visual regression tests (rendering/ subdirectory)
# These require standalone executables as each renders a scene to an image
# file that is then compared against a control PNG.
# -----------------------------------------------------------------------
add_subdirectory(rendering)

# -----------------------------------------------------------------------
# Tools unit tests (tools/ subdirectory)
# Unit tests for utility subsystems (SbModernUtils, etc.)
# -----------------------------------------------------------------------
add_subdirectory(tools)
add_subdirectory(threads)

# ---- Generate render test dispatch table shared by obol_test and obol_render --
# This file is written at configure time; it declares extern references to all
# renamed main() functions and provides dispatchRenderTest().
set(_dispatch "${CMAKE_CURRENT_BINARY_DIR}/render_test_dispatch.cpp")
set(_dc "// Auto-generated render test dispatch - DO NOT EDIT.\n")
string(APPEND _dc "#include <string>\n#include <cstring>\n\n")
foreach(_t ${OBOL_GL_RENDER_TESTS})
    string(APPEND _dc "extern int run_render_test_${_t}(int, char**);\n")
endforeach()
string(APPEND _dc "\nint dispatchRenderTest(const std::string& name, const std::string& basepath) {\n")
string(APPEND _dc "    static const struct { const char* name; int(*fn)(int, char**); } k[] = {\n")
foreach(_t ${OBOL_GL_RENDER_TESTS})
    string(APPEND _dc "        {\"${_t}\", run_render_test_${_t}},\n")
endforeach()
string(APPEND _dc "        {nullptr, nullptr}\n    };\n")
string(APPEND _dc "    for (int i = 0; k[i].name; ++i) {\n")
string(APPEND _dc "        if (name == k[i].name) {\n")
string(APPEND _dc "            char prog[] = {\"obol_render\"};\n")
string(APPEND _dc "            // PATH_MAX on Linux is 4096; reject longer paths to keep the\n")
string(APPEND _dc "            // fixed-size buffer safe.  Return -2 to distinguish from 'not\n")
string(APPEND _dc "            // found' (-1) vs 'path too long' (-2).\n")
string(APPEND _dc "            if (basepath.size() >= 4096) { return -2; }\n")
string(APPEND _dc "            if (basepath.empty()) {\n")
string(APPEND _dc "                // No basepath: call with argc=1 so the test uses its default output path\n")
string(APPEND _dc "                char* argv1[2] = {prog, nullptr};\n")
string(APPEND _dc "                return k[i].fn(1, argv1);\n")
string(APPEND _dc "            }\n")
string(APPEND _dc "            char base[4096];\n")
string(APPEND _dc "            strncpy(base, basepath.c_str(), sizeof(base)-1);\n")
string(APPEND _dc "            base[sizeof(base)-1] = '\\0';\n")
string(APPEND _dc "            // argv[2] == nullptr satisfies the POSIX argv[argc] == nullptr rule\n")
string(APPEND _dc "            char* argv[3] = {prog, base, nullptr};\n")
string(APPEND _dc "            return k[i].fn(2, argv);\n")
string(APPEND _dc "        }\n    }\n")
string(APPEND _dc "    return -1;\n}\n")
file(WRITE "${_dispatch}" "${_dc}")

# Link the render tests library + dispatch table into obol_test.
# The dispatch table's direct function references pull in all needed objects
# from obol_render_tests_gl without requiring --whole-archive.
target_sources(obol_test PRIVATE "${_dispatch}")
if(TARGET obol_render_tests_gl)
    target_link_libraries(obol_test obol_render_tests_gl)
endif()

# -----------------------------------------------------------------------
# Rendering test runner  (obol_render)
# Runs visual/rendering tests; produces control images or image sequences.
# Shares the same render test dispatch as obol_test.
# -----------------------------------------------------------------------
add_executable(obol_render obol_render.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(obol_render PRIVATE
        -Wl,--whole-archive ObolEx -Wl,--no-whole-archive
        ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES}
    )
else()
    target_link_libraries(obol_render PRIVATE ObolEx ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
endif()
target_include_directories(obol_render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(obol_render PRIVATE pthread)
endif()
# Share the dispatch table and render test library with obol_render
target_sources(obol_render PRIVATE "${_dispatch}")
if(TARGET obol_render_tests_gl)
    target_link_libraries(obol_render PRIVATE obol_render_tests_gl)
endif()
# On Linux the renderer includes headless_utils.h which uses GLX or OSMesa
if(UNIX AND NOT APPLE)
    if(X11_FOUND)
        target_link_libraries(obol_render PRIVATE ${X11_X11_LIB})
    endif()
    if(TARGET OpenGL::GL)
        target_link_libraries(obol_render PRIVATE OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
        target_link_libraries(obol_render PRIVATE ${OPENGL_gl_LIBRARY})
    endif()
endif()
if(OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
    target_compile_definitions(obol_render PRIVATE OBOL_SWRAST_BUILD)
    target_include_directories(obol_render PRIVATE
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
elseif(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
    target_compile_definitions(obol_render PRIVATE OBOL_SWRAST_BUILD OBOL_DUAL_GL_BUILD)
endif()
install(TARGETS obol_render RUNTIME DESTINATION bin)
install(TARGETS obol_unittest RUNTIME DESTINATION bin)

# -----------------------------------------------------------------------
# Image comparison utilities
# -----------------------------------------------------------------------

set(CONTROL_IMAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/control_images")

# image_comparator: standalone tool for comparing SGI RGB / PNG images
# Uses bundled lodepng for PNG support (no external dependencies required).
add_executable(image_comparator utils/image_comparator.cpp)
target_include_directories(image_comparator PRIVATE
    "${CMAKE_SOURCE_DIR}/external"
)
target_link_libraries(image_comparator lodepng)
set_target_properties(image_comparator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# rgb_to_png: convert SGI RGB files to PNG using lodepng
add_executable(rgb_to_png utils/rgb_to_png.cpp)
target_include_directories(rgb_to_png PRIVATE
    "${CMAKE_SOURCE_DIR}/external"
)
target_link_libraries(rgb_to_png lodepng)
set_target_properties(rgb_to_png PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# Output directory for test-generated images
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_output")

# -----------------------------------------------------------------------
# Macro: add_image_test
#
# Register a visual regression test with CTest.  The test executable must
# write an SGI RGB image to argv[1]+".rgb" (or argv[1] for direct-write).
# A matching control PNG must exist in control_images/.
#
# Usage:
#   add_image_test(test_name)
#   add_image_test(test_name RMSE_THRESHOLD 25.0)
#   add_image_test(test_name HASH_THRESHOLD 10 RMSE_THRESHOLD 25.0)
# -----------------------------------------------------------------------
macro(add_image_test test_name)
    set(CONTROL_IMAGE "${CONTROL_IMAGES_DIR}/${test_name}_control.png")
    set(TEST_BASE "${CMAKE_CURRENT_BINARY_DIR}/test_output/${test_name}_test")

    set(_hash_thresh ${IMAGE_COMPARISON_HASH_THRESHOLD})
    set(_rmse_thresh ${IMAGE_COMPARISON_RMSE_THRESHOLD})
    set(_next_is_hash FALSE)
    set(_next_is_rmse FALSE)
    foreach(_arg ${ARGN})
        if(_arg STREQUAL "HASH_THRESHOLD")
            set(_next_is_hash TRUE)
            set(_next_is_rmse FALSE)
        elseif(_arg STREQUAL "RMSE_THRESHOLD")
            set(_next_is_rmse TRUE)
            set(_next_is_hash FALSE)
        elseif(_next_is_hash)
            set(_hash_thresh ${_arg})
            set(_next_is_hash FALSE)
        elseif(_next_is_rmse)
            set(_rmse_thresh ${_arg})
            set(_next_is_rmse FALSE)
        endif()
    endforeach()

    if(EXISTS "${CONTROL_IMAGE}")
        add_test(
            NAME ${test_name}_image_test
            COMMAND ${CMAKE_COMMAND}
                -DEXECUTABLE=$<TARGET_FILE:${test_name}>
                -DTEST_BASE=${TEST_BASE}
                -DCONTROL_IMAGE=${CONTROL_IMAGE}
                -DCOMPARATOR=$<TARGET_FILE:image_comparator>
                -DHASH_THRESHOLD=${_hash_thresh}
                -DRMSE_THRESHOLD=${_rmse_thresh}
                -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_image_test.cmake
        )
        set_tests_properties(${test_name}_image_test PROPERTIES
            TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
        )
    endif()
endmacro()

# Register visual regression tests here as they are created:
#   add_image_test(test_basic_sphere)
#   add_image_test(test_material_colors)

# -----------------------------------------------------------------------
# Custom target: generate_controls
# Runs all rendering test executables and converts their output to control PNGs.
# Requires libpng (for rgb_to_png) and a display (or OSMesa).
# -----------------------------------------------------------------------
if(PNG_FOUND)
    add_custom_target(generate_controls
        COMMAND ${CMAKE_COMMAND} -E env
            BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            CONTROL_DIR=${CONTROL_IMAGES_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_control_images.sh
        DEPENDS image_comparator rgb_to_png
        COMMENT "Generating PNG control images for visual regression tests"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
