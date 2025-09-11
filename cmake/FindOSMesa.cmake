# FindOSMesa.cmake
# Finds or builds the starseeker/osmesa library (Off-Screen Mesa)
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

# Try to use the starseeker/osmesa submodule first
if(EXISTS "${PROJECT_SOURCE_DIR}/osmesa/CMakeLists.txt")
    message(STATUS "Using starseeker/osmesa submodule")
    
    # Set OSMesa options for coin3d compatibility 
    set(OSMESA_NAME_MANGLING ON CACHE BOOL "Enable MGL name mangling for OSMesa")
    set(OSMESA_BUILD_EXAMPLES OFF CACHE BOOL "Don't build OSMesa examples")
    
    # Add the osmesa subproject
    add_subdirectory("${PROJECT_SOURCE_DIR}/osmesa" osmesa_build EXCLUDE_FROM_ALL)
    
    # Set up the variables for the submodule build
    set(OSMesa_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/osmesa/include")
    set(OSMesa_LIBRARY osmesa)  # This will be the target name from the submodule
    set(OSMesa_FOUND TRUE)
    set(OSMesa_LIBRARIES ${OSMesa_LIBRARY})
    set(OSMesa_INCLUDE_DIRS ${OSMesa_INCLUDE_DIR})
    
    # Don't create an alias since we'll use the osmesa target directly
    # The main CMakeLists.txt will handle the target vs libraries case
else()
    # Fallback to system OSMesa (original behavior)
    message(STATUS "starseeker/osmesa submodule not found, looking for system OSMesa")
    
    find_path(OSMesa_INCLUDE_DIR
        NAMES GL/osmesa.h
        HINTS
            ${OSMesa_ROOT_DIR}
            $ENV{OSMesa_ROOT_DIR}
        PATH_SUFFIXES include
        DOC "OSMesa include directory"
    )

    find_library(OSMesa_LIBRARY
        NAMES OSMesa osmesa
        HINTS
            ${OSMesa_ROOT_DIR}
            $ENV{OSMesa_ROOT_DIR}
        PATH_SUFFIXES lib lib64
        DOC "OSMesa library"
    )

    # Handle the QUIETLY and REQUIRED arguments and set OSMesa_FOUND to TRUE if
    # all listed variables are TRUE
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(OSMesa
        REQUIRED_VARS
            OSMesa_LIBRARY
            OSMesa_INCLUDE_DIR
    )

    if(OSMesa_FOUND)
        set(OSMesa_LIBRARIES ${OSMesa_LIBRARY})
        set(OSMesa_INCLUDE_DIRS ${OSMesa_INCLUDE_DIR})

        if(NOT TARGET OSMesa::OSMesa)
            add_library(OSMesa::OSMesa UNKNOWN IMPORTED)
            set_target_properties(OSMesa::OSMesa PROPERTIES
                IMPORTED_LOCATION "${OSMesa_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${OSMesa_INCLUDE_DIR}"
            )
        endif()
    endif()

    mark_as_advanced(
        OSMesa_INCLUDE_DIR
        OSMesa_LIBRARY
    )
endif()