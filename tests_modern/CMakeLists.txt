# Modern test suite for Coin3D
# Simplified and streamlined testing approach

# Find or build Catch2
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    # Use bundled Catch2 
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../external/Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2)
endif()

# Test utilities library
add_library(coin_test_utils STATIC utils/test_common.cpp)
target_link_libraries(coin_test_utils 
    PUBLIC 
        Coin
        Catch2::Catch2
)
target_include_directories(coin_test_utils PUBLIC utils)

# Main test executable with all test files
file(GLOB_RECURSE TEST_SOURCES
    "base/*.cpp"
    "fields/*.cpp" 
    "misc/*.cpp"
    "tools/*.cpp"
)

# Optional test modules
if(HAVE_DRAGGERS)
    file(GLOB DRAGGER_TESTS "draggers/*.cpp")
    list(APPEND TEST_SOURCES ${DRAGGER_TESTS})
endif()

add_executable(CoinTests main.cpp ${TEST_SOURCES})

target_link_libraries(CoinTests
    PRIVATE
        Coin
        coin_test_utils
        Catch2::Catch2WithMain
)

# Configure include directories and compiler flags
target_include_directories(CoinTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Platform-specific test configuration
if(APPLE)
    target_compile_options(CoinTests PRIVATE -Wno-tautological-compare)
endif()

# Register tests with CTest
include(CTest)
add_test(NAME CoinTests COMMAND CoinTests)

# Optional: Legacy threading tests (if they exist)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../testsuite)
    add_subdirectory(../testsuite ${CMAKE_CURRENT_BINARY_DIR}/legacy_tests)
endif()

message(STATUS "Configured Coin3D test suite")