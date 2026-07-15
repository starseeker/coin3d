# FindOSMesa.cmake
# Locates or builds the project's OSMesa library.
#
# By default the project builds its own OSMesa from the external/osmesa
# submodule (starseeker/osmesa).  A package integrator may instead set
# OBOL_BUILD_BUNDLED_OSMESA=OFF and supply a compatible installation through
# OSMesa_ROOT, OSMESA_ROOT, or CMAKE_PREFIX_PATH.  In either case the OSMesa
# library must be compiled with OSMESA_NAME_MANGLING so all GL symbols are
# prefixed with "mgl" rather than "gl".  This is required for dual-GL builds
# (OBOL_USE_SWRAST + OBOL_USE_SYSTEM_GL) to coexist with system OpenGL.
#
# NOTE: The system libosmesa6-dev package is intentionally NOT used here.
# Its symbols are NOT name-mangled and would collide with system libGL
# in a dual build.
#
# The external/osmesa directory is populated via the git submodule declared
# in .gitmodules, which is automatically initialised at configure time by
# the obol_init_submodule() helper in the root CMakeLists.txt when the bundled
# option is selected.
#
# This will define the following variables:
#
#   OSMesa_FOUND         - True if OSMesa submodule is available
#   OSMesa_LIBRARIES     - Library target(s)
#   OSMesa_INCLUDE_DIRS  - Directories containing OSMesa headers
#
# Interface target:
#   osmesa_interface     - INTERFACE library wrapping the built osmesa

if(OBOL_BUILD_BUNDLED_OSMESA AND
   EXISTS "${PROJECT_SOURCE_DIR}/external/osmesa/CMakeLists.txt")
    message(STATUS "Building OSMesa from external/osmesa submodule")

    # Name-mangling is required for dual-GL builds (mgl* instead of gl*)
    set(OSMESA_NAME_MANGLING ON CACHE BOOL "Enable MGL name mangling for OSMesa")
    set(OSMESA_BUILD_EXAMPLES OFF CACHE BOOL "Don't build OSMesa examples")

    # Add the osmesa subproject
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/osmesa" osmesa_build EXCLUDE_FROM_ALL)

    # OSMesa is added before Obol establishes its global output directories,
    # so set them explicitly to keep the runtime DLL beside Obol executables.
    set_target_properties(osmesa PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(_config ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${_config}" _config_upper)
        set_target_properties(osmesa PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/lib"
            RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/bin")
    endforeach()

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
    # A package manager may provide the same name-mangled OSMesa as a sibling
    # dependency.  Do not accept an arbitrary system libOSMesa here: it exports
    # gl* rather than mgl* and cannot coexist with a system-GL Obol build.
    set(_obol_osmesa_hints)
    foreach(_obol_osmesa_root ${OSMesa_ROOT} ${OSMESA_ROOT})
        if(_obol_osmesa_root)
            list(APPEND _obol_osmesa_hints "${_obol_osmesa_root}")
        endif()
    endforeach()

    find_path(OSMesa_INCLUDE_DIR
        NAMES OSMesa/osmesa.h osmesa.h
        HINTS ${_obol_osmesa_hints}
        PATH_SUFFIXES include include/OSMesa OSMesa)
    find_library(OSMesa_LIBRARY
        NAMES osmesa OSMesa
        HINTS ${_obol_osmesa_hints}
        PATH_SUFFIXES lib)

    if(OSMesa_INCLUDE_DIR AND OSMesa_LIBRARY)
        # The header carries the mgl aliases only in name-mangled builds.
        find_file(OSMesa_MANGLE_HEADER
            NAMES gl_mangle.h
            HINTS "${OSMesa_INCLUDE_DIR}"
            PATH_SUFFIXES OSMesa)
        if(NOT OSMesa_MANGLE_HEADER)
            message(FATAL_ERROR
                "The selected OSMesa does not provide OSMesa/gl_mangle.h.\n"
                "Obol dual-GL builds require the name-mangled starseeker/osmesa variant.")
        endif()

        include(CheckCSourceCompiles)
        set(_obol_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
        set(_obol_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
        set(CMAKE_REQUIRED_INCLUDES "${OSMesa_INCLUDE_DIR}")
        set(CMAKE_REQUIRED_LIBRARIES "${OSMesa_LIBRARY}")
        check_c_source_compiles(
            "#include <OSMesa/gl_mangle.h>\n#include <OSMesa/gl.h>\nint main(void) { glClear(0); return 0; }"
            OSMesa_HAS_NAME_MANGLING)
        set(CMAKE_REQUIRED_INCLUDES "${_obol_saved_required_includes}")
        set(CMAKE_REQUIRED_LIBRARIES "${_obol_saved_required_libraries}")
        unset(_obol_saved_required_includes)
        unset(_obol_saved_required_libraries)
        if(NOT OSMesa_HAS_NAME_MANGLING)
            message(FATAL_ERROR
                "The selected OSMesa does not link a name-mangled mgl* API.\n"
                "Obol dual-GL builds require the name-mangled starseeker/osmesa variant.")
        endif()

        if(NOT TARGET OSMesa::OSMesa)
            add_library(OSMesa::OSMesa UNKNOWN IMPORTED)
            set_target_properties(OSMesa::OSMesa PROPERTIES
                IMPORTED_LOCATION "${OSMesa_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OSMesa_INCLUDE_DIR}")
        endif()
        set(OSMesa_FOUND TRUE)
        set(OSMesa_INCLUDE_DIRS "${OSMesa_INCLUDE_DIR}")
        set(OSMesa_LIBRARIES OSMesa::OSMesa)
        message(STATUS "Using externally supplied name-mangled OSMesa: ${OSMesa_LIBRARY}")
    else()
        set(OSMesa_FOUND FALSE)
        if(OSMesa_FIND_REQUIRED)
            message(FATAL_ERROR
                "No compatible OSMesa was found.\n"
                "Use Obol's external/osmesa submodule (the default), or set "
                "OBOL_BUILD_BUNDLED_OSMESA=OFF with OSMesa_ROOT/OSMESA_ROOT "
                "pointing at a name-mangled starseeker/osmesa installation.")
        else()
            message(STATUS "Compatible OSMesa not found – OSMesa support disabled")
        endif()
    endif()
endif()
