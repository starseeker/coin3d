# =============================================================================
# Coin3D Test Suite
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
#   ├── threads/                 threading API tests
#   ├── rendering/               visual regression tests (OSMesa / GLX)
#   ├── actions/                 SoAction sub-class tests (non-visual)
#   ├── base/                    SbXxx base-type tests
#   ├── engines/                 SoEngine tests
#   ├── fields/                  SoField tests
#   ├── io/                      I/O (SoDB read/write) tests
#   ├── nodes/                   SoNode tests
#   └── sensors/                 SoSensor tests
#
# Adding new tests
# ----------------
# Non-visual unit tests:
#   1. Create tests/<category>/test_<name>.cpp using the SimpleTest framework
#      (see test_utils.h for the API).
#   2. Either append to the SIMPLE_TESTS list below (for top-level tests) or
#      add a CMakeLists.txt in the category subdirectory and call
#      add_subdirectory() below.
#
# Visual regression tests:
#   1. Write a test executable that renders a scene and writes an SGI RGB
#      file to argv[1]+".rgb".
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

# -----------------------------------------------------------------------
# Existing top-level unit tests (SimpleTest framework)
# -----------------------------------------------------------------------
set(SIMPLE_TESTS
    test_actions
    test_nodes
    test_fields
    test_events
    test_base
    test_engines
    test_sensors
    test_scene_rendering
    test_gradient_debug
    test_simple_debug
    test_coordinate_debug
    test_sphere_positioning
    test_texture_checkerboard
    test_comprehensive
    test_legacy_integration
)

foreach(test_name ${SIMPLE_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${COIN_TARGET_INCLUDE_DIRECTORIES}
    )
    if(USE_PTHREAD)
        target_link_libraries(${test_name} pthread)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

# -----------------------------------------------------------------------
# Subdirectory tests
# -----------------------------------------------------------------------
add_subdirectory(threads)

# Placeholder subdirectories for future tests (add add_subdirectory() calls
# here as tests are written):
#   add_subdirectory(actions)
#   add_subdirectory(base)
#   add_subdirectory(engines)
#   add_subdirectory(fields)
#   add_subdirectory(io)
#   add_subdirectory(nodes)
#   add_subdirectory(rendering)
#   add_subdirectory(sensors)

# -----------------------------------------------------------------------
# Image comparison utilities
# -----------------------------------------------------------------------

# Image comparison thresholds (override with -D on cmake command line)
set(IMAGE_COMPARISON_HASH_THRESHOLD "5"   CACHE STRING "Perceptual hash threshold (0-64)")
set(IMAGE_COMPARISON_RMSE_THRESHOLD "5.0" CACHE STRING "RMSE threshold (0-255)")
set(IMAGE_COMPARISON_TEST_TIMEOUT   "30"  CACHE STRING "Per-test timeout in seconds")

set(CONTROL_IMAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/control_images")

# image_comparator: standalone tool for comparing SGI RGB / PNG images
find_package(PNG QUIET)
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

# -----------------------------------------------------------------------
# Compiler-warning suppression
# -----------------------------------------------------------------------
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare DISABLE_WARNING_TAUTOLOGCOMPARE)
if(DISABLE_WARNING_TAUTOLOGCOMPARE)
    foreach(test_name ${SIMPLE_TESTS})
        target_compile_options(${test_name} PUBLIC -Wno-tautological-compare)
    endforeach()
endif()
