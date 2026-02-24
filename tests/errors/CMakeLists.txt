# Error handling tests
add_executable(test_errors_suite test_errors_suite.cpp)
target_link_libraries(test_errors_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_errors_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_errors_suite pthread)
endif()
add_test(NAME test_errors_suite COMMAND test_errors_suite)
