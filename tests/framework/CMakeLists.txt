# =============================================================================
# Modern Obol test framework
#
# This is deliberately independent of the removed registry / ObolEx layer.
# GTest supplies normal assertions, fixtures, filtering and CTest discovery;
# obol_test_support supplies only Obol-specific lifecycle and rendering helpers.
# =============================================================================

find_package(GTest 1.10 REQUIRED)
include(GoogleTest)

add_library(obol_test_support STATIC
    image_assertions.cpp
    stb_image_write_impl.cpp
    ${PROJECT_SOURCE_DIR}/tests/test_utils.cpp
    test_context.cpp
    render_fixture.cpp
)
if(OBOL_USE_SWRAST)
    # The scene catalogue is shared by fixture-backed render contracts.  Keep
    # it in the modern support target so converted cases do not need a second
    # renderer-specific library.
    target_sources(obol_test_support PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/testlib/test_scenes.cpp
    )
endif()
target_link_libraries(obol_test_support
    PUBLIC
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        GTest::gtest
        lodepng
)
target_include_directories(obol_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/tests/testlib
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_SOURCE_DIR}/external/nanort
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
    cmake_parse_arguments(OBOL_GTEST "" "TIMEOUT" "" ${ARGN})
    if(OBOL_GTEST_TIMEOUT)
        set(_obol_gtest_timeout ${OBOL_GTEST_TIMEOUT})
    else()
        set(_obol_gtest_timeout 30)
    endif()
    add_executable(${target} test_main.cpp ${OBOL_GTEST_UNPARSED_ARGUMENTS})
    target_link_libraries(${target}
        PRIVATE
            obol_test_support
            GTest::gtest
    )
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
    )
    gtest_discover_tests(${target}
        DISCOVERY_MODE PRE_TEST
        PROPERTIES
            LABELS "${label}"
            TIMEOUT ${_obol_gtest_timeout}
    )
endfunction()

obol_add_gtest(obol_unit_tests "unit"
    ${PROJECT_SOURCE_DIR}/tests/unit/base_math_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/base_geometry_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/base_utilities_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/base_value_types_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/draggers_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/engines_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/events_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/fields_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/image_assertions_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/image_fields_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/interaction_kit_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/nodes_test.cpp
    ${PROJECT_SOURCE_DIR}/tests/unit/projectors_test.cpp
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

# OSMesa is the portable deterministic raster backend for visual conformance
# tests.  System-GL tests remain a separate optional backend lane.
if(OBOL_USE_SWRAST)
    # These cover the former primitives/materials/transforms image wrappers
    # with feature-level contracts rather than backend-specific golden files.
    obol_add_gtest(obol_render_tests "render;osmesa"
        ${PROJECT_SOURCE_DIR}/tests/rendering/render_fixture_smoke_test.cpp
        ${PROJECT_SOURCE_DIR}/tests/rendering/offscreen_renderer_api_test.cpp
        ${PROJECT_SOURCE_DIR}/tests/rendering/render_semantic_contracts_test.cpp
        ${PROJECT_SOURCE_DIR}/tests/rendering/interaction_contracts_test.cpp
        ${PROJECT_SOURCE_DIR}/tests/rendering/portable_feature_contracts_test.cpp
        ${PROJECT_SOURCE_DIR}/tests/rendering/demo_catalogue_test.cpp
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
    target_include_directories(obol_render_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/examples/demo_scenes
    )
else()
    message(STATUS "Modern render tests: skipped (OBOL_USE_SWRAST=OFF)")
endif()

option(OBOL_BUILD_STRESS_TESTS
    "Build opt-in stress and sanitizer-oriented tests"
    OFF)
if(OBOL_BUILD_STRESS_TESTS)
    obol_add_gtest(obol_stress_tests "stress;threads" TIMEOUT 120
        ${PROJECT_SOURCE_DIR}/tests/threads/test_thread_safety.cpp
    )
endif()

# -----------------------------------------------------------------------------
# Upstream-derived unit cases
# -----------------------------------------------------------------------------
# These cases retain their useful, broad API coverage, but each source now
# registers a GTest case directly.  They share obol_test_support with the
# focused unit tests; no generated main() wrapper or second runner is needed.
set(OBOL_UPSTREAM_TEST_SOURCES
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
    ${OBOL_UPSTREAM_TEST_SOURCES})
target_include_directories(obol_unit_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/utils
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)


# -----------------------------------------------------------------------------
# Every rendering source is now a normal GTest translation unit.  Keep the
# source list explicit for the modern cases above, then add the converted
# upstream-style cases to the same executable and discovery stream.  There are
# no per-source executables or generated main() adapters left in this lane.
if(OBOL_USE_SWRAST)
    # Keep converted rendering cases in a reviewable manifest.  A new test is
    # intentionally a CMake change, so a source file cannot silently enter a
    # build just because it appeared in the directory.
    set(OBOL_UPSTREAM_RENDER_SOURCES
        ${PROJECT_SOURCE_DIR}/tests/rendering/nogl_scenegraph_test.cpp
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
    target_sources(obol_render_tests PRIVATE
        ${OBOL_UPSTREAM_RENDER_SOURCES})
    target_include_directories(obol_render_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/tests/testlib
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_SOURCE_DIR}/external/nanort
        ${PROJECT_SOURCE_DIR}/external/nanort/examples/common
        ${PROJECT_SOURCE_DIR}/external/osmesa/include)
    target_compile_definitions(obol_render_tests PRIVATE OBOL_SWRAST_BUILD)
    if(OBOL_USE_SYSTEM_GL)
        target_compile_definitions(obol_render_tests PRIVATE OBOL_DUAL_GL_BUILD)
    endif()
    if(EXISTS "${PROJECT_SOURCE_DIR}/fonts/Iosevka/IosevkaAile-Regular.ttc")
        target_compile_definitions(obol_render_tests PRIVATE
            OBOL_FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")
    endif()
endif()
