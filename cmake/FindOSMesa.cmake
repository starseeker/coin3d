# FindOSMesa.cmake
# Builds the starseeker/osmesa library (Off-Screen Mesa) from submodule
#
# This will define the following variables:
#
#   OSMesa_FOUND         - True if OSMesa is found
#   OSMesa_LIBRARIES     - List of libraries when using OSMesa  
#   OSMesa_INCLUDE_DIRS  - The directories containing OSMesa headers
#
# This will define the following imported targets:
#
#   OSMesa::OSMesa

# Always use the starseeker/osmesa submodule
if(EXISTS "${PROJECT_SOURCE_DIR}/external/osmesa/CMakeLists.txt")
    message(STATUS "Using starseeker/osmesa submodule")
    
    # Set OSMesa options for coin3d compatibility 
    set(OSMESA_NAME_MANGLING ON CACHE BOOL "Enable MGL name mangling for OSMesa")
    set(OSMESA_BUILD_EXAMPLES OFF CACHE BOOL "Don't build OSMesa examples")
    
    # Add the osmesa subproject
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/osmesa" osmesa_build EXCLUDE_FROM_ALL)
    
    # Set up the variables for the submodule build
    set(OSMesa_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/external/osmesa/include")
    set(OSMesa_LIBRARY osmesa)  # This will be the target name from the submodule
    set(OSMesa_FOUND TRUE)
    set(OSMesa_LIBRARIES ${OSMesa_LIBRARY})
    set(OSMesa_INCLUDE_DIRS ${OSMesa_INCLUDE_DIR})
    
    # Don't create an alias since we'll use the osmesa target directly
    # The main CMakeLists.txt will handle the target vs libraries case
else()
    # Submodule is required when OSMesa is enabled
    message(FATAL_ERROR "starseeker/osmesa submodule not found at external/osmesa. Please run 'git submodule update --init --recursive'")
endif()