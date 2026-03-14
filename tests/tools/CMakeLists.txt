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
