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

add_compile_options(-DHAVE_CONFIG_H -DOBOL_INTERNAL -DOBOL_DEBUG=$<CONFIG:Debug>)

if(OBOL_PROFILING)
  add_compile_options(-DOBOL_PROFILING)
endif()

include_directories(BEFORE
  "${PROJECT_BINARY_DIR}/include"
  "${PROJECT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/misc"
  "${PROJECT_SOURCE_DIR}/include/Inventor/annex"
  "${PROJECT_BINARY_DIR}/src/shadows"
  "${PROJECT_SOURCE_DIR}"
  # Subdirectory-specific include paths (needed in single-lib mode where
  # subdirectory include_directories() calls don't propagate to the parent)
  "${PROJECT_SOURCE_DIR}/src/base/utf8/include"
  "${PROJECT_BINARY_DIR}/src/draggers"
)

# add include dirs for non targets
if(NOT "${OBOL_TARGET_INCLUDE_DIRECTORIES}" STREQUAL "")
  include_directories(${OBOL_TARGET_INCLUDE_DIRECTORIES})
endif()
# add include dirs for targets
foreach(_tgt ${OBOL_TARGET_LINK_LIBRARIES})
  if(TARGET ${_tgt} )
    include_directories($<TARGET_PROPERTY:${_tgt},INTERFACE_INCLUDE_DIRECTORIES>)
  endif()
endforeach()

set(OBOL_SOURCE misc/CoinTidbits.cpp CACHE INTERNAL "OBOL_SOURCE")

foreach(_subdir actions base bundles caches details draggers elements
                engines errors events fields fonts glue io manips misc rendering lists
                nodes shapenodes projectors sensors shadows threads shaders tools
                hardcopy nodekits profiler hud)
  add_subdirectory(${_subdir})
endforeach()
add_subdirectory(doc)

# As CMake does not support deep header hierarchies for framework generation we tweak the
# installation paths for the install commands.  To make sure the symbolic links for versioned
# Binaries, Headers, and Resources are properly created we add at headers and resources to
# the target.

# ============================================================================
# Library target creation
# ============================================================================

# Determine library type
set(OBOL_LIBRARY_TYPE $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,SHARED,STATIC>)

# Additional headers to include in target
set(OBOL_ADDITIONAL_HDRS 
    ${PROJECT_BINARY_DIR}/src/config.h
)

# In dual-GL builds, gl_osmesa.cpp and SoDBOSMesa.cpp need the OSMesa
# include directories.  Apply them per-file so they don't pollute the rest
# of the build (system GL headers and OSMesa headers can conflict).
if(OBOL_BUILD_DUAL_GL AND OBOL_OSMESA_INCLUDE_DIRS)
    foreach(_osmesa_src
            "${CMAKE_CURRENT_SOURCE_DIR}/glue/gl_osmesa.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/misc/SoDBOSMesa.cpp")
        set_source_files_properties(${_osmesa_src} PROPERTIES
            INCLUDE_DIRECTORIES "${OBOL_OSMESA_INCLUDE_DIRS}")
    endforeach()
endif()

add_library(${PROJECT_NAME} ${OBOL_SOURCE} ${OBOL_ADDITIONAL_HDRS})
if(TARGET obol_parser_gen)
  add_dependencies(${PROJECT_NAME} obol_parser_gen)
endif()

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Set library output name based on backend selection
if(DEFINED OBOL_OUTPUT_NAME)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${OBOL_OUTPUT_NAME})
endif()

# ============================================================================
# Unity Build Support
# ============================================================================

# Enable unity build if requested and supported (CMake 3.16+)
if(OBOL_USE_UNITY_BUILD AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    message(STATUS "Enabling unity build for faster compilation")
    set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
elseif(OBOL_USE_UNITY_BUILD)
    message(WARNING "Unity build requested but requires CMake 3.16+. Current version: ${CMAKE_VERSION}")
endif()

# ============================================================================
# Target-specific compile definitions and properties
# ============================================================================

target_compile_definitions(${PROJECT_NAME} PRIVATE
    HAVE_CONFIG_H
    OBOL_INTERNAL
    OBOL_DEBUG=$<CONFIG:Debug>
    # In OSMesa-only builds, all gl* calls in the library must be mangled to
    # mgl* so they resolve to the OSMesa library symbols.  This is scoped to
    # the library target so that test executables (which may include <GL/glx.h>)
    # are not affected — <GL/glx.h> conditionally includes "glx_mangle.h" when
    # USE_MGL_NAMESPACE is defined, and that file does not exist on Ubuntu 24.04.
    $<$<BOOL:${OBOL_USE_OSMESA}>:USE_MGL_NAMESPACE>
)

# Apply warning flags (controlled by OBOL_WARNINGS option in the top-level CMakeLists.txt)
if(OBOL_WARNING_FLAGS)
    target_compile_options(${PROJECT_NAME} PRIVATE ${OBOL_WARNING_FLAGS})
endif()

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
)

# Target-specific linking (modern approach)  
target_link_libraries(${PROJECT_NAME} PRIVATE ${OBOL_TARGET_LINK_LIBRARIES})

# For dual-GL builds, also link OSMesa so that osmesa_SoGLContext_* functions
# can resolve mgl* symbols at runtime.
if(OBOL_BUILD_DUAL_GL)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OBOL_OSMESA_LIBRARIES})
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

  if(OBOL_BUILD_DOCUMENTATION)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
      COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation
      COMMAND ${CMAKE_COMMAND} -E create_symlink ./Versions/Current/Documentation $<TARGET_FILE_DIR:${PROJECT_NAME}>/../../Documentation
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/html $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation/html)
    if(OBOL_BUILD_DOCUMENTATION_QTHELP)
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
      COMMAND ${CMAKE_COMMAND} -E remove $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation/html/${PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.qch
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/html/${PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.qch $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation)
    endif()
    if(OBOL_BUILD_DOCUMENTATION_MAN)
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/man/man3 $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation/man)
    endif()
  endif()

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


# ############################################################################
# Install target
# ############################################################################

  if(NOT CMAKE_VERSION VERSION_LESS 3.12)
    install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime NAMELINK_COMPONENT development
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT development
      RESOURCE DESTINATION ${CMAKE_INSTALL_DATADIR} COMPONENT development
    )
    if(OBOL_BUILD_SHARED_LIBS)
      if(MSVC)
        install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Debug RelWithDebInfo COMPONENT debug)
      endif()
    endif()
  else()
    install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime NAMELINK_SKIP
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT development
      RESOURCE DESTINATION ${CMAKE_INSTALL_DATADIR} COMPONENT development
    )
    if(OBOL_BUILD_SHARED_LIBS)
      install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT development NAMELINK_ONLY)
      if(MSVC)
        install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Debug RelWithDebInfo COMPONENT debug)
      endif()
    endif()
  endif()

  # When using the bundled OSMesa submodule (DUAL_GL or osmesa-only builds),
  # install its shared library alongside Obol so that consumers who locate Obol
  # via find_package(Obol) can link and run without additional setup.
  if((OBOL_BUILD_DUAL_GL OR OBOL_USE_OSMESA) AND TARGET osmesa)
    install(TARGETS osmesa
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime
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
export(
  TARGETS ${PROJECT_NAME}
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
