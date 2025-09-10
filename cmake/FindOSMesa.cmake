# FindOSMesa.cmake
# Finds the OSMesa library (Off-Screen Mesa)
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