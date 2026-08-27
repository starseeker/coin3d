# ============================================================================
# Configuration file generation
# ============================================================================

# Two-stage configuration for proper generator expression handling
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h_at_configured"
    @ONLY
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/config.h_at_configured"
)

# Each source directory owns an object-library target.  The old build appended
# every file to one global source property, which made module ownership and
# dependency boundaries invisible to CMake.  Object libraries preserve the
# single installed Obol library while keeping each module's source list local.
add_library(obol_internal_config INTERFACE)
target_compile_definitions(obol_internal_config INTERFACE
    HAVE_CONFIG_H
    OBOL_INTERNAL
    OBOL_DEBUG=$<CONFIG:Debug>
    UTF8_NO_AUTOLINK
    UNICODE
    _UNICODE
)
if(OBOL_PROFILING)
  target_compile_definitions(obol_internal_config INTERFACE OBOL_PROFILING)
endif()
if(OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
  target_compile_definitions(obol_internal_config INTERFACE USE_MGL_NAMESPACE)
endif()
if(OBOL_WARNING_FLAGS)
  target_compile_options(obol_internal_config INTERFACE ${OBOL_WARNING_FLAGS})
endif()
if(MSVC)
  target_compile_options(obol_internal_config INTERFACE /utf-8)
endif()
target_include_directories(obol_internal_config INTERFACE
    "${PROJECT_BINARY_DIR}/include"
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/misc"
    "${PROJECT_SOURCE_DIR}/include/Inventor/annex"
    "${PROJECT_BINARY_DIR}/src/shadows"
    "${PROJECT_BINARY_DIR}/src/draggers"
    "${PROJECT_SOURCE_DIR}/src/base/utf8/include"
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)

function(obol_add_source_module module)
  if(NOT ARGN)
    message(FATAL_ERROR "Obol source module '${module}' has no sources")
  endif()
  set(_target "obol_${module}_objects")
  if(TARGET ${_target})
    message(FATAL_ERROR "Obol source module target '${_target}' was declared twice")
  endif()
  add_library(${_target} OBJECT ${ARGN})
  target_link_libraries(${_target} PRIVATE obol_internal_config)
  set_target_properties(${_target} PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
      POSITION_INDEPENDENT_CODE ON
  )
  if(OBOL_USE_UNITY_BUILD)
    set_target_properties(${_target} PROPERTIES UNITY_BUILD ON)
  endif()
endfunction()

foreach(_subdir actions base bundles caches details draggers elements
                engines errors events fields fonts glue io manips misc rendering lists
                nodes shapenodes projectors sensors shadows threads shaders tools
                hardcopy nodekits profiler hud)
  add_subdirectory(${_subdir})
endforeach()
add_subdirectory(cad)

set(OBOL_SOURCE_MODULE_TARGETS)
foreach(_subdir actions base bundles caches details draggers elements
                engines errors events fields fonts glue io manips misc rendering lists
                nodes shapenodes projectors sensors shadows threads shaders tools
                hardcopy nodekits profiler hud cad)
  if(TARGET obol_${_subdir}_objects)
    list(APPEND OBOL_SOURCE_MODULE_TARGETS obol_${_subdir}_objects)
  endif()
endforeach()
if(TARGET obol_elements_gl_objects)
  list(APPEND OBOL_SOURCE_MODULE_TARGETS obol_elements_gl_objects)
endif()
if(NOT OBOL_SOURCE_MODULE_TARGETS)
  message(FATAL_ERROR "No Obol source modules were registered")
endif()

# Keep the high-responsibility CAD assembly and its direct software fallback
# independently compiled.  Source properties are directory scoped, so set
# this where the Obol target itself is created rather than relying only on
# the cad subdirectory's declaration.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/cad/SoCADAssembly.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cad/CadSoftwareWire.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
)

# As CMake does not support deep header hierarchies for framework generation we tweak the
# installation paths for the install commands.  To make sure the symbolic links for versioned
# Binaries, Headers, and Resources are properly created we add at headers and resources to
# the target.

# ============================================================================
# Library target creation
# ============================================================================

# Additional headers to include in target
set(OBOL_ADDITIONAL_HDRS 
    ${PROJECT_BINARY_DIR}/src/config.h
)

set(_obol_object_sources)
foreach(_module_target ${OBOL_SOURCE_MODULE_TARGETS})
    list(APPEND _obol_object_sources "$<TARGET_OBJECTS:${_module_target}>")
endforeach()
if(TARGET obol_lodepng_objects)
    list(APPEND _obol_object_sources "$<TARGET_OBJECTS:obol_lodepng_objects>")
endif()
add_library(${PROJECT_NAME} ${_obol_object_sources} ${OBOL_ADDITIONAL_HDRS})
if(TARGET obol_parser_gen)
  add_dependencies(${PROJECT_NAME} obol_parser_gen)
endif()

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Several supported extension points (notably the GL dispatch wrappers used
# by Inventor/gl.h) retain their historical declarations in private headers.
# Export all compiled symbols on Windows so those established entry points
# are present in the import library for shared builds as they are for static
# builds.
if(WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    if(MSVC)
        # Auto-export necessarily overlaps symbols already marked dllexport.
        # LINK uses the first identical specification, so silence only that
        # expected duplicate-export diagnostic.
        target_link_options(${PROJECT_NAME} PRIVATE /IGNORE:4197)
    endif()
endif()

# Set library output name based on backend selection
if(DEFINED OBOL_OUTPUT_NAME)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${OBOL_OUTPUT_NAME})
endif()

# ============================================================================
# Unity Build Support
# ============================================================================

# Enable unity build when requested.  CMake 3.16 is the project minimum.
if(OBOL_USE_UNITY_BUILD)
    message(STATUS "Enabling unity build for faster compilation")
    set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
endif()

# ============================================================================
# Target-specific compile definitions and properties
# ============================================================================

target_compile_definitions(${PROJECT_NAME} PRIVATE
    # In swrast-only builds, all gl* calls in the library must be mangled to
    # mgl* so they resolve to the OSMesa library symbols.  This is scoped to
    # the library target so that test executables (which may include <GL/glx.h>)
    # are not affected — <GL/glx.h> conditionally includes "glx_mangle.h" when
    # USE_MGL_NAMESPACE is defined, and that file does not exist on Ubuntu 24.04.
    $<$<BOOL:${OBOL_USE_SWRAST}>:$<$<NOT:$<BOOL:${OBOL_USE_SYSTEM_GL}>>:USE_MGL_NAMESPACE>>
)

if(OBOL_PROFILING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE OBOL_PROFILING)
endif()

# Apply warning flags (controlled by OBOL_WARNINGS option in the top-level CMakeLists.txt)
if(OBOL_WARNING_FLAGS)
    target_compile_options(${PROJECT_NAME} PRIVATE ${OBOL_WARNING_FLAGS})
endif()
target_compile_options(${PROJECT_NAME} PRIVATE
    "$<$<C_COMPILER_ID:MSVC>:/utf-8>"
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
)

# Target-specific include directories (modern approach)
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        "${PROJECT_BINARY_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/misc"
        "${PROJECT_SOURCE_DIR}/include/Inventor/annex"
        "${PROJECT_BINARY_DIR}/src/shadows"
        # Subdirectory-specific paths (needed in single-lib mode)
        "${PROJECT_SOURCE_DIR}/src/base/utf8/include"
        "${PROJECT_BINARY_DIR}/src/draggers"
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    INTERFACE
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Obol/render>
)

# Target-specific linking (modern approach)  
target_link_libraries(${PROJECT_NAME}
    PRIVATE ${OBOL_TARGET_LINK_LIBRARIES}
    PUBLIC Threads::Threads)

# For dual-GL builds, link OSMesa so osmesa_SoGLContext_* functions can resolve
# mgl* symbols at runtime.  A shared Obol carries that runtime dependency
# itself; exposing it would make an externally supplied OSMesa target leak
# into the installed package interface.
if(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
    if(OBOL_BUILD_SHARED_LIBS)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OBOL_OSMESA_LIBRARIES})
    else()
        target_link_libraries(${PROJECT_NAME} PUBLIC ${OBOL_OSMESA_LIBRARIES})
    endif()
endif()

if(WIN32)
  if(OBOL_BUILD_SHARED_LIBS)
    configure_file(${PROJECT_NAME}.rc.cmake.in ${PROJECT_NAME}.rc)
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc")
    target_compile_definitions(${PROJECT_NAME} INTERFACE OBOL_DLL)
    if(MINGW)
      set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_NAME ${PROJECT_NAME}-${OBOL_SO_VERSION})
    endif()
  else()
    target_compile_definitions(${PROJECT_NAME} INTERFACE OBOL_NOT_DLL)
    if(MSVC)
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Z7")                       # Override default /Zi to embed
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Z7")     # debugging info in the .lib.
    endif()
  endif()
elseif(APPLE)
  if(OBOL_BUILD_MAC_FRAMEWORK)
    # Set the framework target properties just to make sure the relevant links inside the framework 
    # are created.  Because the default framework name differs from the project name we change the
    # output name property.
    set_target_properties(${PROJECT_NAME} PROPERTIES
      FRAMEWORK TRUE
      FRAMEWORK_VERSION ${OBOL_MAC_FRAMEWORK_VERSION}
      #PUBLIC_HEADER "${OBOL_PUBLIC_HEADERS}" # does not work recursively
      #PRIVATE_HEADER "${OBOL_PRIVATE_HEADERS}" # does not work recursively
      RESOURCE "${CMAKE_BINARY_DIR}/version.plist" # does not work recursively
      MACOSX_FRAMEWORK_IDENTIFIER org.coin3d.Coin.framework
      MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
      OUTPUT_NAME ${OBOL_MAC_FRAMEWORK_NAME}
    )
    # adapt install directory to allow distribution of dylibs/frameworks in users
    # framework/application bundle
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_NAME_DIR "@rpath")
    if(NOT CMAKE_SKIP_BUILD_RPATH)
      if(CMAKE_VERSION VERSION_LESS 3.9)
        set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
      else()
        set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_WITH_INSTALL_NAME_DIR TRUE)
      endif()
    endif()
  endif()
endif()

# ============================================================================
# Platform-specific configuration
# ============================================================================

# The bundled software renderer is installed beside Obol.  Keep that
# relationship relocatable so consumers do not need LD_LIBRARY_PATH merely to
# load a dual-backend Obol installation.
if(UNIX AND NOT APPLE AND OBOL_USE_SWRAST)
  # Obol and its bundled software renderer are published as a co-located
  # pair.  Use the install rpath for the build artifact too: downstream
  # projects may copy the shared library into their own build lib directory,
  # where the producer's absolute build rpath would otherwise make the loader
  # select a stale external libosmesa before the consumer's copy.
  set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN")
endif()

# Set library version properties
if(WIN32 AND NOT MINGW)
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        VERSION ${PROJECT_VERSION} 
        SOVERSION ${OBOL_SO_VERSION}
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        VERSION "${OBOL_SO_VERSION}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}" 
        SOVERSION ${OBOL_SO_VERSION}
    )
endif()

# Setting up a framework means adding all the files and links under a framework
# directory. A single install command then moves the entire contents of the
# framework directory to the OBOL_MAC_FRAMEWORK_PREFIX directory (/Library/Frameworks
# by default)
# As the PUBLIC_HEADER and RESOURCE options of install target do not support
# directory structure creation when building a framework we set the MACOSX_PACKAGE_LOCATION
# property on the source files and add them to the target. This does however not work
# for the generated documentation files.
# All non standard links must be created manually.
if(OBOL_BUILD_MAC_FRAMEWORK)

# ############################################################################
# Add framework headers
# ############################################################################
  coin_set_macosx_properties("${PROJECT_SOURCE_DIR}/include/Inventor;${CMAKE_BINARY_DIR}/include/Inventor" "Headers" "${COMMON_HDRS}")

# ############################################################################
# Add framework data files
# ############################################################################

# ############################################################################
# Add framework documentation files
# ############################################################################

# ############################################################################
# Add framework links
# ############################################################################

  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/Libraries
    COMMAND ${CMAKE_COMMAND} -E create_symlink ./Versions/Current/Libraries $<TARGET_FILE_DIR:${PROJECT_NAME}>/../../Libraries
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../${OBOL_MAC_FRAMEWORK_NAME} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Libraries/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${OBOL_SO_VERSION}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}${CMAKE_SHARED_LIBRARY_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../${OBOL_MAC_FRAMEWORK_NAME} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Libraries/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${OBOL_SO_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../${OBOL_MAC_FRAMEWORK_NAME} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Libraries/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources/include
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../../Headers $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources/include/Inventor
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../../../../SoQt.framework/Headers $<TARGET_FILE_DIR:${PROJECT_NAME}>/Headers/Qt
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../../../../SoXt.framework/Headers $<TARGET_FILE_DIR:${PROJECT_NAME}>/Headers/Xt
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

# ############################################################################
# Install framework target
# ############################################################################

  install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
    FRAMEWORK DESTINATION . COMPONENT development
  )

else()

# ############################################################################
# Install headers
# ############################################################################

  # Install all public headers
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/Inventor/ 
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Inventor 
          COMPONENT development 
          FILES_MATCHING PATTERN "*.h"
          # Exclude specific internal C headers that are no longer part of public API
          PATTERN "C/glue/dl.h" EXCLUDE
          PATTERN "C/basic.h.in" EXCLUDE
          PATTERN "C/basic.h.cmake.in" EXCLUDE)
  install(DIRECTORY ${CMAKE_BINARY_DIR}/include/Inventor/ 
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Inventor 
          COMPONENT development 
          FILES_MATCHING PATTERN "*.h")

  # Install obol-specific public headers (e.g. SoCAD* interfaces)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/Obol/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Obol
          COMPONENT development
          FILES_MATCHING PATTERN "*.h")

# ############################################################################
# Install target
# ############################################################################

  install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime NAMELINK_COMPONENT development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT development
    RESOURCE DESTINATION ${CMAKE_INSTALL_DATADIR} COMPONENT development
  )
  if(OBOL_BUILD_SHARED_LIBS AND MSVC)
    install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Debug RelWithDebInfo COMPONENT debug)
  endif()

  # When using the bundled OSMesa submodule (DUAL_GL or osmesa-only builds),
  # install its shared library alongside Obol so that consumers who locate Obol
  # via find_package(Obol) can link and run without additional setup.
  if((OBOL_USE_SWRAST) AND TARGET osmesa)
    install(TARGETS osmesa osmesa_interface EXPORT ${PROJECT_NAME}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT development
    )
  endif()
endif()

# ############################################################################
# Install config package
# ############################################################################

if(OBOL_BUILD_MAC_FRAMEWORK)
  set(OBOL_CONFIG_PACKAGE_LOCATION ${OBOL_MAC_FRAMEWORK_NAME}.framework/Resources/CMake)
  string(TOLOWER ${OBOL_MAC_FRAMEWORK_NAME} OBOL_CONFIG_PACKAGE_BASENAME)
else()
  set(OBOL_CONFIG_PACKAGE_LOCATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION})
  string(TOLOWER ${PROJECT_NAME} OBOL_CONFIG_PACKAGE_BASENAME)
  if(UNIX OR MINGW)
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/obol-default.cfg.in"
      "${CMAKE_BINARY_DIR}/${PROJECT_NAME_LOWER}-default.cfg"
      @ONLY)
    install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME_LOWER}-default.cfg" DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/Obol/conf)
  endif()
endif()

# Support find_package(Obol NO_MODULE).
set(OBOL_DOC_DIR ${CMAKE_INSTALL_DOCDIR})
set(OBOL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(OBOL_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
set(OBOLDIR ${CMAKE_INSTALL_PREFIX})

include(CMakePackageConfigHelpers)

configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME_LOWER}-config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${OBOL_CONFIG_PACKAGE_BASENAME}-config.cmake"
  INSTALL_DESTINATION ${OBOL_CONFIG_PACKAGE_LOCATION}
  PATH_VARS OBOLDIR OBOL_INCLUDE_DIR OBOL_LIB_DIR OBOL_DOC_DIR
)

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${OBOL_CONFIG_PACKAGE_BASENAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

# To make the component usable not only from the install directory but also from the build directory
set(_obol_export_targets ${PROJECT_NAME})
if(OBOL_USE_SWRAST AND TARGET osmesa_interface AND TARGET osmesa)
  list(APPEND _obol_export_targets osmesa_interface osmesa)
endif()
export(
  TARGETS ${_obol_export_targets}
  FILE ${OBOL_CONFIG_PACKAGE_BASENAME}-export.cmake
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${OBOL_CONFIG_PACKAGE_BASENAME}-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/${OBOL_CONFIG_PACKAGE_BASENAME}-config-version.cmake"
  DESTINATION ${OBOL_CONFIG_PACKAGE_LOCATION}
  COMPONENT development
)

install(EXPORT ${PROJECT_NAME}
  DESTINATION ${OBOL_CONFIG_PACKAGE_LOCATION}
  NAMESPACE ${PROJECT_NAME}::
  FILE ${OBOL_CONFIG_PACKAGE_BASENAME}-export.cmake
  COMPONENT development
)
