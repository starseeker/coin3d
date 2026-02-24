# Misc tests (PROTO, etc.)
add_executable(test_proto test_proto.cpp)
target_link_libraries(test_proto simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_proto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_proto pthread)
endif()
add_test(NAME test_proto COMMAND test_proto)
