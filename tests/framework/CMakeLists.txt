# =============================================================================
# Modern Obol test framework
#
# This is deliberately independent of the removed registry / ObolEx layer.
# GTest supplies normal assertions, fixtures, filtering and CTest discovery;
# obol_test_support supplies only Obol-specific lifecycle and rendering helpers.
# =============================================================================

find_package(GTest 1.10 REQUIRED)
include(GoogleTest)

add_test(NAME package.installed_consumer
    COMMAND ${CMAKE_COMMAND}
        -DOBOL_BINARY_DIR=${PROJECT_BINARY_DIR}
        -DOBOL_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DOBOL_CONFIG=$<CONFIG>
        "-DOBOL_GENERATOR=${CMAKE_GENERATOR}"
        "-DOBOL_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DOBOL_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DOBOL_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DOBOL_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        -P ${PROJECT_SOURCE_DIR}/tests/package/run_consumer.cmake)
set_tests_properties(package.installed_consumer PROPERTIES
    LABELS "package"
    TIMEOUT 180
    RUN_SERIAL TRUE)

add_library(obol_test_support STATIC
    image_assertions.cpp
    stb_image_write_impl.cpp
    ${PROJECT_SOURCE_DIR}/tests/test_utils.cpp
    test_context.cpp
    render_fixture.cpp
)
# Test sources deliberately use translation-unit-local helpers with concise
# names. Parent projects may enable CMAKE_UNITY_BUILD globally, but combining
# independent GTest sources changes that isolation and causes ODR collisions.
set_target_properties(obol_test_support PROPERTIES UNITY_BUILD OFF)
if(OBOL_USE_SWRAST OR OBOL_USE_SYSTEM_GL)
    # The scene catalogue is shared by fixture-backed render contracts.  Keep
    # it in the modern support target so converted cases do not need a second
    # renderer-specific library.
    target_sources(obol_test_support PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/testlib/test_scenes.cpp
    )
endif()
if(OBOL_USE_SWRAST)
    target_compile_definitions(obol_test_support PRIVATE OBOL_TEST_HAVE_SWRAST)
endif()
if(OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE)
    target_compile_definitions(obol_test_support PRIVATE OBOL_TEST_HAVE_SYSTEM_GL)
endif()

if(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
    target_compile_definitions(obol_test_support PRIVATE OBOL_DUAL_GL_BUILD)
endif()
if(OBOL_PROFILING)
    # A small number of API-contract tests need to know whether the library
    # contains the optional implementation.  Keep the macro scoped to test
    # code rather than exposing this private build detail to Obol consumers.
    target_compile_definitions(obol_test_support PUBLIC OBOL_PROFILING)
endif()
target_link_libraries(obol_test_support
    PUBLIC
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        GTest::gtest
)
if(OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE)
    # render_fixture.cpp owns the GLX context implementation, so the support
    # library must carry its direct link dependency to every consumer.  This
    # also keeps dual-backend executables linkable even when they select
    # OSMesa at runtime.
    if(TARGET OpenGL::GLX)
        target_link_libraries(obol_test_support PUBLIC OpenGL::GLX)
    elseif(OPENGL_glx_LIBRARY)
        target_link_libraries(obol_test_support PUBLIC ${OPENGL_glx_LIBRARY})
    else()
        message(FATAL_ERROR
            "System-GL tests require a GLX library on Unix")
    endif()
endif()
target_include_directories(obol_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/tests/testlib
        ${PROJECT_SOURCE_DIR}/examples/demo_support
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_SOURCE_DIR}/external
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(EXISTS "${PROJECT_SOURCE_DIR}/fonts/Iosevka/IosevkaAile-Regular.ttc")
    target_compile_definitions(obol_test_support PRIVATE
        OBOL_FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")
endif()

if(USE_PTHREAD)
    target_link_libraries(obol_test_support PUBLIC pthread)
endif()

function(obol_add_gtest target label)
    cmake_parse_arguments(OBOL_GTEST ""
        "TIMEOUT;ENVIRONMENT;TEST_PREFIX;SKIP_REGULAR_EXPRESSION" "" ${ARGN})
    if(OBOL_GTEST_TIMEOUT)
        set(_obol_gtest_timeout ${OBOL_GTEST_TIMEOUT})
    else()
        set(_obol_gtest_timeout 30)
    endif()
    add_executable(${target} test_main.cpp ${OBOL_GTEST_UNPARSED_ARGUMENTS})
    set_target_properties(${target} PROPERTIES UNITY_BUILD OFF)
    target_link_libraries(${target}
        PRIVATE
            obol_test_support
            GTest::gtest
    )
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
    )
    # GoogleTest's discovery helper forwards PROPERTIES as a CMake list and
    # cannot preserve a semicolon-containing LABELS value.  Normalize compound
    # categories to one searchable label (for example render-osmesa) so later
    # TIMEOUT and ENVIRONMENT pairs cannot be shifted and misinterpreted.
    string(REPLACE ";" "-" _obol_gtest_labels "${label}")
    set(_obol_gtest_properties
        LABELS "${_obol_gtest_labels}"
        TIMEOUT ${_obol_gtest_timeout})
    if(OBOL_GTEST_ENVIRONMENT)
        list(APPEND _obol_gtest_properties
            ENVIRONMENT "${OBOL_GTEST_ENVIRONMENT}")
    endif()
    if(OBOL_GTEST_SKIP_REGULAR_EXPRESSION)
        list(APPEND _obol_gtest_properties
            SKIP_REGULAR_EXPRESSION
            "${OBOL_GTEST_SKIP_REGULAR_EXPRESSION}")
    endif()
    set(_obol_gtest_discovery_options)
    if(OBOL_GTEST_TEST_PREFIX)
        list(APPEND _obol_gtest_discovery_options
            TEST_PREFIX "${OBOL_GTEST_TEST_PREFIX}")
    endif()
    gtest_discover_tests(${target}
        DISCOVERY_MODE PRE_TEST
        PROPERTIES ${_obol_gtest_properties}
        ${_obol_gtest_discovery_options}
    )
endfunction()

# Lifecycle tests must begin before SoDB has been initialized and must be able
# to call SoDB::finish().  Keep them in a dedicated GTest process instead of
# sharing the normal test_main.cpp, which intentionally initializes Obol once.
add_executable(obol_lifecycle_tests
    ${PROJECT_SOURCE_DIR}/tests/unit/lifecycle_test.cpp
)
set_target_properties(obol_lifecycle_tests PROPERTIES UNITY_BUILD OFF)
target_link_libraries(obol_lifecycle_tests
    PRIVATE
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        GTest::gtest_main
)
target_include_directories(obol_lifecycle_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
gtest_discover_tests(obol_lifecycle_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES
        LABELS "unit-lifecycle"
        TIMEOUT 30
)

# A loadable extension must bind to the same process-global type registry as
# its host.  That is the supported shared-library plugin model; linking a
# module to a static Obol archive would give the module a second registry and
# cannot exercise dynamic loading correctly.
if(BUILD_SHARED_LIBS)
    # Exercise the extension-node loader with a real shared object.  The
    # deliberately short class name guards against heuristic filtering of
    # valid third-party types.
    add_library(obol_dynload_probe MODULE
        ${PROJECT_SOURCE_DIR}/tests/unit/dynload_probe.cpp)
    set_target_properties(obol_dynload_probe PROPERTIES
        OUTPUT_NAME Probe
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    target_link_libraries(obol_dynload_probe PRIVATE ${PROJECT_NAME})
    target_include_directories(obol_dynload_probe PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/include)

    function(obol_add_dynload_probe probe source)
        string(TOLOWER "${probe}" _probe_lower)
        add_library(obol_dynload_${_probe_lower} MODULE
            ${source})
        set_target_properties(obol_dynload_${_probe_lower} PROPERTIES
            OUTPUT_NAME ${probe}
            WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
        target_link_libraries(obol_dynload_${_probe_lower} PRIVATE ${PROJECT_NAME})
        target_include_directories(obol_dynload_${_probe_lower} PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_BINARY_DIR}/include)
    endfunction()
    obol_add_dynload_probe(ThrowProbe
        ${PROJECT_SOURCE_DIR}/tests/unit/dynload_throw_probe.cpp)
    obol_add_dynload_probe(WrongProbe
        ${PROJECT_SOURCE_DIR}/tests/unit/dynload_wrong_probe.cpp)

    if(WIN32)
        set(_obol_dynload_environment
            "PATH=$<TARGET_FILE_DIR:obol_dynload_probe>;$ENV{PATH}")
    elseif(APPLE)
        set(_obol_dynload_environment
            "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:obol_dynload_probe>")
    else()
        set(_obol_dynload_environment
            "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:obol_dynload_probe>")
    endif()
    obol_add_gtest(obol_dynload_tests "unit;dynload"
        ENVIRONMENT "${_obol_dynload_environment}"
        ${PROJECT_SOURCE_DIR}/tests/unit/type_dynload_test.cpp)
    add_dependencies(obol_dynload_tests
        obol_dynload_probe
        obol_dynload_throwprobe
        obol_dynload_wrongprobe)
endif()

# Exhausting SoType mutates process-global state irreversibly.  Keep this one
# boundary test isolated from all ordinary unit cases.
add_executable(obol_type_capacity_tests
    ${PROJECT_SOURCE_DIR}/tests/unit/type_capacity_test.cpp)
set_target_properties(obol_type_capacity_tests PROPERTIES UNITY_BUILD OFF)
target_link_libraries(obol_type_capacity_tests PRIVATE
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    GTest::gtest_main)
target_include_directories(obol_type_capacity_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include)
gtest_discover_tests(obol_type_capacity_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES LABELS "unit-capacity" TIMEOUT 60)

obol_add_gtest(obol_unit_tests "unit"
    ${PROJECT_SOURCE_DIR}/tests/unit/base_math_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/base_geometry_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/base_utilities_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/base_value_types_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/draggers_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/engines_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/error_handling_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/events_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/fields_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/image_assertions_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/image_fields_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/image_format_registry_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/interaction_kit_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/nodes_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/nanort_context_manager_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/projectors_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/scene_graph_structure_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/sensors_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/tools/test_tools_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/cad/test_cad_ids.cpp
    ${PROJECT_SOURCE_DIR}/tests/threads/test_threads.cpp
)

obol_add_gtest(obol_integration_tests "integration"
    ${PROJECT_SOURCE_DIR}/tests/integration/actions_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/integration/io_round_trip_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/integration/paths_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/integration/cad_picking_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/cad/test_cad_instance_records.cpp
    ${PROJECT_SOURCE_DIR}/tests/cad/test_cad_resolved_draw.cpp
)
target_include_directories(obol_integration_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/cad
    ${PROJECT_SOURCE_DIR}/src/cad/picking
)

# Every backend-independent rendering contract belongs in one manifest.  Each
# enabled backend compiles and discovers exactly this corpus, which prevents a
# new software-lane regression test from silently missing native OpenGL.
set(OBOL_PORTABLE_RENDER_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_fixture_smoke_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/offscreen_renderer_api_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_semantic_contracts_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/interaction_contracts_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/portable_feature_contracts_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/demo_catalogue_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/subtexture_render_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/visual_regression_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_lighting.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_cameras.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_colored_cube.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_coordinates.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_indexed_face_set.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_point_set.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_quad_mesh.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_indexed_line_set.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_face_set.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_line_set.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_triangle_strip_set.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_vertex_colors.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_marker_set.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_image_node.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_sphere_position.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_reset_transform.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_switch_visibility.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_drawstyle.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_lod.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_text3.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_texture.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_hud_overlay.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_text2.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_hud_no3d.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_hud_demo.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_scene.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_cubemap.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_gl_big_image.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_multi_texture.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_transparency.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_ascii_text.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_annotation.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_array_multiple_copy.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_checker_texture.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_clip_plane.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_shadow.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_gradient.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_background_gradient.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_iosevka_text2.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_iosevka_text3.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_text_demo.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_sorender_manager.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_gl_features.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_texture3.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_bump_map.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_environment.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_alpha_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_texture_transform.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_depth_buffer.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_shape_hints.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_viewport_scene.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_scene_texture.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_scene_texture_multi_mgr.cpp
    ${PROJECT_SOURCE_DIR}/examples/demo_scenes/demo_scenes.cpp
)

function(obol_configure_render_target target)
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/examples/demo_scenes)
    target_compile_definitions(${target} PRIVATE
        OBOL_CONTROL_IMAGES_DIR="${PROJECT_SOURCE_DIR}/tests/control_images"
        OBOL_TEST_OUTPUT_DIR="${PROJECT_BINARY_DIR}/test-output/${target}"
        OBOL_RENDER_TEST_MAIN)
endfunction()

# OSMesa is the deterministic software lane.  In a dual build its selection
# remains runtime-controlled through OBOL_TEST_RENDER_BACKEND.
if(OBOL_USE_SWRAST)
    obol_add_gtest(obol_render_tests "render;osmesa"
        ENVIRONMENT "OBOL_TEST_RENDER_BACKEND=swrast"
        ${OBOL_PORTABLE_RENDER_SOURCES})
    obol_configure_render_target(obol_render_tests)
else()
    message(STATUS "Software-rasterizer render lane: skipped (OBOL_USE_SWRAST=OFF)")
endif()

# Exercise native dispatch and GLX with the same portable contracts.
if(OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE)
    obol_add_gtest(obol_system_gl_render_tests "render;system-gl"
        ENVIRONMENT "OBOL_TEST_RENDER_BACKEND=system"
        TEST_PREFIX "system-gl."
        SKIP_REGULAR_EXPRESSION "system OpenGL backend is unavailable"
        ${OBOL_PORTABLE_RENDER_SOURCES})
    obol_configure_render_target(obol_system_gl_render_tests)
elseif(OBOL_USE_SYSTEM_GL)
    message(STATUS
        "System-GL render lane: skipped (the modern native fixture currently requires GLX)")
endif()

if(NOT OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
    obol_add_gtest(obol_nogl_tests "nogl"
        ${PROJECT_SOURCE_DIR}/tests/rendering/nogl_scenegraph_test.cpp)
endif()

option(OBOL_BUILD_STRESS_TESTS
    "Build opt-in stress and sanitizer-oriented tests"
    OFF)
if(OBOL_BUILD_STRESS_TESTS)
    obol_add_gtest(obol_stress_tests "stress;threads" TIMEOUT 120
        ${PROJECT_SOURCE_DIR}/tests/threads/test_thread_safety.cpp
    )
    # The sanitizer suite exercises private cache-ID infrastructure directly.
    target_compile_definitions(obol_stress_tests PRIVATE OBOL_INTERNAL)
endif()

# -----------------------------------------------------------------------------
# Source-level unit cases
# -----------------------------------------------------------------------------
# These files are ordinary source-level GTest suites with independently
# discoverable cases and no process-local result recorder.
set(OBOL_DOMAIN_UNIT_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/actions/test_actions_extended.cpp
    ${PROJECT_SOURCE_DIR}/tests/actions/test_actions_pick.cpp
    ${PROJECT_SOURCE_DIR}/tests/actions/test_actions_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/actions/test_glrender_action.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_base_advanced.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_base_extras.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_sb_matrix_box.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_sb_plist.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_sb_rotation_plane.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_sb_types.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_sb_viewvolume.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_sbdpmatrix_full.cpp
    ${PROJECT_SOURCE_DIR}/tests/base/test_sbtime_sbdict.cpp
    ${PROJECT_SOURCE_DIR}/tests/bundles/test_bundles_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/caches/test_caches_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/details/test_details_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/details/test_details_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/draggers/test_dragger_sequences.cpp
    ${PROJECT_SOURCE_DIR}/tests/draggers/test_draggers.cpp
    ${PROJECT_SOURCE_DIR}/tests/elements/test_elements_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/engines/test_engines_advanced.cpp
    ${PROJECT_SOURCE_DIR}/tests/engines/test_engines_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/engines/test_engines_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/engines/test_engines_suite2.cpp
    ${PROJECT_SOURCE_DIR}/tests/errors/test_errors_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/events/test_events_deeper.cpp
    ${PROJECT_SOURCE_DIR}/tests/events/test_events_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/fields/test_field_advanced.cpp
    ${PROJECT_SOURCE_DIR}/tests/fields/test_field_connections.cpp
    ${PROJECT_SOURCE_DIR}/tests/fields/test_field_containers.cpp
    ${PROJECT_SOURCE_DIR}/tests/fields/test_field_extras.cpp
    ${PROJECT_SOURCE_DIR}/tests/fields/test_mf_fields.cpp
    ${PROJECT_SOURCE_DIR}/tests/fields/test_sf_fields.cpp
    ${PROJECT_SOURCE_DIR}/tests/io/test_io_edge_cases.cpp
    ${PROJECT_SOURCE_DIR}/tests/io/test_io_file_ops.cpp
    ${PROJECT_SOURCE_DIR}/tests/io/test_io_input_binary.cpp
    ${PROJECT_SOURCE_DIR}/tests/io/test_io_write_action.cpp
    ${PROJECT_SOURCE_DIR}/tests/io/test_sodb.cpp
    ${PROJECT_SOURCE_DIR}/tests/lists/test_lists_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/manips/test_manips.cpp
    ${PROJECT_SOURCE_DIR}/tests/manips/test_manips_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/misc/test_misc_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/misc/test_proto.cpp
    ${PROJECT_SOURCE_DIR}/tests/misc/test_scene_manager.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodekit_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodekit_traversal.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodes_anim_misc.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodes_extended.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodes_geometry.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodes_misc.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodes_path.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodes_shape_ext.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_nodes_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_procedural_shape.cpp
    ${PROJECT_SOURCE_DIR}/tests/nodes/test_selection_nodes.cpp
    ${PROJECT_SOURCE_DIR}/tests/profiler/test_profiler_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/projectors/test_projectors_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/projectors/test_projectors_suite.cpp
    ${PROJECT_SOURCE_DIR}/tests/sensors/test_sensors_deeper.cpp
    ${PROJECT_SOURCE_DIR}/tests/sensors/test_sensors_extra.cpp
    ${PROJECT_SOURCE_DIR}/tests/sensors/test_sensors_suite.cpp
)

target_sources(obol_unit_tests PRIVATE
    ${OBOL_DOMAIN_UNIT_SOURCES})
target_include_directories(obol_unit_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/utils
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)


# -----------------------------------------------------------------------------
# Rendering cases remain in a reviewable manifest.  A new test is an
# intentional CMake change, every source registers an ordinary GTest case, and
# every enabled GL backend receives the same source list.  The shared headless
# helper selects system GL or OSMesa from the executable's
# OBOL_TEST_RENDER_BACKEND environment.
set(OBOL_RENDER_SCENARIO_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/cad/test_cad_subpixel_proxy.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_arb_shader_parameters.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_arb8_draggers.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_arb8_edit_cycle.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_basic_context.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_bbox_action.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_callback_action.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_callback_action_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_callback_node.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_camera_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_draggers.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_embree.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_engine_converter.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_engine_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_event_callback_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_event_propagation.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_ext_selection.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_ext_selection_events.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_field_connections.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_glrender_action_modes.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_glrender_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_hud_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_hud_nanort.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_image_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_light_manips.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_lod_picking.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_manip_sequences.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_material_binding.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_nanort.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_nanort_shadow.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_nodekit_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_offscreen.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_offscreen_advanced.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_path_operations.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_pick_filter.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_pick_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_procedural_shape.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_quad_mesh_deep.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_quad_viewport.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_quad_viewport_lod.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_raypick_shapes.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_render_manager_full.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_rt_proxy_shapes.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_scene_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_search_action.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_selection_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_sensor_interaction.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_sensors_rendering.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_shader_program.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_shadow_advanced.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_simple_draggers.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_sogl_bindings.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_text3_parts.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_view_volume_ops.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_viewport.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/render_write_read_action.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/stt_iosevka.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/stt_reference.cpp
    ${PROJECT_SOURCE_DIR}/tests/rendering/test_shadow_direct.cpp
)

function(obol_add_render_scenario_sources target)
    target_sources(${target} PRIVATE
        ${OBOL_RENDER_SCENARIO_SOURCES})
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/examples/demo_support
        ${PROJECT_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/tests/testlib
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
    )
    if(EXISTS "${PROJECT_SOURCE_DIR}/fonts/Iosevka/IosevkaAile-Regular.ttc")
        target_compile_definitions(${target} PRIVATE
            OBOL_FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")
    endif()
endfunction()

if(TARGET obol_render_tests)
    obol_add_render_scenario_sources(obol_render_tests)
    target_include_directories(obol_render_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/external/osmesa/include)
    target_compile_definitions(obol_render_tests PRIVATE OBOL_SWRAST_BUILD)
    if(OBOL_USE_SYSTEM_GL)
        target_compile_definitions(obol_render_tests PRIVATE OBOL_DUAL_GL_BUILD)
    endif()
endif()

if(TARGET obol_system_gl_render_tests)
    obol_add_render_scenario_sources(obol_system_gl_render_tests)
endif()
