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

add_executable(test_misc_suite test_misc_suite.cpp)
target_link_libraries(test_misc_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_misc_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_misc_suite pthread)
endif()
add_test(NAME test_misc_suite COMMAND test_misc_suite)

add_executable(test_scene_manager test_scene_manager.cpp)
target_link_libraries(test_scene_manager simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_scene_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_scene_manager pthread)
endif()
add_test(NAME test_scene_manager COMMAND test_scene_manager)
