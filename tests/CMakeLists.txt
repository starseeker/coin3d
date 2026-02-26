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
#   ├── testlib/                 unified test registry (obol_testlib)
#   ├── rendering/               visual regression tests (OSMesa / GLX)
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
target_link_libraries(simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(simple_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
# headless_utils.h needs the OSMesa submodule headers for pure osmesa builds
if(COIN3D_USE_OSMESA)
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
# Test library (shared registry, scene factories, unit-test wrappers)
# Must come before any target that links obol_testlib, but after
# simple_test_utils is defined above.
# -----------------------------------------------------------------------
add_subdirectory(testlib)

# -----------------------------------------------------------------------
# Unified CLI runner
# -----------------------------------------------------------------------
add_executable(obol_test obol_test.cpp)
# --whole-archive ensures the static-initialiser REGISTER_TEST blocks in
# unit_test_wrappers.cpp (and test_scenes.cpp) are not discarded by the
# linker even though obol_test.cpp has no direct symbol references to them.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(obol_test
        -Wl,--whole-archive obol_testlib -Wl,--no-whole-archive
        Coin ${COIN_TARGET_LINK_LIBRARIES}
    )
else()
    target_link_libraries(obol_test obol_testlib Coin ${COIN_TARGET_LINK_LIBRARIES})
endif()
target_include_directories(obol_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(obol_test pthread)
endif()
add_test(NAME obol_test_all COMMAND obol_test)

# -----------------------------------------------------------------------
# Visual regression tests (rendering/ subdirectory)
# These require standalone executables as each renders a scene to an image
# file that is then compared against a control PNG.
# -----------------------------------------------------------------------
add_subdirectory(rendering)

# -----------------------------------------------------------------------
# Image comparison utilities
# -----------------------------------------------------------------------

set(CONTROL_IMAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/control_images")

# image_comparator: standalone tool for comparing SGI RGB / PNG images
# Prefer the bundled external/libpng (built as png_static in the parent
# project) over any system libpng so that builds without libpng-dev still
# produce a fully-featured comparator.
if(TARGET png_static)
    set(PNG_FOUND TRUE)
    set(PNG_INCLUDE_DIRS
          "${CMAKE_SOURCE_DIR}/external/libpng"
          "${CMAKE_BINARY_DIR}/libpng_build")
    set(PNG_LIBRARIES png_static)
    message(STATUS "Tests: using bundled png_static for image tools")
else()
    find_package(PNG QUIET)
endif()
add_executable(image_comparator utils/image_comparator.cpp)
if(PNG_FOUND)
    target_compile_definitions(image_comparator PRIVATE HAVE_LIBPNG)
    target_include_directories(image_comparator PRIVATE ${PNG_INCLUDE_DIRS})
    target_link_libraries(image_comparator ${PNG_LIBRARIES})
else()
    target_include_directories(image_comparator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils)
endif()
set_target_properties(image_comparator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# rgb_to_png: convert SGI RGB files to PNG (requires libpng)
if(PNG_FOUND)
    add_executable(rgb_to_png utils/rgb_to_png.c)
    target_include_directories(rgb_to_png PRIVATE ${PNG_INCLUDE_DIRS})
    target_link_libraries(rgb_to_png ${PNG_LIBRARIES})
    set_target_properties(rgb_to_png PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endif()

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
