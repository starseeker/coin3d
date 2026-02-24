# Event tests
add_executable(test_events_suite test_events_suite.cpp)
target_link_libraries(test_events_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_events_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_events_suite pthread)
endif()
add_test(NAME test_events_suite COMMAND test_events_suite)
