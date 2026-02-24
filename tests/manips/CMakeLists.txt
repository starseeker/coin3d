# Manipulator tests
# Tests for Coin3D SoTransformManip subclasses.
# Tier 3 coverage improvement: manips/ (COVERAGE_PLAN.md item 22)

add_executable(test_manips test_manips.cpp)
target_link_libraries(test_manips simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_manips PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_manips pthread)
endif()
add_test(NAME test_manips COMMAND test_manips)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _MANIPS_DISABLE_TAUTCOMP)
if(_MANIPS_DISABLE_TAUTCOMP)
    target_compile_options(test_manips PUBLIC -Wno-tautological-compare)
endif()
