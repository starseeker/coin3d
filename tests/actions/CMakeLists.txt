# Action tests
# Tests for Coin3D action classes, baselined against coin_vanilla tests.

add_executable(test_actions_suite test_actions_suite.cpp)
target_link_libraries(test_actions_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_actions_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_actions_suite pthread)
endif()
add_test(NAME test_actions_suite COMMAND test_actions_suite)

add_executable(test_glrender_action test_glrender_action.cpp)
target_link_libraries(test_glrender_action simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_glrender_action PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_glrender_action pthread)
endif()
add_test(NAME test_glrender_action COMMAND test_glrender_action)

add_executable(test_actions_extended test_actions_extended.cpp)
target_link_libraries(test_actions_extended simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_actions_extended PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_actions_extended pthread)
endif()
add_test(NAME test_actions_extended COMMAND test_actions_extended)

add_executable(test_actions_pick test_actions_pick.cpp)
target_link_libraries(test_actions_pick simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_actions_pick PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_actions_pick pthread)
endif()
add_test(NAME test_actions_pick COMMAND test_actions_pick)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _ACTIONS_DISABLE_TAUTCOMP)
if(_ACTIONS_DISABLE_TAUTCOMP)
    target_compile_options(test_actions_suite PUBLIC -Wno-tautological-compare)
    target_compile_options(test_glrender_action PUBLIC -Wno-tautological-compare)
    target_compile_options(test_actions_extended PUBLIC -Wno-tautological-compare)
    target_compile_options(test_actions_pick PUBLIC -Wno-tautological-compare)
endif()
