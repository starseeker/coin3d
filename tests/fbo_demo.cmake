# Simple demo to test FBO-based rendering with OSMesa
cmake_minimum_required(VERSION 3.5)

# Add demo executable 
add_executable(fbo_demo
    fbo_demo.cpp
)

target_link_libraries(fbo_demo 
    Coin
    ${COIN_TARGET_LINK_LIBRARIES}
)

target_include_directories(fbo_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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
else()
    message(STATUS "FBO demo: OSMesa library not found - demo may not work")
endif()

# Try to link with GL if available
find_library(GL_LIBRARY NAMES GL opengl32)
if(GL_LIBRARY)
    target_link_libraries(fbo_demo ${GL_LIBRARY})
endif()