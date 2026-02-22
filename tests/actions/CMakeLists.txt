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

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _ACTIONS_DISABLE_TAUTCOMP)
if(_ACTIONS_DISABLE_TAUTCOMP)
    target_compile_options(test_actions_suite PUBLIC -Wno-tautological-compare)
endif()
