# Base type tests
# Tests for Coin3D base classes: SbVec*, SbBox*, SbMatrix, SbRotation,
# SbString, SbByteBuffer, SbBSPTree, SbPlane, SbViewVolume, etc.
# Baselined against coin_vanilla COIN_TEST_SUITE blocks.

add_executable(test_sb_types test_sb_types.cpp)
target_link_libraries(test_sb_types simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sb_types PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sb_types pthread)
endif()
add_test(NAME test_sb_types COMMAND test_sb_types)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _BASE_DISABLE_TAUTCOMP)
if(_BASE_DISABLE_TAUTCOMP)
    target_compile_options(test_sb_types PUBLIC -Wno-tautological-compare)
endif()
