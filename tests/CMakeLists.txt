# Explicit Test Suite for Coin3D
# This directory contains explicit test files to replace the comment-driven test generation

# Find Catch2
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../external/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)
endif()

# Common test utilities
add_library(coin_test_utils STATIC
    utils/test_common.cpp
)
target_link_libraries(coin_test_utils Coin Catch2::Catch2)
target_include_directories(coin_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

# Test executable for all explicit tests
add_executable(CoinExplicitTests
    main.cpp
    # Base module tests
    base/test_base.cpp
    # Field tests
    fields/test_fields.cpp
    # Misc tests  
    misc/test_misc.cpp
)

target_link_libraries(CoinExplicitTests 
    Coin 
    coin_test_utils
    Catch2::Catch2WithMain
    ${COIN_TARGET_LINK_LIBRARIES}
)

target_include_directories(CoinExplicitTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

if (USE_PTHREAD)
    target_link_libraries(CoinExplicitTests pthread)
endif()

# Add individual test registration
include(CTest)
add_test(NAME ExplicitTests COMMAND CoinExplicitTests)

# Enable compiler warnings suppression for tautological comparisons on macOS
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG(-Wtautological-compare DISABLE_WARNING_TAUTOLOGCOMPARE)
if(DISABLE_WARNING_TAUTOLOGCOMPARE)
    target_compile_options(CoinExplicitTests PUBLIC -Wno-tautological-compare)
endif()