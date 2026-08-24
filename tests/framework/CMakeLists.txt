# =============================================================================
# Modern Obol test framework
#
# This is deliberately independent of the legacy TestRegistry / ObolEx layer.
# GTest supplies normal assertions, fixtures, filtering and CTest discovery;
# obol_test_support supplies only Obol-specific lifecycle and rendering helpers.
# =============================================================================

find_package(GTest 1.10 REQUIRED)
include(GoogleTest)

add_library(obol_test_support STATIC
    image_assertions.cpp
    test_context.cpp
    render_fixture.cpp
)
target_link_libraries(obol_test_support
    PUBLIC
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        lodepng
)
target_include_directories(obol_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)

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
