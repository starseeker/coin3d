# =============================================================================
# Visual Regression Tests  (tests/rendering/)
#
# Each test executable renders a scene with SoOffscreenRenderer and writes
# an SGI RGB file.  CTest compares that output against a PNG control image
# stored in tests/control_images/.
#
# Control images are generated from vanilla (upstream) Obol + GLX and committed
# once to the repository.  On every subsequent run the same source is compiled
# against Obol; if the pixel output diverges beyond the configured thresholds
# the test fails, indicating a rendering regression.
#
# Backend selection (inherits from parent project):
#   OBOL_USE_OSMESA=ON  → define OBOL_OSMESA_BUILD, link OSMesa
#   default               → system OpenGL via GLX (needs X server / Xvfb)
# =============================================================================

# ---- Backend detection -------------------------------------------------------
if(DEFINED OBOL_USE_OSMESA)
    set(RENDERING_USE_OSMESA ${OBOL_USE_OSMESA})
else()
    set(RENDERING_USE_OSMESA OFF CACHE BOOL "Use OSMesa for rendering tests")
endif()
# Note: OBOL_BUILD_DUAL_GL builds use system GL (GLX+Xvfb) for rendering
# tests; the OSMesa layer is in the library but scene traversal uses system GL.

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
# Compile one rendering-test executable; links against Obol and the chosen
# OpenGL backend.  The headless_utils.h lives in tests/utils/.
macro(add_rendering_test name)
    list(APPEND OBOL_GL_RENDER_TESTS ${name})
endmacro()

# Variant of add_rendering_test for tests that use the shared testlib scene
# factories.  These link against obol_testlib so the CLI test and the
# interactive obol_viewer render identical scenes from the same source.
macro(add_testlib_rendering_test name)
    add_rendering_test(${name})
    list(APPEND OBOL_GL_TESTLIB_RENDER_TESTS ${name})
endmacro()

# ---- Test executables --------------------------------------------------------
add_testlib_rendering_test(render_primitives)
add_testlib_rendering_test(render_materials)
add_testlib_rendering_test(render_lighting)
add_testlib_rendering_test(render_transforms)
add_testlib_rendering_test(render_cameras)
add_testlib_rendering_test(render_drawstyle)
add_testlib_rendering_test(render_texture)
add_testlib_rendering_test(render_text2)
add_testlib_rendering_test(render_text3)
# Moved from top-level SIMPLE_TESTS: these need a rendering context to run
add_testlib_rendering_test(render_gradient)
add_rendering_test(render_background_gradient)
add_testlib_rendering_test(render_colored_cube)
add_testlib_rendering_test(render_coordinates)
add_testlib_rendering_test(render_scene)
# Validation tests: do their own pixel analysis; registered directly with CTest
add_testlib_rendering_test(render_sphere_position)
add_testlib_rendering_test(render_checker_texture)
add_testlib_rendering_test(render_face_set)
add_testlib_rendering_test(render_line_set)
add_rendering_test(render_switch_visibility)
add_testlib_rendering_test(render_vertex_colors)
add_testlib_rendering_test(render_clip_plane)
add_testlib_rendering_test(render_indexed_face_set)
add_testlib_rendering_test(render_quad_mesh)
add_testlib_rendering_test(render_indexed_line_set)
add_testlib_rendering_test(render_lod)
add_testlib_rendering_test(render_point_set)
add_rendering_test(render_scene_texture)
add_testlib_rendering_test(render_shadow)
add_testlib_rendering_test(render_array_multiple_copy)
add_rendering_test(render_shape_hints)
add_rendering_test(render_environment)
add_rendering_test(render_texture3)
add_rendering_test(render_bump_map)
add_rendering_test(render_texture_transform)
add_rendering_test(render_depth_buffer)
add_rendering_test(render_alpha_test)
add_testlib_rendering_test(render_draggers)
add_rendering_test(render_offscreen)
add_testlib_rendering_test(render_triangle_strip_set)
add_testlib_rendering_test(render_ascii_text)
add_testlib_rendering_test(render_image_node)
add_rendering_test(render_sorender_manager)
add_rendering_test(render_gl_big_image)
add_rendering_test(render_cubemap)
add_rendering_test(render_gl_features)
add_rendering_test(render_multi_texture)
add_testlib_rendering_test(render_annotation)
add_testlib_rendering_test(render_reset_transform)
add_testlib_rendering_test(render_transparency)
add_testlib_rendering_test(render_marker_set)
add_testlib_rendering_test(render_hud_overlay)
add_testlib_rendering_test(render_hud_no3d)
add_rendering_test(render_hud_interaction)
# Testlib demo scenes: these use the shared scene factories so CLI and
# obol_viewer render identical output.
add_testlib_rendering_test(render_text_demo)
add_testlib_rendering_test(render_hud_demo)
add_rendering_test(render_procedural_shape)
add_rendering_test(render_arb8_draggers)
add_rendering_test(render_arb8_edit_cycle)
add_rendering_test(render_pick_interaction)
add_testlib_rendering_test(render_simple_draggers)
add_testlib_rendering_test(render_light_manips)
add_rendering_test(render_event_callback_interaction)
add_rendering_test(render_selection_interaction)
add_rendering_test(render_sensor_interaction)
add_rendering_test(render_pick_filter)
add_rendering_test(render_scene_interaction)
add_rendering_test(render_callback_node)
add_rendering_test(render_camera_interaction)
add_testlib_rendering_test(render_manip_sequences)
add_rendering_test(render_nodekit_interaction)
add_rendering_test(render_path_operations)
add_rendering_test(render_lod_picking)
add_rendering_test(render_engine_interaction)
add_rendering_test(render_bbox_action)
add_rendering_test(render_search_action)
add_rendering_test(render_callback_action)
add_rendering_test(render_event_propagation)
add_rendering_test(render_shader_program)
add_rendering_test(render_raypick_shapes)
add_rendering_test(render_material_binding)
add_rendering_test(render_shadow_advanced)
add_rendering_test(render_ext_selection)
add_rendering_test(render_view_volume_ops)
add_rendering_test(render_glrender_action_modes)
add_rendering_test(render_callback_action_deep)
add_rendering_test(render_text3_parts)
add_rendering_test(render_write_read_action)
add_rendering_test(render_sensors_rendering)
add_rendering_test(render_ext_selection_events)
add_rendering_test(render_render_manager_full)
add_rendering_test(render_offscreen_advanced)
add_rendering_test(render_glrender_deep)
add_rendering_test(render_engine_converter)
add_rendering_test(render_image_deep)
add_rendering_test(render_quad_mesh_deep)
add_rendering_test(render_field_connections)
add_rendering_test(render_sogl_bindings)
add_rendering_test(render_rt_proxy_shapes)

# ---- NanoRT raytracing backend -------------------------------------------
#
# add_nanort_rendering_test(name)
#
# Compiles <name>.cpp with:
#   - OBOL_NANORT_BUILD defined  →  headless_utils.h uses SoNanoRTContextManager
#   - nanort.h and stb_image_write.h on the include path
#
# Tests built this way work without a display server and without OpenGL.
# They still use the standard renderToFile() + writeToRGB() path from
# headless_utils.h; SoOffscreenRenderer dispatches to the nanort renderer
# via the generalized ContextManager::renderScene() hook.
macro(add_nanort_rendering_test name)
    add_executable(nrt_${name} ${name}.cpp)
    target_include_directories(nrt_${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
        ${PROJECT_SOURCE_DIR}/external/nanort
        ${PROJECT_SOURCE_DIR}/external/nanort/examples/common
    )
    target_compile_definitions(nrt_${name} PRIVATE OBOL_NANORT_BUILD)
    target_link_libraries(nrt_${name} ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
    set_target_properties(nrt_${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
    add_test(
        NAME nrt_${name}
        COMMAND "${CMAKE_COMMAND}"
            -DEXECUTABLE=$<TARGET_FILE:nrt_${name}>
            -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rendering_test.cmake"
    )
    set_tests_properties(nrt_${name} PROPERTIES
        TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
    )
endmacro()

# Variant for NanoRT tests whose source includes testlib/test_scenes.h
macro(add_nanort_testlib_rendering_test name)
    add_nanort_rendering_test(${name})
    if(TARGET obol_testlib)
        target_link_libraries(nrt_${name} obol_testlib)
        target_include_directories(nrt_${name} PRIVATE
            ${CMAKE_SOURCE_DIR}/tests
        )
    endif()
endmacro()

# ---- NanoRT versions of standard rendering tests ----------------------------
# These tests use SoOffscreenRenderer + the generalized ContextManager hook to
# produce pixel output via nanort.  They operate on triangle-based geometry;
# line geometry is rendered via cylinder proxies (createCylinderProxy /
# createSphereProxy), so dragger/manipulator tests whose parts include
# SoIndexedLineSet feedback geometry are fully supported.
# SoText2 screen-space text is rendered via buildPixelBuffer() + framebuffer
# compositing so letter shapes appear correctly (not bounding-box blocks).
add_nanort_testlib_rendering_test(render_primitives)
add_nanort_testlib_rendering_test(render_materials)
add_nanort_testlib_rendering_test(render_transforms)
add_nanort_testlib_rendering_test(render_colored_cube)
add_nanort_testlib_rendering_test(render_scene)
add_nanort_rendering_test(render_rt_proxy_shapes)
add_nanort_rendering_test(render_background_gradient)
# NanoRT shadow test: validates shadow-ray casting via SoRaytracingParams.
# Uses SoDirectionalLight + SoRaytracingParams(shadowsEnabled=TRUE); does NOT
# use SoShadowGroup (which requires OpenGL GLSL shaders).  The pixel-level
# shadow validation (lit vs shadow regions on the ground plane) is only run
# under the NanoRT build; the GL backend does a basic non-blank geometry check.
add_nanort_rendering_test(render_nanort_shadow)

# NanoRT text3_parts test: validates that SoText3 geometry (FRONT/ALL/BACK parts,
# multiple coloured rows) is correctly collected via SoCallbackAction and rendered
# without the double-scaling bug in SoText3P::generate.
add_nanort_rendering_test(render_text3_parts)

# NanoRT text demo: validates that SoText2 glyph pixels are composited
# correctly onto the NanoRT framebuffer (letter shapes, not solid blocks).
if(TARGET obol_testlib)
    add_executable(nrt_render_text_demo render_text_demo.cpp)
    target_include_directories(nrt_render_text_demo PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests/utils
        ${CMAKE_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
        ${PROJECT_SOURCE_DIR}/external/nanort
        ${PROJECT_SOURCE_DIR}/external/nanort/examples/common
    )
    target_compile_definitions(nrt_render_text_demo PRIVATE OBOL_NANORT_BUILD)
    target_link_libraries(nrt_render_text_demo ${PROJECT_NAME} obol_testlib
                          ${OBOL_TARGET_LINK_LIBRARIES})
    set_target_properties(nrt_render_text_demo PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
    add_test(
        NAME nrt_render_text_demo
        COMMAND "${CMAKE_COMMAND}"
            -DEXECUTABLE=$<TARGET_FILE:nrt_render_text_demo>
            -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rendering_test.cmake"
    )
    set_tests_properties(nrt_render_text_demo PROPERTIES
        TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
    )
endif()

# ---- NanoRT dragger and manipulator tests ------------------------------------
# Dragger/manipulator parts use SoCube, SoCone, SoCylinder, SoSphere,
# SoIndexedLineSet, and SoIndexedFaceSet — all of which are fully supported
# by the nanort context manager (line/point geometry via cylinder/sphere
# proxies).  SoCallbackAction traverses SoNodeKit (the dragger base class)
# normally, collecting triangles from whichever SoSwitch branch is active.
# SoSurroundScale and SoAntiSquish execute correctly during traversal and
# apply the correct world-space transforms to the collected geometry.
# Note: SoClipPlane (used by SoClipPlaneManip) has no nanort equivalent;
# geometry renders without clipping, which is a cosmetic difference only.
add_nanort_testlib_rendering_test(render_simple_draggers)
add_nanort_testlib_rendering_test(render_draggers)
add_nanort_testlib_rendering_test(render_light_manips)
add_nanort_testlib_rendering_test(render_manip_sequences)

# ---- Standalone nanort demo (original, uses its own context manager) --------
# render_nanort.cpp is the original proof-of-concept demo that builds its own
# BVH and does ray-tracing without the SoNanoRTContextManager; it does NOT
# define OBOL_NANORT_BUILD and does NOT link obol_testlib.
add_executable(render_nanort render_nanort.cpp)
target_include_directories(render_nanort PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    ${PROJECT_SOURCE_DIR}/external/nanort
    ${PROJECT_SOURCE_DIR}/external/nanort/examples/common
)
target_link_libraries(render_nanort ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
set_target_properties(render_nanort PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# ---- stb_truetype direct-render reference tool -------------------------------
# stt_reference renders the same text strings as render_text2 and
# render_text_demo directly via SbFont/stb_truetype without OpenGL.
# The output PNG serves as a ground-truth reference for evaluating the
# fidelity of the GL and NanoRT text rendering paths.
# It does not require a display server.
add_executable(stt_reference stt_reference.cpp)
target_include_directories(stt_reference PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    ${PROJECT_SOURCE_DIR}/external/nanort
)
target_link_libraries(stt_reference ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
set_target_properties(stt_reference PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
add_test(
    NAME stt_reference
    COMMAND "${CMAKE_COMMAND}"
        -DEXECUTABLE=$<TARGET_FILE:stt_reference>
        -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rendering_test.cmake"
)
set_tests_properties(stt_reference PROPERTIES
    TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
)

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
                -DEXECUTABLE=$<TARGET_FILE:obol_test>
                -DTEST_NAME=${test_name}
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

# ---- NanoRT image-comparison macro -------------------------------------------
#
# add_nanort_image_test(name [HASH_THRESHOLD n] [RMSE_THRESHOLD f])
#
# Runs nrt_<name> to produce an SGI RGB image, then compares it against the
# OSMesa control image (<name>_control.png) using image_comparator.
#
# NanoRT uses Phong ray-tracing rather than OpenGL rasterization, so the renders
# will differ in shading detail (specular highlights, ambient fill) while still
# depicting the same scene geometry and colours.  The thresholds are therefore
# set higher than the OSMesa regression tests to accommodate these algorithm
# differences, while still catching major regressions such as missing geometry,
# wrong colours, or a completely black image.
#
# Default thresholds:
#   HASH_THRESHOLD 15   – perceptual hash (0-64 range; 5 is near-identical)
#   RMSE_THRESHOLD 60.0 – per-channel RMSE (0-255 range)
macro(add_nanort_image_test name)
    set(_ctrl "${CONTROL_IMAGES_DIR}/${name}_control.png")
    set(_base "${CMAKE_CURRENT_BINARY_DIR}/test_output/nrt_${name}_test")

    # Default thresholds – more lenient than the OSMesa tests because NanoRT
    # uses a different shading model (Phong ray-tracing vs GL rasterization).
    set(_hash_thresh 15)
    set(_rmse_thresh 60.0)

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

    if(EXISTS "${_ctrl}")
        add_test(
            NAME nrt_${name}_image_test
            COMMAND ${CMAKE_COMMAND}
                -DEXECUTABLE=$<TARGET_FILE:nrt_${name}>
                -DTEST_BASE=${_base}
                -DCONTROL_IMAGE=${_ctrl}
                -DCOMPARATOR=$<TARGET_FILE:image_comparator>
                -DHASH_THRESHOLD=${_hash_thresh}
                -DRMSE_THRESHOLD=${_rmse_thresh}
                -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
                -P ${CMAKE_SOURCE_DIR}/tests/run_image_test.cmake
        )
        set_tests_properties(nrt_${name}_image_test PROPERTIES
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
add_rendering_test(render_stt_gl)

# ---- Build GL render test objects into one library ---------------------------
# Each source's main() is renamed by compiling a small per-test wrapper that
# #define-s main before #include-ing the real source.  The wrapper files live
# in the binary directory so they never affect NanoRT (or other) targets that
# compile the same sources without the define.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gl_wrappers")
set(_gl_wrapper_sources "")
foreach(_t ${OBOL_GL_RENDER_TESTS})
    set(_wrapper "${CMAKE_CURRENT_BINARY_DIR}/gl_wrappers/${_t}_gl.cpp")
    file(WRITE "${_wrapper}"
        "#define main run_render_test_${_t}\n"
        "#include \"${CMAKE_CURRENT_SOURCE_DIR}/${_t}.cpp\"\n"
    )
    list(APPEND _gl_wrapper_sources "${_wrapper}")
endforeach()

add_library(obol_render_tests_gl STATIC ${_gl_wrapper_sources})

target_include_directories(obol_render_tests_gl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/tests/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
target_link_libraries(obol_render_tests_gl
    PUBLIC ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES}
)
if(RENDERING_USE_OSMESA)
    target_compile_definitions(obol_render_tests_gl PRIVATE OBOL_OSMESA_BUILD)
    if(OSMESA_INCLUDE_DIR)
        target_include_directories(obol_render_tests_gl PRIVATE ${OSMESA_INCLUDE_DIR})
    endif()
    if(OSMESA_LINK_TARGET)
        target_link_libraries(obol_render_tests_gl PUBLIC ${OSMESA_LINK_TARGET})
    endif()
elseif(X11_FOUND)
    target_link_libraries(obol_render_tests_gl PUBLIC ${X11_X11_LIB})
    if(OpenGL_FOUND AND TARGET OpenGL::GL)
        target_link_libraries(obol_render_tests_gl PUBLIC OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
        target_link_libraries(obol_render_tests_gl PUBLIC ${OPENGL_gl_LIBRARY})
    endif()
    target_include_directories(obol_render_tests_gl PRIVATE ${X11_INCLUDE_DIR})
endif()
if(TARGET obol_testlib)
    target_link_libraries(obol_render_tests_gl PUBLIC obol_testlib)
    target_include_directories(obol_render_tests_gl PRIVATE ${CMAKE_SOURCE_DIR}/tests)
endif()

# Export the list to parent scope so tests/CMakeLists.txt can link obol_test
set(OBOL_GL_RENDER_TESTS "${OBOL_GL_RENDER_TESTS}" PARENT_SCOPE)

add_rendering_image_test(render_stt_gl     RMSE_THRESHOLD 15.0)
# Text rendering: control images generated with Obol's ProFont baseline.
# Higher RMSE threshold to accommodate subpixel differences between platforms.
add_rendering_image_test(render_text2      RMSE_THRESHOLD 20.0)
add_rendering_image_test(render_text3      RMSE_THRESHOLD 20.0)
add_rendering_image_test(render_ascii_text RMSE_THRESHOLD 20.0)
add_rendering_image_test(render_gradient    RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_colored_cube RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_coordinates RMSE_THRESHOLD 10.0)
add_rendering_image_test(render_scene       RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_shape_hints      RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_environment      RMSE_THRESHOLD 20.0)
add_rendering_image_test(render_texture3         RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_bump_map         RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_texture_transform RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_depth_buffer     RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_alpha_test       RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_indexed_face_set  RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_quad_mesh         RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_indexed_line_set  RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_point_set         RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_lod               RMSE_THRESHOLD 15.0)
add_rendering_image_test(render_scene_texture     RMSE_THRESHOLD 20.0)
add_rendering_image_test(render_array_multiple_copy RMSE_THRESHOLD 15.0)
# Shadow rendering: control image generated with OSMesa backend (SoShadowGroup
# GLSL shader compilation crashes on headless GLX pixmap contexts due to
# multiple GL contexts in the same process).  Only register the image
# comparison test when building with OSMesa so GLX builds don't crash.
if(RENDERING_USE_OSMESA)
    add_rendering_image_test(render_shadow RMSE_THRESHOLD 25.0)
endif()

# HUD image-comparison tests (text rendering: higher RMSE threshold for
# subpixel font differences between platforms / backends).
add_rendering_image_test(render_hud_overlay  RMSE_THRESHOLD 25.0)
add_rendering_image_test(render_hud_no3d     RMSE_THRESHOLD 25.0)

# Testlib demo scenes (shared scene factories; match obol_viewer output).
add_rendering_image_test(render_text_demo    RMSE_THRESHOLD 25.0)
add_rendering_image_test(render_hud_demo     RMSE_THRESHOLD 25.0)

# SoProceduralShape visual regression test: solid filled + wireframe cone.
add_rendering_image_test(render_procedural_shape RMSE_THRESHOLD 15.0)

# ARB8 edit-cycle visual progression test: 4-step edit cycle with
# selection display (handle spheres + text labels) and vertex manipulation.
# Text labels increase subpixel variance so a higher RMSE threshold is used.
add_rendering_image_test(render_arb8_edit_cycle RMSE_THRESHOLD 25.0)

# ---- NanoRT image-comparison tests -------------------------------------------
# Compare NanoRT (CPU ray-tracing) output against the OSMesa control images.
# These use the same scenes as the OSMesa tests and should produce substantially
# similar results despite the different rendering algorithms.
# render_rt_proxy_shapes has no control image and is omitted here.
add_nanort_image_test(render_primitives  RMSE_THRESHOLD 60.0)
add_nanort_image_test(render_materials   RMSE_THRESHOLD 60.0)
add_nanort_image_test(render_transforms  RMSE_THRESHOLD 60.0)
add_nanort_image_test(render_colored_cube RMSE_THRESHOLD 60.0)
add_nanort_image_test(render_scene        RMSE_THRESHOLD 60.0)

# ---- Validation tests: run unconditionally, check exit code only -------------
# These tests perform their own pixel analysis and return 0/1 directly.
# run_rendering_test.cmake wraps with xvfb-run when DISPLAY is not set.
#
# Two timeouts are intentional:
#   -DTEST_TIMEOUT  passed into the script for the execute_process() call so
#                   the child executable is killed if it hangs.
#   TIMEOUT property  the outer CTest timeout that covers cmake script startup
#                     overhead as well as the subprocess; set to the same value.
foreach(_vtest render_sphere_position render_checker_texture
               render_face_set render_line_set render_switch_visibility
               render_vertex_colors render_clip_plane
               render_indexed_face_set render_quad_mesh render_indexed_line_set
               render_lod render_point_set
               render_background_gradient
               render_scene_texture render_array_multiple_copy
               render_shape_hints render_environment render_texture3
               render_bump_map render_texture_transform
               render_depth_buffer render_alpha_test
               render_draggers render_offscreen
               render_triangle_strip_set render_ascii_text
               render_image_node render_sorender_manager
               render_gl_big_image render_cubemap render_gl_features
               render_multi_texture render_marker_set
               render_annotation render_reset_transform render_transparency
               render_hud_interaction
               render_pick_interaction render_simple_draggers
               render_light_manips render_event_callback_interaction
               render_selection_interaction
               render_sensor_interaction render_pick_filter
               render_scene_interaction
               render_callback_node render_camera_interaction
               render_manip_sequences
               render_nodekit_interaction render_path_operations
               render_lod_picking render_engine_interaction
               render_bbox_action render_search_action
               render_callback_action render_event_propagation
               render_shader_program render_raypick_shapes
               render_material_binding render_shadow_advanced
               render_ext_selection render_view_volume_ops
               render_glrender_action_modes
               render_callback_action_deep render_text3_parts
               render_write_read_action render_sensors_rendering
               render_procedural_shape render_arb8_edit_cycle
               render_ext_selection_events
               render_render_manager_full
               render_offscreen_advanced
               render_glrender_deep
               render_engine_converter
               render_image_deep
               render_quad_mesh_deep
               render_field_connections
               render_sogl_bindings
               render_rt_proxy_shapes)
    add_test(
        NAME ${_vtest}
        COMMAND "${CMAKE_COMMAND}"
            -DEXECUTABLE=$<TARGET_FILE:obol_test>
            -DTEST_NAME=${_vtest}
            -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rendering_test.cmake"
    )
    set_tests_properties(${_vtest} PROPERTIES
        TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
    )
endforeach()

# render_nanort is still a standalone executable
add_test(
    NAME render_nanort
    COMMAND "${CMAKE_COMMAND}"
        -DEXECUTABLE=$<TARGET_FILE:render_nanort>
        -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rendering_test.cmake"
)
set_tests_properties(render_nanort PROPERTIES
    TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
)

# Shadow rendering validation test: only safe to run with OSMesa (SoShadowGroup
# GLSL shader compilation crashes on headless GLX pixmap contexts).
if(RENDERING_USE_OSMESA)
    add_test(
        NAME render_shadow
        COMMAND "${CMAKE_COMMAND}"
            -DEXECUTABLE=$<TARGET_FILE:obol_test>
            -DTEST_NAME=render_shadow
            -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_rendering_test.cmake"
    )
    set_tests_properties(render_shadow PROPERTIES
        TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
    )
endif()

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
            obol_test
        COMMENT "Generating PNG control images for rendering regression tests"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
