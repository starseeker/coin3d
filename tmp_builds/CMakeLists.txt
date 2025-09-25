cmake_minimum_required(VERSION 3.16)
project(TextureTests)

# Use the same settings as Coin3D build
set(CMAKE_CXX_STANDARD 17)

# Find the built Coin3D library
find_library(COIN_LIBRARY NAMES Coin-osmesa HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../build_dir/lib)
find_library(OSMESA_LIBRARY NAMES osmesa HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../build_dir/osmesa_build/src)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../build_dir/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../external/osmesa/include
)

# Texture crash test
add_executable(texture_crash_test ${CMAKE_CURRENT_SOURCE_DIR}/../texture_crash_test.cpp)
target_link_libraries(texture_crash_test ${COIN_LIBRARY} ${OSMESA_LIBRARY} m pthread)

# Actual texture rendering test
add_executable(texture_test_actual ${CMAKE_CURRENT_SOURCE_DIR}/../texture_test_actual.cpp)
target_link_libraries(texture_test_actual ${COIN_LIBRARY} ${OSMESA_LIBRARY} m pthread)

# Debug info
message(STATUS "COIN_LIBRARY: ${COIN_LIBRARY}")
message(STATUS "OSMESA_LIBRARY: ${OSMESA_LIBRARY}")