# =============================================================================
# Obol Test Suite
#
# Directory structure:
#   tests/
#   ├── CMakeLists.txt           this file
#   ├── test_utils.h/.cpp        SimpleTest framework (lightweight, no deps)
#   ├── run_image_test.cmake     CTest driver for visual regression tests
#   ├── generate_control_images.sh
#   ├── control_images/          reference PNG images for visual tests
#   ├── utils/
#   │   ├── image_comparator.cpp standalone image comparison tool
#   │   ├── rgb_to_png.c         SGI RGB -> PNG converter
#   │   ├── headless_utils.h     headless rendering helpers
#   │   ├── scene_graph_utils.h/.cpp  scene-graph test helpers
#   │   └── stb_image.h          public-domain PNG loader (fallback)
#   ├── testlib/                 unified test registry (obol_testlib)
#   ├── rendering/               visual regression tests (OSMesa / GLX)
#   └── obol_test.cpp            unified CLI test runner
#
# Adding new tests
# ----------------
# Non-visual unit tests:
#   1. Add a REGISTER_TEST entry in tests/testlib/unit_test_wrappers.cpp
#      (see test_registry.h for the API).
#
# Visual regression tests:
#   1. Write a test executable in tests/rendering/ that renders a scene and
#      writes an SGI RGB file to argv[1]+".rgb".
#   2. Build it and run generate_control_images.sh to create the control PNG.
#   3. Call add_image_test() in this file to register it with CTest.
#
# =============================================================================

# -----------------------------------------------------------------------
# Common test utilities shared by all test executables
# -----------------------------------------------------------------------
add_library(simple_test_utils STATIC
    test_utils.cpp
    utils/scene_graph_utils.cpp
)
target_link_libraries(simple_test_utils ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
target_include_directories(simple_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
# headless_utils.h needs the OSMesa submodule headers for pure osmesa builds
if(OBOL_USE_OSMESA)
    target_include_directories(simple_test_utils PUBLIC
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
endif()

# -----------------------------------------------------------------------
# Image comparison thresholds (defined before subdirectories so that
# tests/rendering/ can reference them during the first configure pass)
# -----------------------------------------------------------------------
set(IMAGE_COMPARISON_HASH_THRESHOLD "5"   CACHE STRING "Perceptual hash threshold (0-64)")
set(IMAGE_COMPARISON_RMSE_THRESHOLD "5.0" CACHE STRING "RMSE threshold (0-255)")
set(IMAGE_COMPARISON_TEST_TIMEOUT   "30"  CACHE STRING "Per-test timeout in seconds")

# -----------------------------------------------------------------------
# Test library (shared registry, scene factories, unit-test wrappers)
# Must come before any target that links obol_testlib, but after
# simple_test_utils is defined above.
# -----------------------------------------------------------------------
add_subdirectory(testlib)

# -----------------------------------------------------------------------
# FLTK scene viewer  (obol_viewer)
# Placed here so that obol_testlib (scene catalogue) is already defined.
# Enabled by -DOBOL_BUILD_VIEWER=ON at CMake configure time (auto-detected
# when FLTK is found; requires OBOL_BUILD_TESTS=ON).
# -----------------------------------------------------------------------
if(OBOL_BUILD_VIEWER)
    # FLTK was detected (system or bundled external/fltk) in the top-level
    # CMakeLists.txt.  FLTK_INCLUDE_DIRS and FLTK_LIBRARIES are already set.
    # OBOL_VIEWER_USE_FLTK_GL is defined in the top-level CMakeLists.txt
    # (before the FLTK detection block) so it can control FLTK_BUILD_GL.

    add_executable(obol_viewer obol_viewer.cpp)

    target_include_directories(obol_viewer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}           # testlib/test_registry.h
        ${CMAKE_CURRENT_SOURCE_DIR}/utils     # headless_utils.h / fltk_context_manager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/testlib   # test_registry.h (direct)
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
        ${FLTK_INCLUDE_DIRS}
    )

    # Compile definitions matching the Obol library build
    if(OBOL_BUILD_DUAL_GL)
        target_compile_definitions(obol_viewer PRIVATE OBOL_BUILD_DUAL_GL)
        # Dual-GL builds enable the OSMesa panel (OBOL_VIEWER_OSMESA_PANEL defined
        # in obol_viewer.cpp when OBOL_BUILD_DUAL_GL is set).  The panel uses
        # SoDB::createOSMesaContextManager() – no direct OSMesa dependency needed.
    elseif(OBOL_USE_OSMESA)
        # Pure-OSMesa build: headless_utils.h uses OSMesa context manager.
        # FLTK GL option does not apply here.
        target_compile_definitions(obol_viewer PRIVATE OBOL_OSMESA_BUILD)
        target_include_directories(obol_viewer PRIVATE
            "${PROJECT_SOURCE_DIR}/external/osmesa/include")
    endif()

    # Select the system-GL context manager for the viewer.
    # OBOL_VIEWER_USE_FLTK_GL (default ON) selects fltk_context_manager.h which
    # uses Fl_Gl_Window for cross-platform GL context creation.
    # Set OFF to fall back to headless_utils.h (GLX on Linux) for CI environments
    # where FLTK's OpenGL cannot obtain a context (headless / no display manager).
    # This option does not apply to pure-OSMesa builds.
    if(OBOL_VIEWER_USE_FLTK_GL AND NOT OBOL_USE_OSMESA)
        target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_FLTK_GL)
        message(STATUS "obol_viewer: using FLTK GL context manager (portable)")
    else()
        if(NOT OBOL_USE_OSMESA)
            message(STATUS "obol_viewer: using GLX context manager (headless_utils.h)")
        endif()
    endif()

    # Optional CPU raytracing panel: Embree (preferred) or NanoRT fallback.
    #
    # OBOL_VIEWER_USE_EMBREE (default ON when embree 4 is found on the system)
    # selects the Embree 4 CPU raytracing panel as an alternative to NanoRT.
    # When Embree is enabled, the NanoRT panel is skipped.  If neither backend
    # is available the viewer builds without a CPU raytracing panel.
    find_package(embree 4 QUIET
        HINTS /usr/lib/x86_64-linux-gnu/cmake/embree-4.3.0
              /usr/lib/cmake/embree4
              /usr/local/lib/cmake/embree4)
    if(embree_FOUND OR TARGET embree4)
        if(TARGET embree4)
            set(_obol_embree_link embree4)
        else()
            set(_obol_embree_link embree)
        endif()
        option(OBOL_VIEWER_USE_EMBREE
            "Use Embree (rather than NanoRT) as the CPU raytracing panel in obol_viewer"
            ON)
    else()
        option(OBOL_VIEWER_USE_EMBREE
            "Use Embree (rather than NanoRT) as the CPU raytracing panel in obol_viewer"
            OFF)
    endif()

    if(OBOL_VIEWER_USE_EMBREE AND (embree_FOUND OR TARGET embree4))
        target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_EMBREE)
        target_include_directories(obol_viewer PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/utils"   # embree_context_manager.h
        )
        target_link_libraries(obol_viewer PRIVATE ${_obol_embree_link})
        message(STATUS "obol_viewer: Embree panel enabled (replaces NanoRT)")
    elseif(EXISTS "${PROJECT_SOURCE_DIR}/external/nanort/nanort.h")
        # Embree not available or disabled – fall back to NanoRT if present.
        target_compile_definitions(obol_viewer PRIVATE OBOL_VIEWER_NANORT)
        target_include_directories(obol_viewer PRIVATE
            "${PROJECT_SOURCE_DIR}/external/nanort"
            "${CMAKE_CURRENT_SOURCE_DIR}/utils"   # nanort_context_manager.h
        )
        message(STATUS "obol_viewer: NanoRT panel enabled (external/nanort/nanort.h found)")
    else()
        message(STATUS "obol_viewer: No CPU raytracing panel (neither Embree nor NanoRT found)")
    endif()

    # Link against obol_testlib (with --whole-archive so REGISTER_TEST static
    # initialisers are not discarded) as well as the main Obol library.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_libraries(obol_viewer
            PRIVATE
                -Wl,--whole-archive obol_testlib -Wl,--no-whole-archive
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                ${FLTK_LIBRARIES}
        )
    else()
        target_link_libraries(obol_viewer
            PRIVATE
                obol_testlib
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                ${FLTK_LIBRARIES}
        )
    endif()
    # On Linux, both the FLTK GL path (fltk_context_manager.h uses
    # glXGetProcAddress) and the GLX fallback path (headless_utils.h) require
    # the X11 and OpenGL libraries.  Link them explicitly when available.
    if(UNIX AND NOT APPLE)
        if(X11_FOUND)
            target_link_libraries(obol_viewer PRIVATE ${X11_X11_LIB})
        endif()
        if(TARGET OpenGL::GL)
            target_link_libraries(obol_viewer PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(obol_viewer PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()

    # C++17 std::thread is used for parallel CPU raytracing; ensure the platform
    # threading library is linked.  Threads::Threads is already in
    # OBOL_TARGET_LINK_LIBRARIES (via the top-level find_package(Threads)) so
    # it is transitively available; this explicit link guards against future
    # refactors that might drop it from the umbrella variable.
    find_package(Threads REQUIRED)
    target_link_libraries(obol_viewer PRIVATE Threads::Threads)
    message(STATUS "obol_viewer: C++17 std::thread parallel CPU raytracing enabled (${CMAKE_THREAD_LIBS_INIT})")

    install(TARGETS obol_viewer RUNTIME DESTINATION bin)
endif() # OBOL_BUILD_VIEWER

# -----------------------------------------------------------------------
# Qt scene viewer  (obol_qt_viewer)
# Placed here so obol_testlib (scene catalogue) is already defined.
# Enabled by -DOBOL_BUILD_QT_VIEWER=ON at CMake configure time (auto-
# detected when Qt6 is found; requires OBOL_BUILD_TESTS=ON).
# -----------------------------------------------------------------------
if(OBOL_BUILD_QT_VIEWER)
    add_executable(obol_qt_viewer obol_qt_viewer.cpp)

    # Enable Qt's automatic MOC/UIC/RCC processing for this target.
    set_target_properties(obol_qt_viewer PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
    )

    target_include_directories(obol_qt_viewer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}           # testlib/test_registry.h
        ${CMAKE_CURRENT_SOURCE_DIR}/utils     # qt_context_manager.h
        ${CMAKE_CURRENT_SOURCE_DIR}/testlib   # test_registry.h (direct)
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    )

    # Link against obol_testlib (with --whole-archive to preserve static
    # initialisers), the Obol library, and Qt6 Widgets + Gui.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_libraries(obol_qt_viewer
            PRIVATE
                -Wl,--whole-archive obol_testlib -Wl,--no-whole-archive
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                Qt6::Widgets
                Qt6::Gui
        )
    else()
        target_link_libraries(obol_qt_viewer
            PRIVATE
                obol_testlib
                ${PROJECT_NAME}
                ${OBOL_TARGET_LINK_LIBRARIES}
                Qt6::Widgets
                Qt6::Gui
        )
    endif()

    # On Linux, link OpenGL explicitly (for glXGetProcAddress used by
    # qt_context_manager.h on X11/Linux).
    if(UNIX AND NOT APPLE)
        if(X11_FOUND)
            target_link_libraries(obol_qt_viewer PRIVATE ${X11_X11_LIB})
        endif()
        if(TARGET OpenGL::GL)
            target_link_libraries(obol_qt_viewer PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(obol_qt_viewer PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()

    install(TARGETS obol_qt_viewer RUNTIME DESTINATION bin)
    message(STATUS "obol_qt_viewer: enabled (Qt6 ${Qt6_VERSION})")
endif() # OBOL_BUILD_QT_VIEWER

# -----------------------------------------------------------------------
# Unified CLI runner (legacy – kept for backward compatibility)
# -----------------------------------------------------------------------
add_executable(obol_test obol_test.cpp)
# --whole-archive ensures the static-initialiser REGISTER_TEST blocks in
# unit_test_wrappers.cpp (and test_scenes.cpp) are not discarded by the
# linker even though obol_test.cpp has no direct symbol references to them.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(obol_test
        -Wl,--whole-archive obol_testlib -Wl,--no-whole-archive
        ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES}
    )
else()
    target_link_libraries(obol_test obol_testlib ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
endif()
target_include_directories(obol_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(obol_test pthread)
endif()
add_test(NAME obol_test_all COMMAND obol_test)

# -----------------------------------------------------------------------
# Non-graphical unit test runner  (obol_unittest)
# Runs only tests with a run_unit function; no GL context required.
# -----------------------------------------------------------------------
add_executable(obol_unittest obol_unittest.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(obol_unittest PRIVATE
        -Wl,--whole-archive obol_testlib -Wl,--no-whole-archive
        ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES}
    )
else()
    target_link_libraries(obol_unittest PRIVATE obol_testlib ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
endif()
target_include_directories(obol_unittest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(obol_unittest PRIVATE pthread)
endif()
add_test(NAME obol_unittest_all COMMAND obol_unittest)

# -----------------------------------------------------------------------
# Visual regression tests (rendering/ subdirectory)
# These require standalone executables as each renders a scene to an image
# file that is then compared against a control PNG.
# -----------------------------------------------------------------------
add_subdirectory(rendering)

# ---- Generate render test dispatch table shared by obol_test and obol_render --
# This file is written at configure time; it declares extern references to all
# renamed main() functions and provides dispatchRenderTest().
set(_dispatch "${CMAKE_CURRENT_BINARY_DIR}/render_test_dispatch.cpp")
set(_dc "// Auto-generated render test dispatch - DO NOT EDIT.\n")
string(APPEND _dc "#include <string>\n#include <cstring>\n\n")
foreach(_t ${OBOL_GL_RENDER_TESTS})
    string(APPEND _dc "extern int run_render_test_${_t}(int, char**);\n")
endforeach()
string(APPEND _dc "\nint dispatchRenderTest(const std::string& name, const std::string& basepath) {\n")
string(APPEND _dc "    static const struct { const char* name; int(*fn)(int, char**); } k[] = {\n")
foreach(_t ${OBOL_GL_RENDER_TESTS})
    string(APPEND _dc "        {\"${_t}\", run_render_test_${_t}},\n")
endforeach()
string(APPEND _dc "        {nullptr, nullptr}\n    };\n")
string(APPEND _dc "    for (int i = 0; k[i].name; ++i) {\n")
string(APPEND _dc "        if (name == k[i].name) {\n")
string(APPEND _dc "            char prog[] = {\"obol_render\"};\n")
string(APPEND _dc "            // PATH_MAX on Linux is 4096; reject longer paths to keep the\n")
string(APPEND _dc "            // fixed-size buffer safe.  Return -2 to distinguish from 'not\n")
string(APPEND _dc "            // found' (-1) vs 'path too long' (-2).\n")
string(APPEND _dc "            if (basepath.size() >= 4096) { return -2; }\n")
string(APPEND _dc "            if (basepath.empty()) {\n")
string(APPEND _dc "                // No basepath: call with argc=1 so the test uses its default output path\n")
string(APPEND _dc "                char* argv1[2] = {prog, nullptr};\n")
string(APPEND _dc "                return k[i].fn(1, argv1);\n")
string(APPEND _dc "            }\n")
string(APPEND _dc "            char base[4096];\n")
string(APPEND _dc "            strncpy(base, basepath.c_str(), sizeof(base)-1);\n")
string(APPEND _dc "            base[sizeof(base)-1] = '\\0';\n")
string(APPEND _dc "            // argv[2] == nullptr satisfies the POSIX argv[argc] == nullptr rule\n")
string(APPEND _dc "            char* argv[3] = {prog, base, nullptr};\n")
string(APPEND _dc "            return k[i].fn(2, argv);\n")
string(APPEND _dc "        }\n    }\n")
string(APPEND _dc "    return -1;\n}\n")
file(WRITE "${_dispatch}" "${_dc}")

# Link the render tests library + dispatch table into obol_test.
# The dispatch table's direct function references pull in all needed objects
# from obol_render_tests_gl without requiring --whole-archive.
target_sources(obol_test PRIVATE "${_dispatch}")
if(TARGET obol_render_tests_gl)
    target_link_libraries(obol_test obol_render_tests_gl)
endif()

# -----------------------------------------------------------------------
# Rendering test runner  (obol_render)
# Runs visual/rendering tests; produces control images or image sequences.
# Shares the same render test dispatch as obol_test.
# -----------------------------------------------------------------------
add_executable(obol_render obol_render.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(obol_render PRIVATE
        -Wl,--whole-archive obol_testlib -Wl,--no-whole-archive
        ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES}
    )
else()
    target_link_libraries(obol_render PRIVATE obol_testlib ${PROJECT_NAME} ${OBOL_TARGET_LINK_LIBRARIES})
endif()
target_include_directories(obol_render PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/testlib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(USE_PTHREAD)
    target_link_libraries(obol_render PRIVATE pthread)
endif()
# Share the dispatch table and render test library with obol_render
target_sources(obol_render PRIVATE "${_dispatch}")
if(TARGET obol_render_tests_gl)
    target_link_libraries(obol_render PRIVATE obol_render_tests_gl)
endif()
# On Linux the renderer includes headless_utils.h which uses GLX or OSMesa
if(UNIX AND NOT APPLE)
    if(X11_FOUND)
        target_link_libraries(obol_render PRIVATE ${X11_X11_LIB})
    endif()
    if(TARGET OpenGL::GL)
        target_link_libraries(obol_render PRIVATE OpenGL::GL)
    elseif(OPENGL_gl_LIBRARY)
        target_link_libraries(obol_render PRIVATE ${OPENGL_gl_LIBRARY})
    endif()
endif()
if(OBOL_USE_OSMESA)
    target_compile_definitions(obol_render PRIVATE OBOL_OSMESA_BUILD)
    target_include_directories(obol_render PRIVATE
        "${PROJECT_SOURCE_DIR}/external/osmesa/include")
elseif(OBOL_BUILD_DUAL_GL)
    target_compile_definitions(obol_render PRIVATE OBOL_BUILD_DUAL_GL)
endif()
install(TARGETS obol_render RUNTIME DESTINATION bin)
install(TARGETS obol_unittest RUNTIME DESTINATION bin)

# -----------------------------------------------------------------------
# Image comparison utilities
# -----------------------------------------------------------------------

set(CONTROL_IMAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/control_images")

# image_comparator: standalone tool for comparing SGI RGB / PNG images
# Prefer the bundled external/libpng (built as png_static in the parent
# project) over any system libpng so that builds without libpng-dev still
# produce a fully-featured comparator.
if(TARGET png_static)
    set(PNG_FOUND TRUE)
    set(PNG_INCLUDE_DIRS
          "${CMAKE_SOURCE_DIR}/external/libpng"
          "${CMAKE_BINARY_DIR}/libpng_build")
    set(PNG_LIBRARIES png_static)
    message(STATUS "Tests: using bundled png_static for image tools")
else()
    find_package(PNG QUIET)
endif()
add_executable(image_comparator utils/image_comparator.cpp)
if(PNG_FOUND)
    target_compile_definitions(image_comparator PRIVATE HAVE_LIBPNG)
    target_include_directories(image_comparator PRIVATE ${PNG_INCLUDE_DIRS})
    target_link_libraries(image_comparator ${PNG_LIBRARIES})
else()
    target_include_directories(image_comparator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils)
endif()
set_target_properties(image_comparator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# rgb_to_png: convert SGI RGB files to PNG (requires libpng)
if(PNG_FOUND)
    add_executable(rgb_to_png utils/rgb_to_png.c)
    target_include_directories(rgb_to_png PRIVATE ${PNG_INCLUDE_DIRS})
    target_link_libraries(rgb_to_png ${PNG_LIBRARIES})
    set_target_properties(rgb_to_png PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
    )
endif()

# Output directory for test-generated images
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test_output")

# -----------------------------------------------------------------------
# Macro: add_image_test
#
# Register a visual regression test with CTest.  The test executable must
# write an SGI RGB image to argv[1]+".rgb" (or argv[1] for direct-write).
# A matching control PNG must exist in control_images/.
#
# Usage:
#   add_image_test(test_name)
#   add_image_test(test_name RMSE_THRESHOLD 25.0)
#   add_image_test(test_name HASH_THRESHOLD 10 RMSE_THRESHOLD 25.0)
# -----------------------------------------------------------------------
macro(add_image_test test_name)
    set(CONTROL_IMAGE "${CONTROL_IMAGES_DIR}/${test_name}_control.png")
    set(TEST_BASE "${CMAKE_CURRENT_BINARY_DIR}/test_output/${test_name}_test")

    set(_hash_thresh ${IMAGE_COMPARISON_HASH_THRESHOLD})
    set(_rmse_thresh ${IMAGE_COMPARISON_RMSE_THRESHOLD})
    set(_next_is_hash FALSE)
    set(_next_is_rmse FALSE)
    foreach(_arg ${ARGN})
        if(_arg STREQUAL "HASH_THRESHOLD")
            set(_next_is_hash TRUE)
            set(_next_is_rmse FALSE)
        elseif(_arg STREQUAL "RMSE_THRESHOLD")
            set(_next_is_rmse TRUE)
            set(_next_is_hash FALSE)
        elseif(_next_is_hash)
            set(_hash_thresh ${_arg})
            set(_next_is_hash FALSE)
        elseif(_next_is_rmse)
            set(_rmse_thresh ${_arg})
            set(_next_is_rmse FALSE)
        endif()
    endforeach()

    if(EXISTS "${CONTROL_IMAGE}")
        add_test(
            NAME ${test_name}_image_test
            COMMAND ${CMAKE_COMMAND}
                -DEXECUTABLE=$<TARGET_FILE:${test_name}>
                -DTEST_BASE=${TEST_BASE}
                -DCONTROL_IMAGE=${CONTROL_IMAGE}
                -DCOMPARATOR=$<TARGET_FILE:image_comparator>
                -DHASH_THRESHOLD=${_hash_thresh}
                -DRMSE_THRESHOLD=${_rmse_thresh}
                -DTEST_TIMEOUT=${IMAGE_COMPARISON_TEST_TIMEOUT}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/run_image_test.cmake
        )
        set_tests_properties(${test_name}_image_test PROPERTIES
            TIMEOUT ${IMAGE_COMPARISON_TEST_TIMEOUT}
        )
    endif()
endmacro()

# Register visual regression tests here as they are created:
#   add_image_test(test_basic_sphere)
#   add_image_test(test_material_colors)

# -----------------------------------------------------------------------
# Custom target: generate_controls
# Runs all rendering test executables and converts their output to control PNGs.
# Requires libpng (for rgb_to_png) and a display (or OSMesa).
# -----------------------------------------------------------------------
if(PNG_FOUND)
    add_custom_target(generate_controls
        COMMAND ${CMAKE_COMMAND} -E env
            BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
            CONTROL_DIR=${CONTROL_IMAGES_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_control_images.sh
        DEPENDS image_comparator rgb_to_png
        COMMENT "Generating PNG control images for visual regression tests"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
