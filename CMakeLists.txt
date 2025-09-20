cmake_minimum_required(VERSION 3.16...3.31)

# Modern CMake project definition with metadata
project(Coin
  VERSION 4.0.5
  DESCRIPTION "A high-level 3D visualization library with Open Inventor 2.1 API"
  HOMEPAGE_URL "https://github.com/coin3d/coin"
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
option(COIN_BUILD_TESTS "Build unit tests" ON)
option(COIN_BUILD_EXAMPLES "Build examples" OFF)

# Compatibility alias for legacy code
set(COIN_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Documentation options removed - not needed for minimal library build

# Feature options
option(COIN3D_USE_OSMESA "Build against OSMesa for offscreen/headless rendering" OFF)
option(COIN_THREADSAFE "Enable thread safe render traversals" OFF)

# Component options
option(HAVE_NODEKITS "Enable NodeKit support" ON)
option(HAVE_DRAGGERS "Enable Dragger support" ON)
option(HAVE_MANIPULATORS "Enable Manipulator support" ON)

# External dependencies
option(USE_EXCEPTIONS "Compile with exceptions (g++ only)" ON)

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
  # System OpenGL backend (default)
  find_package(OpenGL REQUIRED)
  set(HAVE_OPENGL 1)

  if(OpenGL_OpenGL_FOUND)
    list(APPEND COIN_TARGET_LINK_LIBRARIES_GL OpenGL::OpenGL)
  else()
    list(APPEND COIN_TARGET_LINK_LIBRARIES_GL OpenGL::GL)
  endif()

  if(OpenGL_GLU_FOUND)
    set(HAVE_GLU 1)
    list(APPEND COIN_TARGET_LINK_LIBRARIES_GL OpenGL::GLU)
  endif()

  # macOS specific settings
  if(APPLE)
    set(GLU_IS_PART_OF_GL 1)
  endif()

  message(STATUS "Using system OpenGL")
endif()

list(APPEND COIN_TARGET_LINK_LIBRARIES ${COIN_TARGET_LINK_LIBRARIES_GL})

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

# Modern fixed-width integer types (C++17)
set(COIN_INT8_T "int8_t")
set(COIN_UINT8_T "uint8_t")
set(COIN_INT16_T "int16_t")
set(COIN_UINT16_T "uint16_t")
set(COIN_INT32_T "int32_t")
set(COIN_UINT32_T "uint32_t")
set(COIN_INT64_T "int64_t")
set(COIN_UINT64_T "uint64_t")
set(COIN_INTPTR_T "intptr_t")
set(COIN_UINTPTR_T "uintptr_t")

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
check_include_file(GL/glu.h HAVE_GL_GLU_H)
check_include_file(OpenGL/glu.h HAVE_OPENGL_GLU_H)
check_include_files("GL/gl.h;GL/glext.h" HAVE_GL_GLEXT_H)
check_include_files("OpenGL/gl.h;OpenGL/glext.h" HAVE_OPENGL_GLEXT_H)

if(HAVE_WINDOWS_H)
  set(SIM_INCLUDE_WINDOWS_H "#include <windows.h>")
else()
  set(SIM_INCLUDE_WINDOWS_H "/* #include <windows.h> - not needed on system */")
endif()

if(COIN3D_USE_OSMESA)
  # starseeker/osmesa backend - use starseeker/osmesa headers (MGL mangling is automatic when USE_MGL_NAMESPACE is defined)
  set(SIM_INCLUDE_GL_H "#include <OSMesa/osmesa.h>
  #include <OSMesa/gl.h>")
  set(SIM_INCLUDE_GLU_H "/* #include <GL/glu.h> - GLU not typically used with OSMesa */")
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
  if(HAVE_SUPERGLU)
    set(SIM_INCLUDE_GLU_H "/* #include <GL/glu.h> - not used, Coin linked with embedded SuperGLU */")
  elseif(HAVE_GL_GLU_H)
    set(SIM_INCLUDE_GLU_H "#include <GL/glu.h>")
  elseif(HAVE_OPENGL_GLU_H)
    set(SIM_INCLUDE_GLU_H "#include <OpenGL/glu.h>")
  else()
    set(SIM_INCLUDE_GLU_H "/* #include <GL/glu.h> - not found on system */")
  endif()
endif()
if(HAVE_GL_GLEXT_H)
  set(SIM_INCLUDE_GLEXT_H "#include <GL/glext.h>")
elseif(HAVE_OPENGL_GLEXT_H)
  set(SIM_INCLUDE_GLEXT_H "#include <OpenGL/glext.h>")
else()
  set(SIM_INCLUDE_GLEXT_H "/* #include <GL/glext.h> - not found on system */")
endif()

# Essential function checks only (simplified for modern C++)
check_symbol_exists(va_copy stdarg.h HAVE_VA_COPY_MACRO)
check_symbol_exists(memmove string.h HAVE_MEMMOVE)
check_symbol_exists(fstat "sys/stat.h;sys/types.h" HAVE_FSTAT)
if(NOT HAVE_FSTAT)
  check_symbol_exists(_fstat "sys/stat.h;sys/types.h" HAVE__FSTAT)
endif()
check_symbol_exists(getcwd unistd.h HAVE_GETCWD)
if(NOT HAVE_GETCWD)
  check_symbol_exists(_getcwd direct.h HAVE__GETCWD)
endif()

# Math functions (C++17 should provide these, but check for fallbacks)
check_symbol_exists(isinf math.h HAVE_ISINF)
check_symbol_exists(isnan math.h HAVE_ISNAN)
check_symbol_exists(finite math.h HAVE_FINITE)
if(NOT HAVE_FINITE)
  check_symbol_exists(_finite float.h HAVE__FINITE)
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
# Add subdirectories
# ============================================================================

add_subdirectory(include)
add_subdirectory(src)

if(COIN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(COIN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(testsuite)  # Legacy threading tests
  add_subdirectory(tests)      # Main Catch2-based test system
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
