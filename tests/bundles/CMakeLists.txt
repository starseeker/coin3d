# Bundle tests (SoNormalBundle, SoMaterialBundle, SoVertexAttributeBundle)
# Covers: bundles/ subsystem (COVERAGE_PLAN.md item 56)

add_executable(test_bundles_suite test_bundles_suite.cpp)
target_link_libraries(test_bundles_suite simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_bundles_suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_bundles_suite pthread)
endif()
add_test(NAME test_bundles_suite COMMAND test_bundles_suite)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _BUNDLES_DISABLE_TAUTCOMP)
if(_BUNDLES_DISABLE_TAUTCOMP)
    target_compile_options(test_bundles_suite PUBLIC -Wno-tautological-compare)
endif()
