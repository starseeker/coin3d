# Sensor tests
# Tests for Coin3D sensor classes.
# Note: coin_vanilla has no COIN_TEST_SUITE blocks for sensors; tests here
# are based on documented API behavior and the existing test_sensors.cpp.

add_executable(test_sensors_suite test_sensors_suite.cpp)
target_link_libraries(test_sensors_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sensors_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sensors_suite pthread)
endif()
add_test(NAME test_sensors_suite COMMAND test_sensors_suite)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _SENSORS_DISABLE_TAUTCOMP)
if(_SENSORS_DISABLE_TAUTCOMP)
    target_compile_options(test_sensors_suite PUBLIC -Wno-tautological-compare)
endif()
