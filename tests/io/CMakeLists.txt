# I/O tests
# Tests for Coin3D SoDB, SoInput/SoOutput, SoBase, SoType read/write.
# Baselined against coin_vanilla COIN_TEST_SUITE blocks.

add_executable(test_sodb test_sodb.cpp)
target_link_libraries(test_sodb simple_test_utils Coin ${COIN_TARGET_LINK_LIBRARIES})
target_include_directories(test_sodb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(test_sodb pthread)
endif()
add_test(NAME test_sodb COMMAND test_sodb)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _IO_DISABLE_TAUTCOMP)
if(_IO_DISABLE_TAUTCOMP)
    target_compile_options(test_sodb PUBLIC -Wno-tautological-compare)
endif()
