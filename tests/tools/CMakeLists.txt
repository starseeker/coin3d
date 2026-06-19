# =============================================================================
# tests/tools/CMakeLists.txt
#
# Unit tests for the SbModernUtils (tools/ subsystem) and other
# tool/utility modules.
# =============================================================================

# test_tools_suite: tests for SbModernUtils C++17 utilities
add_executable(test_tools_suite test_tools_suite.cpp)
target_link_libraries(test_tools_suite
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_tools_suite PRIVATE
    ${CMAKE_SOURCE_DIR}/tests          # test_utils.h
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
add_test(NAME test_tools_suite COMMAND test_tools_suite)

# test_v2_scene_suite: v2 scene/render facade smoke tests
add_executable(test_v2_scene_suite test_v2_scene_suite.cpp)
target_link_libraries(test_v2_scene_suite
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_v2_scene_suite PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
add_test(NAME test_v2_scene_suite COMMAND test_v2_scene_suite)

add_test(
    NAME test_v2_example_render_api
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_v2_example_render_api.cmake
)

add_test(
    NAME test_v2_public_headers
    COMMAND ${CMAKE_COMMAND}
        -DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_v2_public_headers.cmake
)
