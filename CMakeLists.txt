cmake_minimum_required(VERSION 3.16)

# Modern CMake project definition with metadata
project(Obol
  VERSION 1.0.0
  DESCRIPTION "A 3D visualization library implementing a subset of the Open Inventor 2.1 API (with some variations)"
  HOMEPAGE_URL "https://github.com/starseeker/obol"
  LANGUAGES C CXX
  )

# Set modern CMake policies
cmake_policy(SET CMP0054 NEW)  # Only interpret if() arguments as variables or keywords when unquoted
cmake_policy(SET CMP0072 NEW)  # FindOpenGL prefers GLVND
cmake_policy(SET CMP0075 NEW)  # Include CMAKE_REQUIRED_LIBRARIES in CheckIncludeFile
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
cmake_policy(SET CMP0079 NEW)  # target_link_libraries() allows use with targets in other directories

# Project variables for compatibility
set(COIN_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set(COIN_MINOR_VERSION ${PROJECT_VERSION_MINOR})
set(COIN_MICRO_VERSION ${PROJECT_VERSION_PATCH})
set(COIN_BETA_VERSION)
set(COIN_VERSION ${PROJECT_VERSION})
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

string(TIMESTAMP COIN_BUILD_YEAR "%Y")
math(EXPR COIN_SO_VERSION ${PROJECT_VERSION_MAJOR}*20)

# Enforce out-of-source builds
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "${CMAKE_PROJECT_NAME} requires an out-of-source build. Please create a separate build directory and run 'cmake <path_to_${CMAKE_PROJECT_NAME}> [options]' from there.")
endif()

# Make sure submodules are initialized
if (NOT EXISTS ${PROJECT_SOURCE_DIR}/external/osmesa/CMakeLists.txt)
  find_program(GIT_EXECUTABLE git)
  if (NOT GIT_EXECUTABLE)
    message(FATAL_ERROR "Need to populate Git submodule, but unable to find git executable")
  endif (NOT GIT_EXECUTABLE)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive ${PROJECT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# Set C++ standard requirements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "Setting C++ standard: C++${CMAKE_CXX_STANDARD} (required)")

# Add path for Coin-specific utility scripts
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Standard CMake modules
include(CheckCSourceRuns)
include(CheckCXXSourceCompiles)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckTypeSize)
include(CMakeDependentOption)
include(GNUInstallDirs)
include(FeatureSummary)

# Build type handling
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# ============================================================================
# Build configuration options
# ============================================================================

# Core build options
option(BUILD_SHARED_LIBS "Build shared library when ON, static when OFF" ON)
option(OBOL_BUILD_TESTS "Build unit tests" ON)
option(OBOL_BUILD_VIEWER "Build the FLTK scene viewer (obol_viewer); requires FLTK" OFF)

# Internal alias used throughout src/ for Windows shared/static detection
set(COIN_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Documentation options removed - not needed for minimal library build

# Feature options
# Backend selection:
#   COIN3D_USE_OSMESA=ON          → OSMesa-only (headless, no system GL)
#   COIN3D_USE_SYSTEM_ONLY=ON     → system OpenGL only (no OSMesa)
#   default (both OFF)            → auto-detect; dual mode when both are available
option(COIN3D_USE_OSMESA "Build against OSMesa only for offscreen/headless rendering" OFF)
option(COIN3D_USE_SYSTEM_ONLY "Build against system OpenGL only (no OSMesa fallback)" OFF)
option(COIN3D_BUILD_DUAL_GL "Build with both system-OpenGL and OSMesa backends in a single library" OFF)
option(COIN_THREADSAFE "Enable thread safe render traversals" ON)

# Component options
option(HAVE_NODEKITS "Enable NodeKit support" ON)
option(HAVE_DRAGGERS "Enable Dragger support" ON)
option(HAVE_MANIPULATORS "Enable Manipulator support" ON)
option(OBOL_PROFILING "Enable profiling subsystem" OFF)

# External dependencies
option(USE_EXCEPTIONS "Compile with exceptions (g++ only)" ON)

# Code coverage (GCC/Clang only; builds with --coverage flags and provides a
# 'coverage' target that runs CTest then generates an lcov HTML report)
option(OBOL_COVERAGE "Enable code coverage instrumentation (GCC/Clang)" OFF)

# Build optimization options
option(OBOL_USE_PRECOMPILED_HEADERS "Use precompiled headers to speed up compilation" ON)

# Thread debugging (requires C++11 thread support)
check_include_file_cxx(thread HAVE_STD_THREAD)

# ============================================================================
# Installation paths configuration
# ============================================================================

# Coin-specific data directory
set(CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")

# ============================================================================
# Dependency management
# ============================================================================

# Initialize lists for target properties (modern approach)
set(COIN_TARGET_LINK_LIBRARIES)
set(COIN_TARGET_INCLUDE_DIRECTORIES)
set(COIN_TARGET_LINK_LIBRARIES_GL)

# OpenGL backend selection
# -----------------------------------------------------------------------
# Auto-detection when neither OSMESA-only nor SYSTEM-only is forced:
#   1. Check for system OpenGL and for the OSMesa submodule
#   2. If both are available → enable DUAL GL (one library, both backends)
#   3. If only OSMesa submodule is available → OSMesa-only mode
#   4. If only system GL is available → system-only mode
# The user can override by setting COIN3D_USE_OSMESA=ON (osmesa only)
# or COIN3D_USE_SYSTEM_ONLY=ON (system GL only) before configuring.
#
# NOTE: system libosmesa6-dev is intentionally NOT used for auto-detection.
# The project requires its own name-mangled OSMesa build (external/osmesa
# submodule with USE_MGL_NAMESPACE).  The submodule include path
# <OSMesa/osmesa.h> was chosen specifically to avoid pulling in the
# system header at <GL/osmesa.h>, which uses un-mangled gl* symbols
# incompatible with a dual system-GL+OSMesa build.
# -----------------------------------------------------------------------
if(NOT COIN3D_USE_OSMESA AND NOT COIN3D_USE_SYSTEM_ONLY AND NOT COIN3D_BUILD_DUAL_GL)
  find_package(OpenGL QUIET)
  # "OSMesa available" means our submodule is present (not system osmesa)
  if(EXISTS "${PROJECT_SOURCE_DIR}/external/osmesa/CMakeLists.txt")
    set(_have_osmesa_submodule TRUE)
  else()
    set(_have_osmesa_submodule FALSE)
  endif()
  if(OpenGL_FOUND AND _have_osmesa_submodule)
    set(COIN3D_BUILD_DUAL_GL ON CACHE BOOL
        "Build with both system-OpenGL and OSMesa backends in a single library" FORCE)
    message(STATUS "Auto-detected system OpenGL + OSMesa submodule: enabling DUAL GL mode")
  elseif(_have_osmesa_submodule)
    set(COIN3D_USE_OSMESA ON CACHE BOOL
        "Build against OSMesa only for offscreen/headless rendering" FORCE)
    message(STATUS "Auto-detected OSMesa submodule only: enabling OSMesa-only mode")
  else()
    # System-only (OpenGL required below will error if not present)
    message(STATUS "Auto-detected: system OpenGL only (no OSMesa submodule found)")
  endif()
endif()

if(COIN3D_USE_OSMESA)
  # OSMesa backend for offscreen/headless rendering
  find_package(OSMesa REQUIRED)
  if(OSMesa_FOUND)
    set(HAVE_OPENGL 1)
    set(HAVE_OSMESA 1)

    if(TARGET OSMesa::OSMesa)
      list(APPEND COIN_TARGET_LINK_LIBRARIES_GL OSMesa::OSMesa)
    elseif(TARGET osmesa_interface)
      # starseeker/osmesa submodule interface wrapper
      list(APPEND COIN_TARGET_LINK_LIBRARIES_GL osmesa_interface)
      list(APPEND COIN_TARGET_INCLUDE_DIRECTORIES ${OSMesa_INCLUDE_DIRS})
    else()
      list(APPEND COIN_TARGET_INCLUDE_DIRECTORIES ${OSMesa_INCLUDE_DIRS})
      list(APPEND COIN_TARGET_LINK_LIBRARIES_GL ${OSMesa_LIBRARIES})
    endif()

    message(STATUS "Using OSMesa for offscreen/headless rendering")
  endif()
else()
  # System OpenGL backend (default or SYSTEM_ONLY)
  find_package(OpenGL REQUIRED)
  set(HAVE_OPENGL 1)

  if(OpenGL_OpenGL_FOUND AND TARGET OpenGL::OpenGL)
    list(APPEND COIN_TARGET_LINK_LIBRARIES_GL OpenGL::OpenGL)
  elseif(OpenGL_GL_FOUND AND TARGET OpenGL::GL)
    list(APPEND COIN_TARGET_LINK_LIBRARIES_GL OpenGL::GL)
  elseif(OPENGL_gl_LIBRARY)
    list(APPEND COIN_TARGET_LINK_LIBRARIES_GL ${OPENGL_gl_LIBRARY})
  endif()

  # On Unix/Linux, detect GLX for offscreen rendering support
  if(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
    if(X11_FOUND AND X11_X11_LIB)
      # Check for GLX support (required for offscreen rendering on Linux)
      include(CheckIncludeFileCXX)
      set(CMAKE_REQUIRED_INCLUDES ${X11_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})
      check_include_file_cxx("GL/glx.h" HAVE_GLX_H)
      if(HAVE_GLX_H)
        set(HAVE_GLX 1)
        list(APPEND COIN_TARGET_LINK_LIBRARIES_GL ${X11_X11_LIB})
        list(APPEND COIN_TARGET_INCLUDE_DIRECTORIES ${X11_INCLUDE_DIR})
        message(STATUS "GLX offscreen rendering support enabled")
      endif()
    endif()
  endif()

  message(STATUS "Using system OpenGL")
endif()

list(APPEND COIN_TARGET_LINK_LIBRARIES ${COIN_TARGET_LINK_LIBRARIES_GL})

# Dual-GL build: also find and store OSMesa libraries separately so
# gl_osmesa.cpp can be compiled against them alongside system GL.
# Our FindOSMesa.cmake only looks at the external/osmesa submodule, which
# provides the name-mangled (mgl*) build required by dual GL.
if(COIN3D_BUILD_DUAL_GL)
  if(COIN3D_USE_OSMESA)
    message(FATAL_ERROR "COIN3D_BUILD_DUAL_GL and COIN3D_USE_OSMESA are mutually exclusive. "
            "DUAL GL uses system OpenGL as primary with OSMesa as a secondary backend.")
  endif()
  find_package(OSMesa REQUIRED)
  if(NOT OSMesa_FOUND)
    message(FATAL_ERROR "COIN3D_BUILD_DUAL_GL requires the external/osmesa submodule.\n"
            "Run: git submodule update --init --recursive")
  endif()
  # Collect OSMesa include dirs and link libraries for the per-file target
  if(TARGET OSMesa::OSMesa)
    get_target_property(_osmesa_inc OSMesa::OSMesa INTERFACE_INCLUDE_DIRECTORIES)
    set(COIN_OSMESA_INCLUDE_DIRS "${_osmesa_inc}")
    set(COIN_OSMESA_LIBRARIES OSMesa::OSMesa)
  elseif(TARGET osmesa_interface)
    set(COIN_OSMESA_INCLUDE_DIRS "${OSMesa_INCLUDE_DIRS}")
    set(COIN_OSMESA_LIBRARIES osmesa_interface)
  else()
    set(COIN_OSMESA_INCLUDE_DIRS "${OSMesa_INCLUDE_DIRS}")
    set(COIN_OSMESA_LIBRARIES "${OSMesa_LIBRARIES}")
  endif()
  add_definitions(-DCOIN3D_BUILD_DUAL_GL)
  message(STATUS "Dual-GL build enabled: system OpenGL + OSMesa in one library")
endif()

# Set library output name based on backend selection
if(COIN3D_USE_OSMESA)
  set(COIN3D_OUTPUT_NAME "Coin-osmesa")
  add_definitions(-DCOIN3D_OSMESA_BUILD)
  add_definitions(-DUSE_MGL_NAMESPACE)  # Enable MGL name mangling to match osmesa library
  message(STATUS "Library will be named: lib${COIN3D_OUTPUT_NAME}")
else()
  set(COIN3D_OUTPUT_NAME "Coin")
  message(STATUS "Library will be named: lib${COIN3D_OUTPUT_NAME}")
endif()

# Threading support
find_package(Threads REQUIRED)
set(HAVE_THREADS 1)
if(CMAKE_USE_WIN32_THREADS_INIT)
  set(USE_W32THREAD 1)
  set(COIN_THREADID_TYPE DWORD)
elseif(CMAKE_USE_PTHREADS_INIT)
  set(USE_PTHREAD 1)
  set(COIN_THREADID_TYPE pthread_t)
endif()
list(APPEND COIN_TARGET_LINK_LIBRARIES Threads::Threads)

# ============================================================================
# Build environment configuration
# ============================================================================

# Output directories (consistent for all configurations)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Multi-config generators (Visual Studio, Xcode)
foreach(_config ${CMAKE_CONFIGURATION_TYPES})
  string(TOUPPER ${_config} _config)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/lib")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/lib")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/bin")
endforeach()


# Platform-specific compiler settings
if(MSVC)
  # MSVC-specific options
  option(COIN_BUILD_MSVC_STATIC_RUNTIME "Build against the static Microsoft Visual C runtime library" OFF)
  option(COIN_BUILD_SINGLE_LIB "Build only one library when ON, multiple when OFF" ON)

  if(MSVC_VERSION GREATER_EQUAL 1500)
    option(COIN_BUILD_MSVC_MP "Enable build with multiple processes in Visual Studio" ON)
  else()
    set(COIN_BUILD_MSVC_MP OFF CACHE BOOL "Compiler option /MP requires at least Visual Studio 2008 (VS9) or newer" FORCE)
  endif()

  if(COIN_BUILD_MSVC_MP)
    add_compile_options(/MP)
  endif()

  # Enable C++ exception handling
  add_compile_options(/EHsc)

  # Static runtime linking
  if(COIN_BUILD_MSVC_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()

  # MSVC-specific definitions
  add_compile_definitions(
    _CRT_NONSTDC_NO_DEPRECATE
    _CRT_SECURE_NO_DEPRECATE 
    _CRT_SECURE_NO_WARNINGS
    _USE_MATH_DEFINES)
else()
  # Non-MSVC platforms
  if(APPLE)
    add_definitions(-DGL_SILENCE_DEPRECATION)
  endif()
  option(COIN_BUILD_SINGLE_LIB "Build only one library when ON, multiple when OFF" OFF)
endif()

set(HAVE_GZDOPEN 1)
if(WIN32)
  set(HAVE_WIN32_API 1)
  #set(HAVE_WGL 1)
  if(MINGW)
    set(COIN_DEFAULT_SHARED_POSTFIX "")
    set(COIN_DEFAULT_STATIC_POSTFIX "")
  else()
    # On Windows the major version number is part of the library name
    set(COIN_DEFAULT_SHARED_POSTFIX ${PROJECT_VERSION_MAJOR})
    set(COIN_DEFAULT_STATIC_POSTFIX ${PROJECT_VERSION_MAJOR}s)
  endif()
  if(COIN_BUILD_SHARED_LIBS)
    set(COIN_DEFAULT_POSTFIX ${COIN_DEFAULT_SHARED_POSTFIX})
    set(COIN_LIBRARY_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    add_definitions(-DCOIN_MAKE_DLL)
  else()
    set(COIN_DEFAULT_POSTFIX ${COIN_DEFAULT_STATIC_POSTFIX})
    set(COIN_LIBRARY_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()
  set(CMAKE_RELEASE_POSTFIX "${COIN_DEFAULT_POSTFIX}"
    CACHE STRING "Default filename postfix for libraries under configuration Release")
  set(CMAKE_MINSIZEREL_POSTFIX "${COIN_DEFAULT_POSTFIX}"
    CACHE STRING "Default filename postfix for libraries under configuration MinSizeRel")
  set(CMAKE_RELWITHDEBINFO_POSTFIX "${COIN_DEFAULT_POSTFIX}"
    CACHE STRING "Default filename postfix for libraries under configuration RelWithDebInfo")
  set(CMAKE_DEBUG_POSTFIX "${COIN_DEFAULT_POSTFIX}d"
    CACHE STRING "Default filename postfix for libraries under configuration Debug")

  set(COIN_RELEASE_SYSTEM_LIBRARY_NAME Coin${CMAKE_RELEASE_POSTFIX}${COIN_LIBRARY_SUFFIX})
  set(COIN_DEBUG_SYSTEM_LIBRARY_NAME Coin${CMAKE_DEBUG_POSTFIX}${COIN_LIBRARY_SUFFIX})
elseif(APPLE)
  # macOS-specific settings (minimal)
  if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
  endif()

  if(POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
  endif()

  # Basic macOS linking
  list(APPEND COIN_TARGET_LINK_LIBRARIES "-framework CoreFoundation" "-framework CoreGraphics")
  set(COIN_EXTRA_LDFLAGS " -lCoin -Wl,-framework,OpenGL")

  set(COIN_RELEASE_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:Coin>")
  set(COIN_DEBUG_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:Coin>")
else()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  set(COIN_RELEASE_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:Coin>")
  set(COIN_DEBUG_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:Coin>")
endif()

# Essential header checks only
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(time.h HAVE_TIME_H)
check_include_files("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)

# Platform-specific detection (simplified)
check_include_file(windows.h HAVE_WINDOWS_H)
if(HAVE_WINDOWS_H)
  check_include_file(direct.h HAVE_DIRECT_H)
  check_symbol_exists(LoadLibrary windows.h HAVE_WINDLL_RUNTIME_BINDING)
else()
  # Dynamic library loading support
  check_include_file(dlfcn.h HAVE_DLFCN_H)
  if(HAVE_DLFCN_H)
    set(HAVE_DL_LIB 1)
    list(APPEND COIN_TARGET_LINK_LIBRARIES ${CMAKE_DL_LIBS})
  endif()
endif()

# OpenGL header detection (simplified)
check_include_file(GL/gl.h HAVE_GL_GL_H)
check_include_file(OpenGL/gl.h HAVE_OPENGL_GL_H)
check_include_files("GL/gl.h;GL/glext.h" HAVE_GL_GLEXT_H)
check_include_files("OpenGL/gl.h;OpenGL/glext.h" HAVE_OPENGL_GLEXT_H)

if(HAVE_WINDOWS_H)
  set(SIM_INCLUDE_WINDOWS_H "#include <windows.h>")
else()
  set(SIM_INCLUDE_WINDOWS_H "/* #include <windows.h> - not needed on system */")
endif()

if(COIN3D_USE_OSMESA)
  # starseeker/osmesa backend - use starseeker/osmesa headers only.
  # OSMesa's gl.h already includes OSMesa/glext.h, so no system glext needed.
  # Mixing system GL/glext.h with OSMesa's MGL-mangled headers causes symbol
  # conflicts that corrupt the heap at runtime.
  set(SIM_INCLUDE_GL_H "#include <OSMesa/osmesa.h>
  #include <OSMesa/gl.h>")
  set(SIM_INCLUDE_GLEXT_H "/* OSMesa/gl.h already includes OSMesa/glext.h - system GL/glext.h must NOT be included */")
  message(STATUS "Using starseeker/osmesa headers for GL with automatic MGL name mangling")
else()
  # System OpenGL backend
  if(HAVE_GL_GL_H)
    set(SIM_INCLUDE_GL_H "#include <GL/gl.h>")
  elseif(HAVE_OPENGL_GL_H)
    set(SIM_INCLUDE_GL_H "#include <OpenGL/gl.h>")
  else()
    set(SIM_INCLUDE_GL_H "#error \"don't know how to include gl.h header\"")
  endif()
  if(HAVE_GL_GLEXT_H)
    set(SIM_INCLUDE_GLEXT_H "#include <GL/glext.h>")
  elseif(HAVE_OPENGL_GLEXT_H)
    set(SIM_INCLUDE_GLEXT_H "#include <OpenGL/glext.h>")
  else()
    set(SIM_INCLUDE_GLEXT_H "/* #include <GL/glext.h> - not found on system */")
  endif()
endif()

# getcwd detection (POSIX vs Windows)
check_symbol_exists(getcwd unistd.h HAVE_GETCWD)
if(NOT HAVE_GETCWD)
  check_symbol_exists(_getcwd direct.h HAVE__GETCWD)
endif()

# C++ function name support
check_symbol_exists(__func__ "" FUNC)
if(FUNC)
  set(HAVE_C_COMPILER_FUNCTION_NAME_VAR __func__)
  set(HAVE_CPP_COMPILER_FUNCTION_NAME_VAR __func__)
else()
  check_symbol_exists(__FUNCTION__ "" FUNCTION)
  if(FUNCTION)
    set(HAVE_C_COMPILER_FUNCTION_NAME_VAR __FUNCTION__)
    set(HAVE_CPP_COMPILER_FUNCTION_NAME_VAR __FUNCTION__)
  endif()
endif()

if(NOT HAVE_UNISTD_H)
  add_definitions(-DYY_NO_UNISTD_H)
endif()

# Dynamic linking capability detection (simplified)
if(HAVE_WINDLL_RUNTIME_BINDING OR HAVE_DL_LIB)
  set(HAVE_DYNAMIC_LINKING 1)
endif()

# Simple feature flags
if(HAVE_THREADS)
  set(FEAT_HAVE_THREADS 1)
else()
  set(FEAT_HAVE_THREADS 0)
endif()
if(COIN_THREADSAFE)
  set(FEAT_HAVE_SAFETHREAD 1)
else()
  set(FEAT_HAVE_SAFETHREAD 0)
endif()

# ============================================================================
# Code coverage instrumentation flags
#
# When OBOL_COVERAGE=ON (and the compiler is GCC or Clang), add --coverage
# to the compile and link flags for the entire project.  This must be done
# BEFORE the source subdirectories are added so that the Coin library and all
# test executables are instrumented.
#
# After building, run:
#   cmake --build <dir> --target coverage
# to execute the test suite and generate an lcov HTML report.
# ============================================================================
if(OBOL_COVERAGE)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" OR
     CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    find_program(LCOV_EXECUTABLE    lcov    DOC "lcov coverage tool")
    find_program(GENHTML_EXECUTABLE genhtml DOC "genhtml report generator")
    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
      message(STATUS "Code coverage instrumentation enabled")
      message(STATUS "  lcov:    ${LCOV_EXECUTABLE}")
      message(STATUS "  genhtml: ${GENHTML_EXECUTABLE}")
      add_compile_options(--coverage)
      add_link_options(--coverage)
    else()
      message(WARNING
        "OBOL_COVERAGE=ON but lcov/genhtml not found – coverage disabled. "
        "Install with: sudo apt-get install lcov")
      set(OBOL_COVERAGE OFF CACHE BOOL "" FORCE)
    endif()
  else()
    message(WARNING
      "OBOL_COVERAGE=ON but compiler is not GCC/Clang – coverage disabled.")
    set(OBOL_COVERAGE OFF CACHE BOOL "" FORCE)
  endif()
endif()

# ============================================================================
# Add subdirectories
# ============================================================================

add_subdirectory(include)
add_subdirectory(src)

if(OBOL_BUILD_VIEWER)
  # ---- FLTK ----------------------------------------------------------------
  find_package(FLTK REQUIRED)

  # ---- Viewer executable ---------------------------------------------------
  add_executable(obol_viewer tests/obol_viewer.cpp)

  target_include_directories(obol_viewer PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/utils     # headless_utils.h
    ${COIN_TARGET_INCLUDE_DIRECTORIES}
    ${FLTK_INCLUDE_DIRS}
  )

  # Compile definitions matching the Coin library build
  if(COIN3D_BUILD_DUAL_GL)
    target_compile_definitions(obol_viewer PRIVATE COIN3D_BUILD_DUAL_GL)
    # Dual-GL builds enable the OSMesa panel (OBOL_VIEWER_OSMESA_PANEL defined
    # in obol_viewer.cpp when COIN3D_BUILD_DUAL_GL is set).  The panel uses
    # SoDB::createOSMesaContextManager() – no direct OSMesa dependency needed.
  elseif(COIN3D_USE_OSMESA)
    # Pure-OSMesa build: headless_utils.h uses OSMesa context manager
    target_compile_definitions(obol_viewer PRIVATE COIN3D_OSMESA_BUILD)
    target_include_directories(obol_viewer PRIVATE
      "${PROJECT_SOURCE_DIR}/external/osmesa/include")
  endif()

  # Optional NanoRT panel: enabled when nanort.h is found in external/nanort/
  if(EXISTS "${PROJECT_SOURCE_DIR}/external/nanort/nanort.h")
    target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_NANORT)
    target_include_directories(obol_viewer PRIVATE
      "${PROJECT_SOURCE_DIR}/external/nanort"
      "${PROJECT_SOURCE_DIR}/tests/utils"   # nanort_context_manager.h
    )
    message(STATUS "obol_viewer: NanoRT panel enabled (external/nanort/nanort.h found)")
  else()
    message(STATUS "obol_viewer: NanoRT panel disabled (external/nanort/nanort.h not found)")
  endif()

  target_link_libraries(obol_viewer
    PRIVATE
      Coin
      ${COIN_TARGET_LINK_LIBRARIES}
      ${FLTK_LIBRARIES}
  )
  # On Linux the viewer includes headless_utils.h which uses GLX symbols for
  # the system-GL path; link the X/GL libs explicitly when available.
  if(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    if(X11_FOUND)
      target_link_libraries(obol_viewer PRIVATE ${X11_X11_LIB})
    endif()
    if(TARGET OpenGL::GL)
      target_link_libraries(obol_viewer PRIVATE OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
      target_link_libraries(obol_viewer PRIVATE ${OPENGL_gl_LIBRARY})
    endif()
  endif()

  install(TARGETS obol_viewer RUNTIME DESTINATION bin)
endif() # OBOL_BUILD_VIEWER

if(OBOL_BUILD_TESTS)
  # Build libpng from the bundled submodule so that rgb_to_png and
  # image_comparator can write / compare PNG control images without
  # requiring a system-installed libpng-dev package.
  if(EXISTS "${PROJECT_SOURCE_DIR}/external/libpng/CMakeLists.txt")
    set(PNG_SHARED      OFF CACHE BOOL "Build libpng shared library" FORCE)
    set(PNG_STATIC      ON  CACHE BOOL "Build libpng static library" FORCE)
    set(PNG_TESTS       OFF CACHE BOOL "Build libpng tests"          FORCE)
    set(PNG_TOOLS       OFF CACHE BOOL "Build libpng tools"          FORCE)
    set(PNG_EXECUTABLES OFF CACHE BOOL "Build libpng executables"    FORCE)
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/libpng" libpng_build
                     EXCLUDE_FROM_ALL)
    # Expose the built library to the tests subdirectory via standard
    # find_package(PNG) variables so no changes are needed in tests/.
    set(PNG_FOUND        TRUE  CACHE BOOL   "" FORCE)
    set(PNG_INCLUDE_DIRS
          "${PROJECT_SOURCE_DIR}/external/libpng"
          "${CMAKE_BINARY_DIR}/libpng_build"
        CACHE STRING "" FORCE)
    set(PNG_LIBRARIES    png_static CACHE STRING "" FORCE)
    message(STATUS "Using bundled libpng from external/libpng")
  endif()

  enable_testing()
  add_subdirectory(tests)
  add_subdirectory(Mentor)
endif()

# ============================================================================
# Code coverage report target (lcov + genhtml)
# Enabled when -DOBOL_COVERAGE=ON (see flags block above, before add_subdirectory).
#
# Usage:
#   cmake -DOBOL_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug ...
#   cmake --build <build_dir>
#   cmake --build <build_dir> --target coverage
#
# The target:
#   1. Resets all coverage counters.
#   2. Runs ctest (all registered tests).
#   3. Captures coverage data with lcov, filtering out system headers and
#      third-party code (external/, upstream/).
#   4. Generates an HTML report in <build>/coverage_report/.
# ============================================================================
if(OBOL_COVERAGE AND LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
  set(COVERAGE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/coverage_report")
  set(COVERAGE_INFO_FILE  "${CMAKE_BINARY_DIR}/coverage.info")

  add_custom_target(coverage
    COMMENT "Collecting lcov coverage data and generating HTML report"

    # 1. Reset counters
    COMMAND ${LCOV_EXECUTABLE}
        --zerocounters
        --directory "${CMAKE_BINARY_DIR}"

    # 2. Run all registered tests; continue even if some tests fail so that
    #    coverage data is captured from passing tests.  A CMake script wrapper
    #    is used to execute ctest and suppress its non-zero exit code.
    COMMAND ${CMAKE_COMMAND}
        -DCTEST_COMMAND=${CMAKE_CTEST_COMMAND}
        -DBUILD_CONFIG=$<CONFIG>
        -DTEST_TIMEOUT=120
        -P "${CMAKE_CURRENT_LIST_DIR}/cmake/run_tests_for_coverage.cmake"

    # 3. Capture coverage data
    COMMAND ${LCOV_EXECUTABLE}
        --capture
        --directory "${CMAKE_BINARY_DIR}"
        --output-file "${COVERAGE_INFO_FILE}"
        --rc branch_coverage=1
        --ignore-errors mismatch

    # 4. Remove noise: system headers and external / upstream subtrees
    COMMAND ${LCOV_EXECUTABLE}
        --remove "${COVERAGE_INFO_FILE}"
        "/usr/*"
        "${CMAKE_SOURCE_DIR}/external/*"
        "${CMAKE_SOURCE_DIR}/upstream/*"
        --output-file "${COVERAGE_INFO_FILE}"
        --ignore-errors unused

    # 5. Generate HTML report
    COMMAND ${GENHTML_EXECUTABLE}
        "${COVERAGE_INFO_FILE}"
        --output-directory "${COVERAGE_OUTPUT_DIR}"
        --branch-coverage
        --title "Obol Coverage Report"

    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  )

  message(STATUS
    "Run 'cmake --build <dir> --target coverage' to generate the HTML report")
  message(STATUS "  Report will be written to: ${COVERAGE_OUTPUT_DIR}")
endif()

# ============================================================================
# Feature summary
# ============================================================================

add_feature_info(ThreadSafe COIN_THREADSAFE "Thread safe render traversals")
add_feature_info(Exceptions USE_EXCEPTIONS "Compile with exceptions")

feature_summary(WHAT ALL
  INCLUDE_QUIET_PACKAGES
  DESCRIPTION "Enabled Features:"
  VAR enabledFeaturesText)
message(STATUS "${enabledFeaturesText}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8 cino=N-s
