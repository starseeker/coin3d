# =============================================================================
# Visual Regression Tests  (tests/rendering/)
#
# Each test executable renders a scene with SoOffscreenRenderer and writes
# an SGI RGB file.  CTest compares that output against a PNG control image
# stored in tests/control_images/.
#
# Control images are generated from vanilla (upstream) Coin + GLX and committed
# once to the repository.  On every subsequent run the same source is compiled
# against Obol; if the pixel output diverges beyond the configured thresholds
# the test fails, indicating a rendering regression.
#
# Backend selection (inherits from parent project):
#   COIN3D_USE_OSMESA=ON  → define COIN3D_OSMESA_BUILD, link OSMesa
#   default               → system OpenGL via GLX (needs X server / Xvfb)
# =============================================================================

# ---- Backend detection -------------------------------------------------------
if(DEFINED COIN3D_USE_OSMESA)
    set(RENDERING_USE_OSMESA ${COIN3D_USE_OSMESA})
else()
    set(RENDERING_USE_OSMESA OFF CACHE BOOL "Use OSMesa for rendering tests")
endif()

if(RENDERING_USE_OSMESA)
    set(OSMESA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/osmesa/include")
    if(TARGET osmesa_interface)
        set(OSMESA_LINK_TARGET osmesa_interface)
    elseif(TARGET osmesa)
        set(OSMESA_LINK_TARGET osmesa)
    endif()
    message(STATUS "Rendering tests: OSMesa backend")
else()
    message(STATUS "Rendering tests: GLX backend")
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
endif()

# ---- Build macro -------------------------------------------------------------
# Compile one rendering-test executable; links against Coin and the chosen
# OpenGL backend.  The headless_utils.h lives in tests/utils/.
macro(add_rendering_test name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${COIN_TARGET_INCLUDE_DIRECTORIES}
    )
    target_link_libraries(${name} Coin ${COIN_TARGET_LINK_LIBRARIES})
    if(RENDERING_USE_OSMESA)
        target_compile_definitions(${name} PRIVATE COIN3D_OSMESA_BUILD)
        if(OSMESA_INCLUDE_DIR)
            target_include_directories(${name} PRIVATE ${OSMESA_INCLUDE_DIR})
        endif()
        if(OSMESA_LINK_TARGET)
            target_link_libraries(${name} ${OSMESA_LINK_TARGET})
        endif()
    elseif(X11_FOUND)
        # When building as a subdirectory of the main Coin project, Coin already
        # brings in OpenGL transitively via COIN_TARGET_LINK_LIBRARIES.
        # We only need X11 explicitly for the GLX headless_utils.h context manager.
        target_link_libraries(${name} ${X11_X11_LIB})
        if(OpenGL_FOUND AND TARGET OpenGL::GL)
            target_link_libraries(${name} OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(${name} ${OPENGL_gl_LIBRARY})
        endif()
        target_include_directories(${name} PRIVATE ${X11_INCLUDE_DIR})
    endif()
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endmacro()

# ---- Test executables --------------------------------------------------------
add_rendering_test(render_primitives)
add_rendering_test(render_materials)
add_rendering_test(render_lighting)
add_rendering_test(render_transforms)
add_rendering_test(render_cameras)
add_rendering_test(render_drawstyle)
add_rendering_test(render_texture)

# ---- CTest image-comparison integration --------------------------------------
# Control images live in the shared tests/control_images/ directory.
# The run_image_test.cmake driver handles Xvfb wrapping for GLX builds.

set(CONTROL_IMAGES_DIR "${CMAKE_SOURCE_DIR}/tests/control_images")

# Per-test threshold overrides follow the same pattern as Mentor/CMakeLists.txt:
#   add_rendering_image_test(name RMSE_THRESHOLD 25.0)
#   add_rendering_image_test(name HASH_THRESHOLD 10 RMSE_THRESHOLD 25.0)
macro(add_rendering_image_test test_name)
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
                -P ${CMAKE_SOURCE_DIR}/tests/run_image_test.cmake
        )
        set_tests_properties(${test_name}_image_test PROPERTIES
            TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
        )
    endif()
endmacro()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_output")

# Per-test RMSE thresholds.
# Control images are generated from Obol + GLX; when testing with OSMesa the
# same scene may render with subtle floating-point or shading differences
# (measured at 0.95-7.67 RMSE).  Thresholds are set at roughly 2× the
# measured maximum difference to allow backend variation while still catching
# significant rendering regressions.
add_rendering_image_test(render_primitives RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_materials  RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_lighting)                         # < 2.0 RMSE, default ok
add_rendering_image_test(render_transforms RMSE_THRESHOLD 10.0)
add_rendering_image_test(render_cameras    RMSE_THRESHOLD 10.0)
add_rendering_image_test(render_drawstyle  RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_texture    RMSE_THRESHOLD 15.0)

# ---- Custom target: generate control images ----------------------------------
# Runs all rendering tests with GLX (via Xvfb) and converts the .rgb output to
# control PNGs.  Invoke with:
#   cmake --build build_dir --target generate_rendering_controls
if(PNG_FOUND)
    add_custom_target(generate_rendering_controls
        COMMAND ${CMAKE_COMMAND} -E env
            BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            CONTROL_DIR=${CONTROL_IMAGES_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_controls.sh
        DEPENDS
            image_comparator rgb_to_png
            render_primitives render_materials render_lighting
            render_transforms render_cameras render_drawstyle
            render_texture
        COMMENT "Generating PNG control images for rendering regression tests"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
