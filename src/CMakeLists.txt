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

configure_file(setup.h.cmake.in setup.h)

add_compile_options(-DHAVE_CONFIG_H -DCOIN_INTERNAL -DCOIN_DEBUG=$<CONFIG:Debug>)

include_directories(BEFORE
  "${PROJECT_BINARY_DIR}/include"
  "${PROJECT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${PROJECT_SOURCE_DIR}/include/Inventor/annex"
  "${PROJECT_BINARY_DIR}/src/shadows"
)

# add include dirs for non targets
if(NOT "${COIN_TARGET_INCLUDE_DIRECTORIES}" STREQUAL "")
  include_directories(${COIN_TARGET_INCLUDE_DIRECTORIES})
endif()
# add include dirs for targets
foreach(_tgt ${COIN_TARGET_LINK_LIBRARIES})
  if(TARGET ${_tgt} )
    include_directories($<TARGET_PROPERTY:${_tgt},INTERFACE_INCLUDE_DIRECTORIES>)
  endif()
endforeach()

set(COIN_SOURCE C/CoinTidbits.cpp CACHE INTERNAL "COIN_SOURCE")
# Minimal core subdirectories - removed GUI/toolkit but keeping minimal dependencies
set(COIN_SUBDIRS
  actions base bundles caches details draggers elements
  engines errors events fields fonts glue io manips misc rendering lists
  nodes shapenodes projectors sensors
  collision shadows geo threads shaders tools
  hardcopy
)

# Add nodekits and related components if enabled
list(APPEND COIN_SUBDIRS nodekits profiler)

foreach(_subdir ${COIN_SUBDIRS})
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
set(COIN_LIBRARY_TYPE $<IF:$<BOOL:${BUILD_SHARED_LIBS}>,SHARED,STATIC>)

# Additional headers to include in target
set(COIN_ADDITIONAL_HDRS 
    ${PROJECT_SOURCE_DIR}/include/SoWinEnterScope.h 
    ${PROJECT_SOURCE_DIR}/include/SoWinLeaveScope.h 
    ${PROJECT_SOURCE_DIR}/include/SoDebug.h 
    ${PROJECT_BINARY_DIR}/src/config.h
)

# Create the main library target
if(COIN_BUILD_SINGLE_LIB)
    add_library(${PROJECT_NAME} ${COIN_SOURCE} ${COIN_ADDITIONAL_HDRS})
else()
    add_library(${PROJECT_NAME} ${COIN_SOURCE} ${COIN_ADDITIONAL_HDRS}
        $<TARGET_OBJECTS:actions>
        $<TARGET_OBJECTS:base>
        $<TARGET_OBJECTS:bundles>
        $<TARGET_OBJECTS:caches>
        $<TARGET_OBJECTS:collision>
        $<TARGET_OBJECTS:details>
        $<TARGET_OBJECTS:draggers>
        $<TARGET_OBJECTS:elements>
        $<TARGET_OBJECTS:engines>
        $<TARGET_OBJECTS:errors>
        $<TARGET_OBJECTS:events>
        $<TARGET_OBJECTS:fields>
        $<TARGET_OBJECTS:fonts>
        $<TARGET_OBJECTS:geo>
        $<TARGET_OBJECTS:glelements>
        $<TARGET_OBJECTS:glue>
        $<TARGET_OBJECTS:hardcopy>
        $<TARGET_OBJECTS:io>
        $<TARGET_OBJECTS:lists>
        $<TARGET_OBJECTS:manips>
        $<TARGET_OBJECTS:misc>
        $<TARGET_OBJECTS:nodekits>
        $<TARGET_OBJECTS:nodes>
        $<TARGET_OBJECTS:profiler>
        $<TARGET_OBJECTS:projectors>
        $<TARGET_OBJECTS:rendering>
        $<TARGET_OBJECTS:sensors>
        $<TARGET_OBJECTS:shaders>
        $<TARGET_OBJECTS:shadows>
        $<TARGET_OBJECTS:shapenodes>
        $<TARGET_OBJECTS:threads>
        $<TARGET_OBJECTS:tools>
    )
endif()

# Set target properties
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Set library output name based on backend selection
if(DEFINED COIN3D_OUTPUT_NAME)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${COIN3D_OUTPUT_NAME})
endif()

# ============================================================================
# Target-specific compile definitions and properties
# ============================================================================

target_compile_definitions(${PROJECT_NAME} PRIVATE
    HAVE_CONFIG_H
    COIN_INTERNAL
    COIN_DEBUG=$<CONFIG:Debug>
)

# Target-specific include directories (modern approach)
target_include_directories(${PROJECT_NAME} 
    PRIVATE
        "${PROJECT_BINARY_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${PROJECT_SOURCE_DIR}/include/Inventor/annex"
        "${PROJECT_BINARY_DIR}/src/shadows"
        ${COIN_TARGET_INCLUDE_DIRECTORIES}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    INTERFACE
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Target-specific linking (modern approach)  
target_link_libraries(${PROJECT_NAME} PRIVATE ${COIN_TARGET_LINK_LIBRARIES})

if(WIN32)
  if(COIN_BUILD_SHARED_LIBS)
    configure_file(${PROJECT_NAME}.rc.cmake.in ${PROJECT_NAME}.rc)
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.rc")
    target_compile_definitions(${PROJECT_NAME} INTERFACE COIN_DLL)
    if(MINGW)
      set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_NAME ${PROJECT_NAME}-${COIN_SO_VERSION})
    endif()
  else()
    target_compile_definitions(${PROJECT_NAME} INTERFACE COIN_NOT_DLL)
    if(MSVC)
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Z7")                       # Override default /Zi to embed
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Z7")     # debugging info in the .lib.
    endif()
  endif()
elseif(APPLE)
  if(COIN_BUILD_MAC_FRAMEWORK)
    # Set the framework target properties just to make sure the relevant links inside the framework 
    # are created.  Because the default framework name differs from the project name we change the
    # output name property.
    set_target_properties(${PROJECT_NAME} PROPERTIES
      FRAMEWORK TRUE
      FRAMEWORK_VERSION ${COIN_MAC_FRAMEWORK_VERSION}
      #PUBLIC_HEADER "${COIN_PUBLIC_HEADERS}" # does not work recursively
      #PRIVATE_HEADER "${COIN_PRIVATE_HEADERS}" # does not work recursively
      RESOURCE "${CMAKE_BINARY_DIR}/version.plist" # does not work recursively
      MACOSX_FRAMEWORK_IDENTIFIER org.coin3d.Coin.framework
      MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
      OUTPUT_NAME ${COIN_MAC_FRAMEWORK_NAME}
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
        SOVERSION ${COIN_SO_VERSION}
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        VERSION "${COIN_SO_VERSION}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}" 
        SOVERSION ${COIN_SO_VERSION}
    )
endif()

# Setting up a framework means adding all the files and links under a framework
# directory. A single install command then moves the entire contents of the
# framework directory to the COIN_MAC_FRAMEWORK_PREFIX directory (/Library/Frameworks
# by default)
# As the PUBLIC_HEADER and RESOURCE options of install target do not support
# directory structure creation when building a framework we set the MACOSX_PACKAGE_LOCATION
# property on the source files and add them to the target. This does however not work
# for the generated documentation files.
# All non standard links must be created manually.
if(COIN_BUILD_MAC_FRAMEWORK)

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

  if(COIN_BUILD_DOCUMENTATION)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
      COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation
      COMMAND ${CMAKE_COMMAND} -E create_symlink ./Versions/Current/Documentation $<TARGET_FILE_DIR:${PROJECT_NAME}>/../../Documentation
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/html $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation/html)
    if(COIN_BUILD_DOCUMENTATION_QTHELP)
      add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD 
      COMMAND ${CMAKE_COMMAND} -E remove $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation/html/${PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.qch
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/html/${PROJECT_NAME}-${PROJECT_VERSION_MAJOR}.qch $<TARGET_FILE_DIR:${PROJECT_NAME}>/Documentation)
    endif()
    if(COIN_BUILD_DOCUMENTATION_MAN)
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
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../${COIN_MAC_FRAMEWORK_NAME} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Libraries/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${COIN_SO_VERSION}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}${CMAKE_SHARED_LIBRARY_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../${COIN_MAC_FRAMEWORK_NAME} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Libraries/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${COIN_SO_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../${COIN_MAC_FRAMEWORK_NAME} $<TARGET_FILE_DIR:${PROJECT_NAME}>/Libraries/${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
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
          PATTERN "C/threads/condvar.h" EXCLUDE  
          PATTERN "C/threads/mutex.h" EXCLUDE
          PATTERN "C/threads/recmutex.h" EXCLUDE
          PATTERN "C/threads/thread.h" EXCLUDE
          PATTERN "C/glue/dl.h" EXCLUDE
          PATTERN "C/basic.h.in" EXCLUDE
          PATTERN "C/basic.h.cmake.in" EXCLUDE)
  install(DIRECTORY ${CMAKE_BINARY_DIR}/include/Inventor/ 
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Inventor 
          COMPONENT development 
          FILES_MATCHING PATTERN "*.h")
  install(FILES ${PROJECT_SOURCE_DIR}/include/SoWinEnterScope.h ${PROJECT_SOURCE_DIR}/include/SoWinLeaveScope.h ${PROJECT_SOURCE_DIR}/include/SoDebug.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT development)

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
    if(COIN_BUILD_SHARED_LIBS)
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
    if(COIN_BUILD_SHARED_LIBS)
      install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT development NAMELINK_ONLY)
      if(MSVC)
        install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION ${CMAKE_INSTALL_BINDIR} CONFIGURATIONS Debug RelWithDebInfo COMPONENT debug)
      endif()
    endif()
  endif()
endif()

# ############################################################################
# Install config package
# ############################################################################

if(COIN_BUILD_MAC_FRAMEWORK)
  set(COIN_CONFIG_PACKAGE_LOCATION ${COIN_MAC_FRAMEWORK_NAME}.framework/Resources/CMake)
  string(TOLOWER ${COIN_MAC_FRAMEWORK_NAME} COIN_CONFIG_PACKAGE_BASENAME)
else()
  set(COIN_CONFIG_PACKAGE_LOCATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}-${PROJECT_VERSION})
  string(TOLOWER ${PROJECT_NAME} COIN_CONFIG_PACKAGE_BASENAME)
  if(UNIX OR MINGW)
    install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME_LOWER}-default.cfg" DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/Coin/conf)
  endif()
endif()

# Support find_package(Coin NO_MODULE).
set(COIN_DOC_DIR ${CMAKE_INSTALL_DOCDIR})
set(COIN_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(COIN_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
set(COINDIR ${CMAKE_INSTALL_PREFIX})

include(CMakePackageConfigHelpers)

configure_package_config_file(${PROJECT_NAME_LOWER}-config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${COIN_CONFIG_PACKAGE_BASENAME}-config.cmake"
  INSTALL_DESTINATION ${COIN_CONFIG_PACKAGE_LOCATION}
  PATH_VARS COINDIR COIN_INCLUDE_DIR COIN_LIB_DIR COIN_DOC_DIR
)

write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${COIN_CONFIG_PACKAGE_BASENAME}-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

# To make the component usable not only from the install directory but also from the build directory
export(
  TARGETS ${PROJECT_NAME}
  FILE ${COIN_CONFIG_PACKAGE_BASENAME}-export.cmake
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${COIN_CONFIG_PACKAGE_BASENAME}-config.cmake" "${CMAKE_CURRENT_BINARY_DIR}/${COIN_CONFIG_PACKAGE_BASENAME}-config-version.cmake"
  DESTINATION ${COIN_CONFIG_PACKAGE_LOCATION}
  COMPONENT development
)

install(EXPORT ${PROJECT_NAME}
  DESTINATION ${COIN_CONFIG_PACKAGE_LOCATION}
  NAMESPACE ${PROJECT_NAME}::
  FILE ${COIN_CONFIG_PACKAGE_BASENAME}-export.cmake
  COMPONENT development
)
