# FBO demo executable - integrated with test framework
cmake_minimum_required(VERSION 3.10)

# Add demo executable 
add_executable(fbo_demo
    fbo_demo.cpp
)

target_link_libraries(fbo_demo 
    Coin
    coin_test_utils  # Link against shared test utilities for PNG functionality
    ${COIN_TARGET_LINK_LIBRARIES}
)

target_include_directories(fbo_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/utils  # Include test utilities for shared PNG functions
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/osmesa/include
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
)

# Try to link with OSMesa if available  
find_library(OSMESA_LIBRARY NAMES OSMesa osmesa PATHS ${PROJECT_SOURCE_DIR}/osmesa/lib NO_DEFAULT_PATH)
if(OSMESA_LIBRARY)
    target_link_libraries(fbo_demo ${OSMESA_LIBRARY})
    message(STATUS "FBO demo: Found OSMesa library: ${OSMESA_LIBRARY}")
    target_compile_definitions(fbo_demo PRIVATE COIN3D_OSMESA_BUILD)
else()
    message(STATUS "FBO demo: Using built-in OSMesa from submodule (library path not required)")
    # Note: With COIN3D_USE_OSMESA=ON, OSMesa is built as part of the main library
    # The demo will work with the integrated OSMesa functionality
endif()

# Try to link with GL if available
find_library(GL_LIBRARY NAMES GL opengl32)
if(GL_LIBRARY)
    target_link_libraries(fbo_demo ${GL_LIBRARY})
endif()