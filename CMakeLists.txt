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
if(POLICY CMP0118)
  cmake_policy(SET CMP0118 NEW)  # GENERATED property is globally visible (needed for build-dir parser files)
endif()

# Project variables for compatibility
set(OBOL_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set(OBOL_MINOR_VERSION ${PROJECT_VERSION_MINOR})
set(OBOL_MICRO_VERSION ${PROJECT_VERSION_PATCH})
set(OBOL_BETA_VERSION)
set(OBOL_VERSION ${PROJECT_VERSION})
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)

string(TIMESTAMP OBOL_BUILD_YEAR "%Y")
math(EXPR OBOL_SO_VERSION ${PROJECT_VERSION_MAJOR}*20)

# Enforce out-of-source builds
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "${CMAKE_PROJECT_NAME} requires an out-of-source build. Please create a separate build directory and run 'cmake <path_to_${CMAKE_PROJECT_NAME}> [options]' from there.")
endif()

# ============================================================================
# Git submodule helper
# ============================================================================
# Try to initialise a single git submodule at configure time.
# Call this before using any path that may live in a submodule.
#
#   obol_init_submodule("external/fltk")
#
# Behaviour:
#   1. If <path>/CMakeLists.txt already exists → no-op (already populated).
#   2. Otherwise, check if the path is registered as a gitlink in the git
#      index (mode 160000).  Only if it is do we run
#      "git submodule update --init <path>" — this avoids accidentally
#      kicking off a full network clone for a submodule that hasn't been
#      registered yet.
#   3. If git is unavailable or the command fails → emit a WARNING and
#      continue; callers are expected to gracefully degrade.
macro(obol_init_submodule submodule_path)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${submodule_path}/CMakeLists.txt")
    find_program(_obol_git git)
    if(_obol_git)
      # Check whether this path is actually registered as a git submodule
      # (i.e. has a 160000 gitlink entry in the index).  Without the gitlink,
      # "git submodule update --init" would try to clone from the URL in
      # .gitmodules, which we do NOT want to trigger silently at configure
      # time when the developer hasn't explicitly set up the submodule.
      execute_process(
        COMMAND ${_obol_git} ls-files --stage -- ${submodule_path}
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE _gitlink_output
        RESULT_VARIABLE _gitlink_result
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      if(_gitlink_result EQUAL 0 AND _gitlink_output MATCHES "^160000 ")
        # Registered gitlink found → safe to initialise
        message(STATUS "Initialising git submodule: ${submodule_path}")
        execute_process(
          COMMAND ${_obol_git} submodule update --init -- ${submodule_path}
          WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
          RESULT_VARIABLE _submodule_result
          OUTPUT_QUIET
          ERROR_QUIET
        )
        if(NOT _submodule_result EQUAL 0)
          message(WARNING
            "Failed to initialise submodule ${submodule_path}. "
            "Run: git submodule update --init ${submodule_path}")
        endif()
        unset(_submodule_result)
      else()
        # Not yet registered as a gitlink — skip silently.
        # Callers will detect the missing directory and degrade gracefully.
      endif()
      unset(_gitlink_output)
      unset(_gitlink_result)
    endif()
    unset(_obol_git)
  endif()
endmacro()

# Ensure the OSMesa submodule is present (needed for osmesa and dual-GL builds).
# This is a no-op when osmesa files are already on disk (bundled or previously
# initialised).
obol_init_submodule("external/osmesa")

# Set C++ standard requirements
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "Setting C++ standard: C++${CMAKE_CXX_STANDARD} (required)")

# Add path for Obol-specific utility scripts
list(PREPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Standard CMake modules
include(CheckCCompilerFlag)
include(CheckCSourceRuns)
include(CheckCXXCompilerFlag)
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

# FLTK GL context manager for obol_viewer.
# The actual compile-definition is set for the obol_viewer target in
# examples/fltk/CMakeLists.txt.
option(OBOL_VIEWER_USE_FLTK_GL
    "Use FLTK GL context for obol_viewer system-GL panel (portable; disable for CI/headless)"
    ON)

# ---------------------------------------------------------------------------
# FLTK detection and submodule fallback
#
# Priority:
#   1. System FLTK (libfltk1.3-dev / libfltk-dev)
#   2. external/fltk git submodule (populated at configure time when available)
#   3. Disabled — FLTK-based viewers are silently skipped
#
# The git submodule is declared in .gitmodules and points to
# https://github.com/fltk/fltk (branch-1.3).  It is initialised
# automatically at configure time when system FLTK is absent.
# ---------------------------------------------------------------------------
find_package(FLTK QUIET)
if(FLTK_FOUND)
  message(STATUS "FLTK: using system installation (${FLTK_INCLUDE_DIRS})")
else()
  # System FLTK not found.  Try to populate the submodule fallback.
  obol_init_submodule("external/fltk")
  if(EXISTS "${PROJECT_SOURCE_DIR}/external/fltk/CMakeLists.txt")
    message(STATUS "FLTK: using external/fltk submodule")
    set(FLTK_BUILD_FLUID        OFF CACHE BOOL "Build FLTK fluid tool"         FORCE)
    set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL "Build FLTK fltk-options tool"  FORCE)
    set(FLTK_BUILD_TEST         OFF CACHE BOOL "Build FLTK test/demo programs"  FORCE)
    set(FLTK_BUILD_EXAMPLES     OFF CACHE BOOL "Build FLTK example programs"    FORCE)
    set(FLTK_BUILD_FORMS        OFF CACHE BOOL "Build FLTK forms library"       FORCE)
    if(OBOL_VIEWER_USE_FLTK_GL)
      set(FLTK_BUILD_GL         ON  CACHE BOOL "Build FLTK OpenGL library"      FORCE)
    else()
      set(FLTK_BUILD_GL         OFF CACHE BOOL "Build FLTK OpenGL library"      FORCE)
    endif()
    set(FLTK_BUILD_SHARED_LIBS  OFF CACHE BOOL "Build FLTK shared libraries"    FORCE)
    add_subdirectory("${PROJECT_SOURCE_DIR}/external/fltk" fltk_build EXCLUDE_FROM_ALL)
    set(FLTK_FOUND TRUE)
    set(FLTK_LIBRARIES fltk::fltk)
    if(OBOL_VIEWER_USE_FLTK_GL AND TARGET fltk::gl)
      list(APPEND FLTK_LIBRARIES fltk::gl)
    endif()
    set(FLTK_INCLUDE_DIRS
        "${PROJECT_SOURCE_DIR}/external/fltk"
        "${CMAKE_BINARY_DIR}/fltk_build")
  else()
    if(OBOL_BUILD_TESTS)
      message(STATUS "FLTK: not found (no system package, no external/fltk submodule) – FLTK viewers disabled")
    endif()
  endif()
endif()

# obol_viewer uses libObolEx for its scene catalogue, so it can only be
# built when OBOL_BUILD_TESTS is also ON.  cmake_dependent_option enforces
# this: the viewer is silently disabled if FLTK is absent or tests are off.
cmake_dependent_option(OBOL_BUILD_VIEWER
  "Build the FLTK scene viewer (obol_viewer); requires FLTK and OBOL_BUILD_TESTS"
  "${FLTK_FOUND}"
  "OBOL_BUILD_TESTS;FLTK_FOUND"
  OFF)

# Qt scene viewer (obol_qt_viewer)
# Requires Qt6 Widgets + Gui modules and OBOL_BUILD_TESTS.
# Auto-detected: enabled when Qt6 is found and OBOL_BUILD_TESTS is ON.
find_package(Qt6 QUIET COMPONENTS Widgets Gui)
if(OBOL_BUILD_TESTS AND Qt6_FOUND)
  message(STATUS "Qt6 found (${Qt6_VERSION}): obol_qt_viewer can be built")
  set(QT_VIEWER_DEFAULT ON)
else()
  if(OBOL_BUILD_TESTS)
    message(STATUS "Qt6 not found – obol_qt_viewer disabled")
  endif()
  set(QT_VIEWER_DEFAULT OFF)
endif()
cmake_dependent_option(OBOL_BUILD_QT_VIEWER
  "Build the Qt scene viewer (obol_qt_viewer); requires Qt6 and OBOL_BUILD_TESTS"
  "${QT_VIEWER_DEFAULT}"
  "OBOL_BUILD_TESTS;Qt6_FOUND"
  OFF)

# Internal alias used throughout src/ for Windows shared/static detection
set(OBOL_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Documentation: optional Doxygen API reference
option(OBOL_BUILD_DOCS "Build Doxygen API documentation" OFF)
if(OBOL_BUILD_DOCS)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    configure_file(
      "${PROJECT_SOURCE_DIR}/docs/Doxyfile.in"
      "${CMAKE_BINARY_DIR}/docs/Doxyfile"
      @ONLY)
    add_custom_target(obol_docs
      COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/docs/Doxyfile"
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      COMMENT "Generating Obol API documentation with Doxygen"
      VERBATIM)
    message(STATUS "Doxygen found: API docs target 'obol_docs' is available")
  else()
    message(WARNING "OBOL_BUILD_DOCS=ON but Doxygen was not found; docs target will not be created")
  endif()
endif()

# Feature options
# Backend selection:
#   OBOL_USE_OSMESA=ON          → OSMesa-only (headless, no system GL)
#   OBOL_USE_SYSTEM_ONLY=ON     → system OpenGL only (no OSMesa)
#   default (both OFF)            → auto-detect; dual mode when both are available
option(OBOL_USE_OSMESA "Build against OSMesa only for offscreen/headless rendering" OFF)
option(OBOL_USE_SYSTEM_ONLY "Build against system OpenGL only (no OSMesa fallback)" OFF)
option(OBOL_BUILD_DUAL_GL "Build with both system-OpenGL and OSMesa backends in a single library" OFF)
option(OBOL_THREADSAFE "Enable thread safe render traversals" ON)

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

# Extra warning flags for the Obol library target (GCC/Clang only).
# Applied via target_compile_options so external dependencies are unaffected.
#
# Flags applied when OBOL_WARNINGS=ON:
#   -Wall               catch a broad set of common problems
#   -Wextra             additional checks beyond -Wall
#   -Wundef             warn when an undefined identifier is used in #if
#   -Wshadow            warn when a local variable shadows a member or outer variable
#   -Wno-long-long      suppress warnings about 'long long' (valid in C++11 and later)
#   -Wpedantic          enforce strict ISO C++17.  The codebase now uses [[maybe_unused]]
#                       (C++17) instead of __attribute__((__unused__)).  The generated
#                       parser files (evaluator_tab.cpp + so_eval.ic) use internal
#                       __attribute__ annotations from the bison/flex skeletons; these
#                       are guarded by the tools' own ifdef machinery and do not require
#                       any suppression.
#   -Wformat-nonliteral warn when a format string argument to printf/strftime is not a
#                       string literal and cannot be checked at compile time.  The two
#                       intentional pass-through strftime calls in SbTime::formatDate()
#                       are suppressed with a local GCC diagnostic pragma.
#   -Wmaybe-uninitialized  warn when a variable may be used before it is initialized.
#   -Wmissing-declarations  warn when a non-static function has no prior declaration.
#                           All 30 instances were fixed: static functions gained the
#                           static keyword; cross-TU helpers gained forward declarations;
#                           missing header declarations were added to glp.h and
#                           SoNodeKitPath.h.
#   -Wredundant-decls       warn when a symbol is declared more than once in the same
#                           scope.  All 80 instances were fixed: the duplicate
#                           SbAtexitStaticInternal declaration was removed from
#                           CoinTidbits.h (SuUtils.h is now included instead);
#                           redundant forward decls in SoGLDriverDatabase.cpp and glp.h
#                           were removed; the flex-generated so_eval.ic include is
#                           wrapped with a diagnostic pragma.
#
# Flags intentionally NOT applied and why (sourced from brlcad_CMakeLists.txt):
#   -Wfloat-equal  Not viable: 3D geometry code legitimately compares floating-
#                  point values against exact constants (0.0, 1.0) to test for
#                  degenerate cases (zero-length vectors, identity matrices, etc.).
#                  Suppressing all 300+ such comparisons would bloat the code
#                  with meaningless casts and obscure the intent.
#   -Wbad-function-cast  C-only flag; not applicable to the C++ Obol library.
#   -Wc++-compat         C-only flag; not applicable.
#   -Winline       Too noisy: brlcad explicitly uses -Wno-inline for g++ because
#                  GCC spews unquellable bogus warnings on default constructors.
#   -Wdocumentation  Clang only; currently produces ~9000 warnings against the
#                    Doxygen comment headers.  Not viable until the doc comments
#                    are brought into conformance.
#   -Wmissing-field-initializers  ~207 warnings; deferred to a future clean-up pass.
option(OBOL_WARNINGS "Enable extra warning flags (-Wall -Wextra -Wundef -Wshadow -Wpedantic -Wformat-nonliteral -Wmaybe-uninitialized -Wmissing-declarations -Wredundant-decls -Wcast-qual) on the Obol target" ON)

if(OBOL_WARNINGS AND NOT MSVC)
  set(_obol_warning_flags "")
  foreach(_flag Wall Wextra Wundef Wshadow Wno-long-long Wpedantic Wformat-nonliteral Wmaybe-uninitialized Wmissing-declarations Wredundant-decls Wcast-qual)
    check_cxx_compiler_flag("-${_flag}" OBOL_CXX_HAS_${_flag})
    if(OBOL_CXX_HAS_${_flag})
      list(APPEND _obol_warning_flags "-${_flag}")
    endif()
  endforeach()
  set(OBOL_WARNING_FLAGS "${_obol_warning_flags}" CACHE INTERNAL
      "Compiler warning flags applied to the Obol library target")
  if(OBOL_WARNING_FLAGS)
    message(STATUS "Obol warning flags: ${OBOL_WARNING_FLAGS}")
  endif()
endif()

# Build optimization options
option(OBOL_USE_UNITY_BUILD "Batch source files into unity build units for faster compilation (CMake 3.16+)" ON)

# Thread debugging (requires C++11 thread support)
check_include_file_cxx(thread HAVE_STD_THREAD)

# ============================================================================
# Installation paths configuration
# ============================================================================

# Obol-specific data directory
set(CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")

# ============================================================================
# Dependency management
# ============================================================================

# Initialize lists for target properties (modern approach)
set(OBOL_TARGET_LINK_LIBRARIES)
set(OBOL_TARGET_INCLUDE_DIRECTORIES)
set(OBOL_TARGET_LINK_LIBRARIES_GL)

# OpenGL backend selection
# -----------------------------------------------------------------------
# Auto-detection when neither OSMESA-only nor SYSTEM-only is forced:
#   1. Check for system OpenGL and for the OSMesa submodule
#   2. If both are available → enable DUAL GL (one library, both backends)
#   3. If only OSMesa submodule is available → OSMesa-only mode
#   4. If only system GL is available → system-only mode
# The user can override by setting OBOL_USE_OSMESA=ON (osmesa only)
# or OBOL_USE_SYSTEM_ONLY=ON (system GL only) before configuring.
#
# NOTE: system libosmesa6-dev is intentionally NOT used for auto-detection.
# The project requires its own name-mangled OSMesa build (external/osmesa
# submodule with USE_MGL_NAMESPACE).  The submodule include path
# <OSMesa/osmesa.h> was chosen specifically to avoid pulling in the
# system header at <GL/osmesa.h>, which uses un-mangled gl* symbols
# incompatible with a dual system-GL+OSMesa build.
# -----------------------------------------------------------------------
if(NOT OBOL_USE_OSMESA AND NOT OBOL_USE_SYSTEM_ONLY AND NOT OBOL_BUILD_DUAL_GL)
  find_package(OpenGL QUIET)
  # "OSMesa available" means our submodule is present (not system osmesa)
  if(EXISTS "${PROJECT_SOURCE_DIR}/external/osmesa/CMakeLists.txt")
    set(_have_osmesa_submodule TRUE)
  else()
    set(_have_osmesa_submodule FALSE)
  endif()
  if(OpenGL_FOUND AND _have_osmesa_submodule)
    set(OBOL_BUILD_DUAL_GL ON CACHE BOOL
        "Build with both system-OpenGL and OSMesa backends in a single library" FORCE)
    message(STATUS "Auto-detected system OpenGL + OSMesa submodule: enabling DUAL GL mode")
  elseif(_have_osmesa_submodule)
    set(OBOL_USE_OSMESA ON CACHE BOOL
        "Build against OSMesa only for offscreen/headless rendering" FORCE)
    message(STATUS "Auto-detected OSMesa submodule only: enabling OSMesa-only mode")
  else()
    # System-only (OpenGL required below will error if not present)
    message(STATUS "Auto-detected: system OpenGL only (no OSMesa submodule found)")
  endif()
endif()

if(OBOL_USE_OSMESA)
  # OSMesa backend for offscreen/headless rendering
  find_package(OSMesa REQUIRED)
  if(OSMesa_FOUND)
    set(HAVE_OPENGL 1)
    set(HAVE_OSMESA 1)

    if(TARGET OSMesa::OSMesa)
      list(APPEND OBOL_TARGET_LINK_LIBRARIES_GL OSMesa::OSMesa)
    elseif(TARGET osmesa_interface)
      # starseeker/osmesa submodule interface wrapper
      list(APPEND OBOL_TARGET_LINK_LIBRARIES_GL osmesa_interface)
      list(APPEND OBOL_TARGET_INCLUDE_DIRECTORIES ${OSMesa_INCLUDE_DIRS})
    else()
      list(APPEND OBOL_TARGET_INCLUDE_DIRECTORIES ${OSMesa_INCLUDE_DIRS})
      list(APPEND OBOL_TARGET_LINK_LIBRARIES_GL ${OSMesa_LIBRARIES})
    endif()

    message(STATUS "Using OSMesa for offscreen/headless rendering")
  endif()
else()
  # System OpenGL backend (default or SYSTEM_ONLY)
  find_package(OpenGL REQUIRED)
  set(HAVE_OPENGL 1)

  if(OpenGL_OpenGL_FOUND AND TARGET OpenGL::OpenGL)
    list(APPEND OBOL_TARGET_LINK_LIBRARIES_GL OpenGL::OpenGL)
  elseif(OpenGL_GL_FOUND AND TARGET OpenGL::GL)
    list(APPEND OBOL_TARGET_LINK_LIBRARIES_GL OpenGL::GL)
  elseif(OPENGL_gl_LIBRARY)
    list(APPEND OBOL_TARGET_LINK_LIBRARIES_GL ${OPENGL_gl_LIBRARY})
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
        list(APPEND OBOL_TARGET_LINK_LIBRARIES_GL ${X11_X11_LIB})
        list(APPEND OBOL_TARGET_INCLUDE_DIRECTORIES ${X11_INCLUDE_DIR})
        message(STATUS "GLX offscreen rendering support enabled")
      endif()
    endif()
  endif()

  message(STATUS "Using system OpenGL")
endif()

list(APPEND OBOL_TARGET_LINK_LIBRARIES ${OBOL_TARGET_LINK_LIBRARIES_GL})

# Dual-GL build: also find and store OSMesa libraries separately so
# gl_osmesa.cpp can be compiled against them alongside system GL.
# Our FindOSMesa.cmake only looks at the external/osmesa submodule, which
# provides the name-mangled (mgl*) build required by dual GL.
if(OBOL_BUILD_DUAL_GL)
  if(OBOL_USE_OSMESA)
    message(FATAL_ERROR "OBOL_BUILD_DUAL_GL and OBOL_USE_OSMESA are mutually exclusive. "
            "DUAL GL uses system OpenGL as primary with OSMesa as a secondary backend.")
  endif()
  find_package(OSMesa REQUIRED)
  if(NOT OSMesa_FOUND)
    message(FATAL_ERROR "OBOL_BUILD_DUAL_GL requires the external/osmesa submodule.\n"
            "Run: git submodule update --init --recursive")
  endif()
  # Collect OSMesa include dirs and link libraries for the per-file target
  if(TARGET OSMesa::OSMesa)
    get_target_property(_osmesa_inc OSMesa::OSMesa INTERFACE_INCLUDE_DIRECTORIES)
    set(OBOL_OSMESA_INCLUDE_DIRS "${_osmesa_inc}")
    set(OBOL_OSMESA_LIBRARIES OSMesa::OSMesa)
  elseif(TARGET osmesa_interface)
    set(OBOL_OSMESA_INCLUDE_DIRS "${OSMesa_INCLUDE_DIRS}")
    set(OBOL_OSMESA_LIBRARIES osmesa_interface)
  else()
    set(OBOL_OSMESA_INCLUDE_DIRS "${OSMesa_INCLUDE_DIRS}")
    set(OBOL_OSMESA_LIBRARIES "${OSMesa_LIBRARIES}")
  endif()
  add_definitions(-DOBOL_BUILD_DUAL_GL)
  message(STATUS "Dual-GL build enabled: system OpenGL + OSMesa in one library")
endif()

# Set library output name based on backend selection
if(OBOL_USE_OSMESA)
  set(OBOL_OUTPUT_NAME "Obol-osmesa")
  add_definitions(-DOBOL_OSMESA_BUILD)
  # USE_MGL_NAMESPACE is set only on the Obol library target (see src/CMakeLists.txt)
  # to avoid propagating it to test executables that include <GL/glx.h>, which
  # conditionally includes "glx_mangle.h" (not present on modern Ubuntu 24.04).
  message(STATUS "Library will be named: lib${OBOL_OUTPUT_NAME}")
else()
  set(OBOL_OUTPUT_NAME "Obol")
  message(STATUS "Library will be named: lib${OBOL_OUTPUT_NAME}")
endif()

# Threading support
find_package(Threads REQUIRED)
set(HAVE_THREADS 1)
if(CMAKE_USE_WIN32_THREADS_INIT)
  set(USE_W32THREAD 1)
  set(OBOL_THREADID_TYPE DWORD)
elseif(CMAKE_USE_PTHREADS_INIT)
  set(USE_PTHREAD 1)
  set(OBOL_THREADID_TYPE pthread_t)
endif()
list(APPEND OBOL_TARGET_LINK_LIBRARIES Threads::Threads)

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
  option(OBOL_BUILD_MSVC_STATIC_RUNTIME "Build against the static Microsoft Visual C runtime library" OFF)

  if(MSVC_VERSION GREATER_EQUAL 1500)
    option(OBOL_BUILD_MSVC_MP "Enable build with multiple processes in Visual Studio" ON)
  else()
    set(OBOL_BUILD_MSVC_MP OFF CACHE BOOL "Compiler option /MP requires at least Visual Studio 2008 (VS9) or newer" FORCE)
  endif()

  if(OBOL_BUILD_MSVC_MP)
    add_compile_options(/MP)
  endif()

  # Enable C++ exception handling
  add_compile_options(/EHsc)

  # Static runtime linking
  if(OBOL_BUILD_MSVC_STATIC_RUNTIME)
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
endif()

set(HAVE_GZDOPEN 1)
if(WIN32)
  set(HAVE_WIN32_API 1)
  #set(HAVE_WGL 1)
  if(MINGW)
    set(OBOL_DEFAULT_SHARED_POSTFIX "")
    set(OBOL_DEFAULT_STATIC_POSTFIX "")
  else()
    # On Windows the major version number is part of the library name
    set(OBOL_DEFAULT_SHARED_POSTFIX ${PROJECT_VERSION_MAJOR})
    set(OBOL_DEFAULT_STATIC_POSTFIX ${PROJECT_VERSION_MAJOR}s)
  endif()
  if(OBOL_BUILD_SHARED_LIBS)
    set(OBOL_DEFAULT_POSTFIX ${OBOL_DEFAULT_SHARED_POSTFIX})
    set(OBOL_LIBRARY_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    add_definitions(-DOBOL_MAKE_DLL)
  else()
    set(OBOL_DEFAULT_POSTFIX ${OBOL_DEFAULT_STATIC_POSTFIX})
    set(OBOL_LIBRARY_SUFFIX ${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()
  set(CMAKE_RELEASE_POSTFIX "${OBOL_DEFAULT_POSTFIX}"
    CACHE STRING "Default filename postfix for libraries under configuration Release")
  set(CMAKE_MINSIZEREL_POSTFIX "${OBOL_DEFAULT_POSTFIX}"
    CACHE STRING "Default filename postfix for libraries under configuration MinSizeRel")
  set(CMAKE_RELWITHDEBINFO_POSTFIX "${OBOL_DEFAULT_POSTFIX}"
    CACHE STRING "Default filename postfix for libraries under configuration RelWithDebInfo")
  set(CMAKE_DEBUG_POSTFIX "${OBOL_DEFAULT_POSTFIX}d"
    CACHE STRING "Default filename postfix for libraries under configuration Debug")

  set(OBOL_RELEASE_SYSTEM_LIBRARY_NAME Obol${CMAKE_RELEASE_POSTFIX}${OBOL_LIBRARY_SUFFIX})
  set(OBOL_DEBUG_SYSTEM_LIBRARY_NAME Obol${CMAKE_DEBUG_POSTFIX}${OBOL_LIBRARY_SUFFIX})
elseif(APPLE)
  # macOS-specific settings (minimal)
  if(POLICY CMP0042)
    cmake_policy(SET CMP0042 NEW)
  endif()

  if(POLICY CMP0068)
    cmake_policy(SET CMP0068 NEW)
  endif()

  # Basic macOS linking
  list(APPEND OBOL_TARGET_LINK_LIBRARIES "-framework CoreFoundation" "-framework CoreGraphics")
  set(OBOL_EXTRA_LDFLAGS " -lObol -Wl,-framework,OpenGL")

  set(OBOL_RELEASE_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:${PROJECT_NAME}>")
  set(OBOL_DEBUG_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:${PROJECT_NAME}>")
else()
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)

  set(OBOL_RELEASE_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:${PROJECT_NAME}>")
  set(OBOL_DEBUG_SYSTEM_LIBRARY_NAME "$<TARGET_FILE_NAME:${PROJECT_NAME}>")
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
    list(APPEND OBOL_TARGET_LINK_LIBRARIES ${CMAKE_DL_LIBS})
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

if(OBOL_USE_OSMESA)
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
if(OBOL_THREADSAFE)
  set(FEAT_HAVE_SAFETHREAD 1)
else()
  set(FEAT_HAVE_SAFETHREAD 0)
endif()

# ============================================================================
# Code coverage instrumentation flags
#
# When OBOL_COVERAGE=ON (and the compiler is GCC or Clang), add --coverage
# to the compile and link flags for the entire project.  This must be done
# BEFORE the source subdirectories are added so that the Obol library and all
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

if(OBOL_BUILD_TESTS)
  # Build lodepng from the bundled source so that rgb_to_png and
  # image_comparator can write / compare PNG control images without
  # requiring any system-installed PNG library.
  add_library(lodepng STATIC "${PROJECT_SOURCE_DIR}/external/lodepng.cpp")
  target_include_directories(lodepng PUBLIC "${PROJECT_SOURCE_DIR}/external")
  message(STATUS "Using bundled lodepng from external/lodepng.cpp")

  enable_testing()
  add_subdirectory(tests)
  add_subdirectory(examples)
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

add_feature_info(ThreadSafe OBOL_THREADSAFE "Thread safe render traversals")
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
