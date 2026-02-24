# Profiler tests
# Tests for Coin3D SoProfiler and SbProfilingData (profiler/ subsystem, 0.8% coverage)
# COVERAGE_PLAN.md item 47.

add_executable(test_profiler_suite test_profiler_suite.cpp)
target_link_libraries(test_profiler_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_profiler_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_profiler_suite pthread)
endif()
add_test(NAME test_profiler_suite COMMAND test_profiler_suite)
