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
  actions base bundles caches details elements
  engines errors events fields fonts glue io misc rendering lists
  nodes shapenodes projectors sensors
  collision shadows geo threads shaders tools
  hardcopy
)

foreach(_subdir ${COIN_SUBDIRS})
  add_subdirectory(${_subdir})
endforeach()
add_subdirectory(doc)

# Adding source groups for IDE's (Visual Studio, XCode, Qt Creator, etc.)
source_group("Header Files\\Inventor"                            REGULAR_EXPRESSION ".*/include/Inventor/.*[.]h")
source_group("Header Files\\Inventor\\actions"                   REGULAR_EXPRESSION ".*/include/Inventor/actions/.*[.]h")
source_group("Header Files\\Inventor\\annex"                     REGULAR_EXPRESSION ".*/include/Inventor/annex/.*[.]h")
source_group("Header Files\\Inventor\\annex\\ForeignFiles"       REGULAR_EXPRESSION ".*/include/Inventor/annex/ForeignFiles/.*[.]h")
source_group("Header Files\\Inventor\\annex\\FXViz"              REGULAR_EXPRESSION ".*/include/Inventor/annex/FXViz/.*[.]h")
source_group("Header Files\\Inventor\\annex\\FXViz\\elements"    REGULAR_EXPRESSION ".*/include/Inventor/annex/FXViz/elements/.*[.]h")
source_group("Header Files\\Inventor\\annex\\FXViz\\nodes"       REGULAR_EXPRESSION ".*/include/Inventor/annex/FXViz/nodes/.*[.]h")
source_group("Header Files\\Inventor\\annex\\Hardcopy"           REGULAR_EXPRESSION ".*/include/Inventor/annex/Hardcopy/.*[.]h")
source_group("Header Files\\Inventor\\annex\\Profiler"           REGULAR_EXPRESSION ".*/include/Inventor/annex/Profiler/.*[.]h")
source_group("Header Files\\Inventor\\annex\\Profiler\\elements" REGULAR_EXPRESSION ".*/include/Inventor/annex/Profiler/elements/.*[.]h")
source_group("Header Files\\Inventor\\annex\\Profiler\\engines"  REGULAR_EXPRESSION ".*/include/Inventor/annex/Profiler/engines/.*[.]h")
source_group("Header Files\\Inventor\\annex\\Profiler\\nodekits" REGULAR_EXPRESSION ".*/include/Inventor/annex/Profiler/nodekits/.*[.]h")
source_group("Header Files\\Inventor\\annex\\Profiler\\nodes"    REGULAR_EXPRESSION ".*/include/Inventor/annex/Profiler/nodes/.*[.]h")
source_group("Header Files\\Inventor\\annex\\Profiler\\utils"    REGULAR_EXPRESSION ".*/include/Inventor/annex/Profiler/utils/.*[.]h")
source_group("Header Files\\Inventor\\bundles"                   REGULAR_EXPRESSION ".*/include/Inventor/bundles/.*[.]h")
source_group("Header Files\\Inventor\\C"                         REGULAR_EXPRESSION ".*/include/Inventor/C/.*[.]h")
source_group("Header Files\\Inventor\\C\\base"                   REGULAR_EXPRESSION ".*/include/Inventor/C/base/.*[.]h")
source_group("Header Files\\Inventor\\C\\errors"                 REGULAR_EXPRESSION ".*/include/Inventor/C/errors/.*[.]h")
source_group("Header Files\\Inventor\\C\\glue"                   REGULAR_EXPRESSION ".*/include/Inventor/C/glue/.*[.]h")
source_group("Header Files\\Inventor\\C\\threads"                REGULAR_EXPRESSION ".*/include/Inventor/C/threads/.*[.]h")
source_group("Header Files\\Inventor\\C\\XML"                    REGULAR_EXPRESSION ".*/include/Inventor/C/XML/.*[.]h")
source_group("Header Files\\Inventor\\manips"                    REGULAR_EXPRESSION ".*/include/Inventor/manips/.*[.]h")
source_group("Header Files\\Inventor\\misc"                      REGULAR_EXPRESSION ".*/include/Inventor/misc/.*[.]h")

source_group("Header Files\\Inventor\\nodekits"                  REGULAR_EXPRESSION ".*/include/Inventor/nodekits/.*[.]h")
source_group("Header Files\\Inventor\\nodes"                     REGULAR_EXPRESSION ".*/include/Inventor/nodes/.*[.]h")
source_group("Header Files\\Inventor\\projectors"                REGULAR_EXPRESSION ".*/include/Inventor/projectors/.*[.]h")
source_group("Header Files\\Inventor\\scxml"                     REGULAR_EXPRESSION ".*/include/Inventor/scxml/.*[.]h")
source_group("Header Files\\Inventor\\sensors"                   REGULAR_EXPRESSION ".*/include/Inventor/sensors/.*[.]h")
source_group("Header Files\\Inventor\\system"                    REGULAR_EXPRESSION ".*/include/Inventor/system/.*[.]h")
source_group("Header Files\\Inventor\\VRMLnodes"                 REGULAR_EXPRESSION ".*/include/Inventor/VRMLnodes/.*[.]h")

source_group("Source Files\\3ds"                       REGULAR_EXPRESSION ".*/src/3ds/.*[.](h|c|cpp)")
source_group("Source Files\\actions"                   REGULAR_EXPRESSION ".*/src/actions/.*[.](h|c|cpp)")
source_group("Source Files\\base"                      REGULAR_EXPRESSION ".*/src/base/.*[.](h|c|cpp)")
source_group("Source Files\\bundles"                   REGULAR_EXPRESSION ".*/src/bundles/.*[.](h|c|cpp)")
source_group("Source Files\\caches"                    REGULAR_EXPRESSION ".*/src/caches/.*[.](h|c|cpp)")
source_group("Source Files\\collision"                 REGULAR_EXPRESSION ".*/src/collision/.*[.](h|c|cpp)")
source_group("Source Files\\details"                   REGULAR_EXPRESSION ".*/src/details/.*[.](h|c|cpp)")
source_group("Source Files\\elements"                  REGULAR_EXPRESSION ".*/src/elements/.*[.](h|c|cpp)")
source_group("Source Files\\elements\\GL"              REGULAR_EXPRESSION ".*/src/elements/GL/.*[.](h|c|cpp)")
source_group("Source Files\\engines"                   REGULAR_EXPRESSION ".*/src/engines/.*[.](h|c|cpp)")
source_group("Source Files\\errors"                    REGULAR_EXPRESSION ".*/src/errors/.*[.](h|c|cpp)")
source_group("Source Files\\events"                    REGULAR_EXPRESSION ".*/src/events/.*[.](h|c|cpp)")
source_group("Source Files\\fields"                    REGULAR_EXPRESSION ".*/src/fields/.*[.](h|c|cpp)")
source_group("Source Files\\fonts"                     REGULAR_EXPRESSION ".*/src/fonts/.*[.](h|c|cpp)")
source_group("Source Files\\foreignfiles"              REGULAR_EXPRESSION ".*/src/foreignfiles/.*[.](h|c|cpp)")
source_group("Source Files\\geo"                       REGULAR_EXPRESSION ".*/src/geo/.*[.](h|c|cpp)")
source_group("Source Files\\glue"                      REGULAR_EXPRESSION ".*/src/glue/.*[.](h|c|cpp)")
source_group("Source Files\\hardcopy"                  REGULAR_EXPRESSION ".*/src/hardcopy/.*[.](h|c|cpp)")
source_group("Source Files\\io"                        REGULAR_EXPRESSION ".*/src/io/.*[.](h|c|cpp)")
source_group("Source Files\\lists"                     REGULAR_EXPRESSION ".*/src/lists/.*[.](h|c|cpp)")
source_group("Source Files\\manips"                    REGULAR_EXPRESSION ".*/src/manips/.*[.](h|c|cpp)")
source_group("Source Files\\misc"                      REGULAR_EXPRESSION ".*/src/misc/.*[.](h|c|cpp)")
source_group("Source Files\\navigation"                REGULAR_EXPRESSION ".*/src/navigation/.*[.](h|c|cpp)")
source_group("Source Files\\nodekits"                  REGULAR_EXPRESSION ".*/src/nodekits/.*[.](h|c|cpp)")
source_group("Source Files\\nodes"                     REGULAR_EXPRESSION ".*/src/nodes/.*[.](h|c|cpp)")
source_group("Source Files\\profiler"                  REGULAR_EXPRESSION ".*/src/profiler/.*[.](h|c|cpp)")
source_group("Source Files\\profiler\\tools"           REGULAR_EXPRESSION ".*/src/profiler/tools/.*[.](h|c|cpp)")
source_group("Source Files\\projectors"                REGULAR_EXPRESSION ".*/src/projectors/.*[.](h|c|cpp)")
source_group("Source Files\\rendering"                 REGULAR_EXPRESSION ".*/src/rendering/.*[.](h|c|cpp)")
source_group("Source Files\\scxml"                     REGULAR_EXPRESSION ".*/src/scxml/.*[.](h|c|cpp)")
source_group("Source Files\\sensors"                   REGULAR_EXPRESSION ".*/src/sensors/.*[.](h|c|cpp)")
source_group("Source Files\\shaders"                   REGULAR_EXPRESSION ".*/src/shaders/.*[.](h|c|cpp)")
source_group("Source Files\\shadows"                   REGULAR_EXPRESSION ".*/src/shadows/.*[.](h|c|cpp)")
source_group("Source Files\\shapenodes"                REGULAR_EXPRESSION ".*/src/shapenodes/.*[.](h|c|cpp)")
source_group("Source Files\\soscxml"                   REGULAR_EXPRESSION ".*/src/soscxml/.*[.](h|c|cpp)")
source_group("Source Files\\system"                    REGULAR_EXPRESSION ".*/src/system/.*[.](h|c|cpp)")
source_group("Source Files\\threads"                   REGULAR_EXPRESSION ".*/src/threads/.*[.](h|c|cpp)")
source_group("Source Files\\vrml97"                    REGULAR_EXPRESSION ".*/src/vrml97/.*[.](h|c|cpp)")

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
        $<TARGET_OBJECTS:misc>
        $<TARGET_OBJECTS:nodes>
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
  if(MSVC)
    # No SCXML-related Bison warnings to suppress after removing SCXML
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
# Install dragger .iv files
# ############################################################################

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
