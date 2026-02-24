# List tests
# Tests for Coin3D list classes (lists/ subsystem, 75.6% coverage - adding more depth)

add_executable(test_lists_suite test_lists_suite.cpp)
target_link_libraries(test_lists_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_lists_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_lists_suite pthread)
endif()
add_test(NAME test_lists_suite COMMAND test_lists_suite)
