# Threading tests
# Tests for Coin threading primitives: SbMutex, SbThreadMutex, SbCondVar,
# SbRWMutex, SbThread, SbBarrier, SbFifo, SbStorage, SbTypedStorage.
#
# Only built when threading support is available.

if(NOT COIN_THREADSAFE)
    message(STATUS "Threading tests skipped (COIN_THREADSAFE not set)")
    return()
endif()

add_executable(test_threads test_threads.cpp)

target_link_libraries(test_threads
    simple_test_utils
    Coin
    ${COIN_TARGET_LINK_LIBRARIES}
)

target_include_directories(test_threads PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

if(USE_PTHREAD)
    target_link_libraries(test_threads pthread)
endif()

add_test(NAME test_threads COMMAND test_threads)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare _DISABLE_TAUTCOMP)
if(_DISABLE_TAUTCOMP)
    target_compile_options(test_threads PUBLIC -Wno-tautological-compare)
endif()
