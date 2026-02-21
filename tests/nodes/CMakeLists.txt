# Node tests
# Tests for Coin3D SoNode subclasses, baselined against coin_vanilla.

add_executable(test_nodes_suite test_nodes_suite.cpp)
target_link_libraries(test_nodes_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_nodes_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_nodes_suite pthread)
endif()
add_test(NAME test_nodes_suite COMMAND test_nodes_suite)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _NODES_DISABLE_TAUTCOMP)
if(_NODES_DISABLE_TAUTCOMP)
    target_compile_options(test_nodes_suite PUBLIC -Wno-tautological-compare)
endif()
