# FindOSMesa.cmake
# Locates or builds the project's OSMesa library.
#
# The project builds its own OSMesa from the external/osmesa submodule
# (starseeker/osmesa).  This version is compiled with OSMESA_NAME_MANGLING
# so all GL symbols are prefixed with "mgl" rather than "gl", which is
# required for OBOL_BUILD_DUAL_GL to coexist with system OpenGL in the
# same shared library without symbol conflicts.
#
# NOTE: The system libosmesa6-dev package is intentionally NOT used here.
# Its symbols are NOT name-mangled and would collide with system libGL
# in a dual build.
#
# If osmesa is not found (submodule missing / not initialised), the caller
# can still configure a system-OpenGL-only build by setting
# OBOL_USE_SYSTEM_ONLY=ON (or by not enabling OSMesa support at all).
#
# This will define the following variables:
#
#   OSMesa_FOUND         - True if OSMesa submodule is available
#   OSMesa_LIBRARIES     - Library target(s)
#   OSMesa_INCLUDE_DIRS  - Directories containing OSMesa headers
#
# Interface target:
#   osmesa_interface     - INTERFACE library wrapping the built osmesa

if(EXISTS "${PROJECT_SOURCE_DIR}/external/osmesa/CMakeLists.txt")
    message(STATUS "Building OSMesa from external/osmesa submodule")

    # Name-mangling is required for dual-GL builds (mgl* instead of gl*)
    set(OSMESA_NAME_MANGLING ON CACHE BOOL "Enable MGL name mangling for OSMesa")
    set(OSMESA_BUILD_EXAMPLES OFF CACHE BOOL "Don't build OSMesa examples")

    # Add the osmesa subproject
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/osmesa" osmesa_build EXCLUDE_FROM_ALL)

    # Create an interface wrapper to avoid export issues
    if(NOT TARGET osmesa_interface)
        add_library(osmesa_interface INTERFACE)
        target_link_libraries(osmesa_interface INTERFACE osmesa)
    endif()

    set(OSMesa_INCLUDE_DIR  "${PROJECT_SOURCE_DIR}/external/osmesa/include")
    set(OSMesa_LIBRARY      osmesa_interface)
    set(OSMesa_FOUND        TRUE)
    set(OSMesa_LIBRARIES    ${OSMesa_LIBRARY})
    set(OSMesa_INCLUDE_DIRS ${OSMesa_INCLUDE_DIR})

else()
    set(OSMesa_FOUND FALSE)
    if(OSMesa_FIND_REQUIRED)
        message(FATAL_ERROR
            "OSMesa submodule not found at external/osmesa.\n"
            "Please run: git submodule update --init --recursive\n"
            "The project requires its own name-mangled OSMesa build; "
            "the system libosmesa6-dev package is not compatible.")
    else()
        message(STATUS "OSMesa submodule not found at external/osmesa – OSMesa support disabled")
    endif()
endif()