# =============================================================================
# Modern Obol test framework
#
# This is deliberately independent of the removed registry / ObolEx layer.
# GTest supplies normal assertions, fixtures, filtering and CTest discovery;
# obol_test_support supplies only Obol-specific lifecycle and rendering helpers.
# =============================================================================

find_package(GTest 1.10 QUIET)
if(NOT GTest_FOUND)
    obol_init_submodule("external/googletest")
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/googletest/CMakeLists.txt")
        message(FATAL_ERROR
            "Obol tests require GoogleTest 1.10 or newer. Install a system "
            "GTest package or initialize the bundled fallback with: "
            "git submodule update --init external/googletest")
    endif()

    message(STATUS "GTest: using bundled external/googletest")
    set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Install bundled GoogleTest" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL
        "Use the shared MSVC runtime for bundled GoogleTest" FORCE)
    # Tests do not need a second runtime DLL merely because Obol itself is a
    # shared library. Keep this override scoped to the dependency directory.
    set(_obol_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(
        "${PROJECT_SOURCE_DIR}/external/googletest"
        "${PROJECT_BINARY_DIR}/external/googletest"
        EXCLUDE_FROM_ALL)
    set(BUILD_SHARED_LIBS "${_obol_saved_build_shared_libs}")
    unset(_obol_saved_build_shared_libs)
else()
    message(STATUS "GTest: using system package")
endif()
add_test(NAME package.installed_consumer
    COMMAND ${CMAKE_COMMAND}
        -DOBOL_BINARY_DIR=${PROJECT_BINARY_DIR}
        -DOBOL_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DOBOL_CONFIG=$<CONFIG>
        "-DOBOL_GENERATOR=${CMAKE_GENERATOR}"
        "-DOBOL_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
        "-DOBOL_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET}"
        "-DOBOL_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"
        "-DOBOL_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        -P ${PROJECT_SOURCE_DIR}/tests/package/run_consumer.cmake)
set_tests_properties(package.installed_consumer PROPERTIES
    LABELS "package"
    TIMEOUT 180
    RUN_SERIAL TRUE)

add_library(obol_test_support STATIC
    image_assertions.cpp
    stb_image_write_impl.cpp
    ${PROJECT_SOURCE_DIR}/tests/test_utils.cpp
    test_context.cpp
    render_fixture.cpp
)
# Test sources deliberately use translation-unit-local helpers with concise
# names. Parent projects may enable CMAKE_UNITY_BUILD globally, but combining
# independent GTest sources changes that isolation and causes ODR collisions.
set_target_properties(obol_test_support PROPERTIES UNITY_BUILD OFF)
if(OBOL_USE_SWRAST OR OBOL_USE_SYSTEM_GL)
    # The scene catalogue is shared by fixture-backed render contracts.  Keep
    # it in the modern support target so converted cases do not need a second
    # renderer-specific library.
    target_sources(obol_test_support PRIVATE
        ${PROJECT_SOURCE_DIR}/tests/testlib/test_scenes.cpp
        ${PROJECT_SOURCE_DIR}/tests/testlib/test_scenes_extended.cpp
        ${PROJECT_SOURCE_DIR}/tests/testlib/test_scenes_scene_texture.cpp
    )
endif()
if(OBOL_USE_SWRAST)
    target_compile_definitions(obol_test_support PRIVATE OBOL_TEST_HAVE_SWRAST)
endif()
if(OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE)
    target_compile_definitions(obol_test_support PRIVATE OBOL_TEST_HAVE_SYSTEM_GL)
elseif(OBOL_USE_SYSTEM_GL AND WIN32 AND FLTK_FOUND)
    target_compile_definitions(obol_test_support PUBLIC
        OBOL_TEST_HAVE_SYSTEM_GL
        OBOL_TEST_WGL)
    target_link_libraries(obol_test_support PUBLIC ${FLTK_LIBRARIES})
endif()

if(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
    target_compile_definitions(obol_test_support PRIVATE OBOL_DUAL_GL_BUILD)
endif()
if(OBOL_PROFILING)
    # A small number of API-contract tests need to know whether the library
    # contains the optional implementation.  Keep the macro scoped to test
    # code rather than exposing this private build detail to Obol consumers.
    target_compile_definitions(obol_test_support PUBLIC OBOL_PROFILING)
endif()
target_link_libraries(obol_test_support
    PUBLIC
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        GTest::gtest
)
if(OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE)
    # render_fixture.cpp owns the GLX context implementation, so the support
    # library must carry its direct link dependency to every consumer.  This
    # also keeps dual-backend executables linkable even when they select
    # OSMesa at runtime.
    if(TARGET OpenGL::GLX)
        target_link_libraries(obol_test_support PUBLIC OpenGL::GLX)
    elseif(OPENGL_glx_LIBRARY)
        target_link_libraries(obol_test_support PUBLIC ${OPENGL_glx_LIBRARY})
    else()
        message(FATAL_ERROR
            "System-GL tests require a GLX library on Unix")
    endif()
endif()
target_include_directories(obol_test_support
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/tests
        ${PROJECT_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/tests/testlib
        ${PROJECT_SOURCE_DIR}/examples/demo_support
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Inventor/annex
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
        ${PROJECT_SOURCE_DIR}/external
        ${FLTK_INCLUDE_DIRS}
        ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
if(EXISTS "${PROJECT_SOURCE_DIR}/fonts/Iosevka/IosevkaAile-Regular.ttc")
    target_compile_definitions(obol_test_support PRIVATE
        OBOL_FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")
endif()

if(USE_PTHREAD)
    target_link_libraries(obol_test_support PUBLIC pthread)
endif()

function(obol_add_gtest target label)
    # PARSE_ARGV preserves semicolons inside quoted arguments.  Expanding
    # ARGN here flattened Windows PATH values, causing every PATH component
    # after the first to be misinterpreted as an executable source file.
    cmake_parse_arguments(PARSE_ARGV 2 OBOL_GTEST ""
        "TIMEOUT;ENVIRONMENT;SKIP_REGULAR_EXPRESSION" "")
    if(OBOL_GTEST_TIMEOUT)
        set(_obol_gtest_timeout ${OBOL_GTEST_TIMEOUT})
    else()
        set(_obol_gtest_timeout 30)
    endif()
    add_executable(${target} test_main.cpp ${OBOL_GTEST_UNPARSED_ARGUMENTS})
    set_target_properties(${target} PROPERTIES UNITY_BUILD OFF)
    target_link_libraries(${target}
        PRIVATE
            obol_test_support
            GTest::gtest
    )
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/tests
    )
    # Register each ordinary executable once. Running every case in a fresh
    # process hid order dependencies and made CTest startup dominate the suite.
    # A stable shuffled order gives local failures a reproducible seed; CI also
    # runs repeated time-seeded shuffles to explore other orders.
    add_test(NAME ${target}
        COMMAND ${target} --gtest_shuffle --gtest_random_seed=73421)

    # Normalize compound categories to one searchable CTest label.
    string(REPLACE ";" "-" _obol_gtest_labels "${label}")
    set(_obol_gtest_properties
        LABELS "${_obol_gtest_labels}"
        TIMEOUT ${_obol_gtest_timeout})
    if(OBOL_GTEST_ENVIRONMENT)
        # CTest stores ENVIRONMENT as a CMake list. Preserve separators inside
        # values such as Windows PATH instead of turning PATH components into
        # additional property arguments.
        string(REPLACE ";" "\\;" _obol_gtest_environment
            "${OBOL_GTEST_ENVIRONMENT}")
        list(APPEND _obol_gtest_properties
            ENVIRONMENT "${_obol_gtest_environment}")
    endif()
    if(OBOL_GTEST_SKIP_REGULAR_EXPRESSION)
        list(APPEND _obol_gtest_properties
            SKIP_REGULAR_EXPRESSION
            "${OBOL_GTEST_SKIP_REGULAR_EXPRESSION}")
    endif()
    set_tests_properties(${target} PROPERTIES ${_obol_gtest_properties})
endfunction()

# Lifecycle tests must begin before SoDB has been initialized and must be able
# to call SoDB::finish().  Keep them in a dedicated GTest process instead of
# sharing the normal test_main.cpp, which intentionally initializes Obol once.
add_executable(obol_lifecycle_tests
    ${PROJECT_SOURCE_DIR}/tests/unit/lifecycle_test.cpp
)
set_target_properties(obol_lifecycle_tests PROPERTIES UNITY_BUILD OFF)
target_link_libraries(obol_lifecycle_tests
    PRIVATE
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        GTest::gtest_main
)
target_include_directories(obol_lifecycle_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
)
add_test(NAME obol_lifecycle_tests COMMAND obol_lifecycle_tests)
set_tests_properties(obol_lifecycle_tests PROPERTIES
    LABELS "unit-lifecycle"
    TIMEOUT 30)

# A loadable extension must bind to the same process-global type registry as
# its host.  That is the supported shared-library plugin model; linking a
# module to a static Obol archive would give the module a second registry and
# cannot exercise dynamic loading correctly.
if(BUILD_SHARED_LIBS)
    # CMake's WINDOWS_EXPORT_ALL_SYMBOLS only applies to SHARED libraries, not
    # MODULE libraries.  Windows plugins are DLLs in either case, so model the
    # probes as SHARED there to export their C++ initClass symbols.  Other
    # platforms retain the semantically appropriate MODULE target type.
    if(WIN32)
        set(_obol_dynload_library_type SHARED)
    else()
        set(_obol_dynload_library_type MODULE)
    endif()

    # Exercise the extension-node loader with a real shared object.  The
    # deliberately short class name guards against heuristic filtering of
    # valid third-party types.
    add_library(obol_dynload_probe ${_obol_dynload_library_type}
        ${PROJECT_SOURCE_DIR}/tests/unit/dynload_probe.cpp)
    set_target_properties(obol_dynload_probe PROPERTIES
        OUTPUT_NAME Probe
        WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    target_link_libraries(obol_dynload_probe PRIVATE ${PROJECT_NAME})
    target_include_directories(obol_dynload_probe PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/include)

    function(obol_add_dynload_probe probe source)
        string(TOLOWER "${probe}" _probe_lower)
        add_library(obol_dynload_${_probe_lower} ${_obol_dynload_library_type}
            ${source})
        set_target_properties(obol_dynload_${_probe_lower} PROPERTIES
            OUTPUT_NAME ${probe}
            WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
        target_link_libraries(obol_dynload_${_probe_lower} PRIVATE ${PROJECT_NAME})
        target_include_directories(obol_dynload_${_probe_lower} PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_BINARY_DIR}/include)
    endfunction()
    obol_add_dynload_probe(ThrowProbe
        ${PROJECT_SOURCE_DIR}/tests/unit/dynload_throw_probe.cpp)
    obol_add_dynload_probe(WrongProbe
        ${PROJECT_SOURCE_DIR}/tests/unit/dynload_wrong_probe.cpp)

    if(WIN32)
        set(_obol_dynload_environment
            "PATH=$<TARGET_FILE_DIR:obol_dynload_probe>;$ENV{PATH}")
    elseif(APPLE)
        set(_obol_dynload_environment
            "DYLD_LIBRARY_PATH=$<TARGET_FILE_DIR:obol_dynload_probe>")
    else()
        set(_obol_dynload_environment
            "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:obol_dynload_probe>")
    endif()
    obol_add_gtest(obol_dynload_tests "unit;dynload"
        ENVIRONMENT "${_obol_dynload_environment}"
        ${PROJECT_SOURCE_DIR}/tests/unit/type_dynload_test.cpp)
    add_dependencies(obol_dynload_tests
        obol_dynload_probe
        obol_dynload_throwprobe
        obol_dynload_wrongprobe)
    unset(_obol_dynload_library_type)
endif()

# Exhausting SoType mutates process-global state irreversibly.  Keep this one
# boundary test isolated from all ordinary unit cases.
add_executable(obol_type_capacity_tests
    ${PROJECT_SOURCE_DIR}/tests/unit/type_capacity_test.cpp)
set_target_properties(obol_type_capacity_tests PROPERTIES UNITY_BUILD OFF)
target_link_libraries(obol_type_capacity_tests PRIVATE
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    GTest::gtest_main)
target_include_directories(obol_type_capacity_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include)
add_test(NAME obol_type_capacity_tests COMMAND obol_type_capacity_tests)
set_tests_properties(obol_type_capacity_tests PROPERTIES
    LABELS "unit-capacity"
    TIMEOUT 60)

# Source manifests keep suite membership reviewable without letting this file
# regress into another monolithic list of legacy translation units.
include(cmake/ObolUnitSources.cmake)
include(cmake/ObolIntegrationSources.cmake)

obol_add_gtest(obol_unit_tests "unit" ${OBOL_UNIT_SOURCES})
obol_add_gtest(obol_integration_tests "integration" ${OBOL_INTEGRATION_SOURCES})
target_include_directories(obol_integration_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/src/cad
    ${PROJECT_SOURCE_DIR}/src/cad/picking
)

# Every backend-independent rendering contract belongs in one manifest.  Each
# enabled backend compiles and discovers exactly this corpus, which prevents a
# new software-lane regression test from silently missing native OpenGL.
include(cmake/ObolPortableRenderSources.cmake)

function(obol_configure_render_target target)
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/examples/demo_scenes)
    target_compile_definitions(${target} PRIVATE
        OBOL_CONTROL_IMAGES_DIR="${PROJECT_SOURCE_DIR}/tests/control_images"
        OBOL_TEST_OUTPUT_DIR="${PROJECT_BINARY_DIR}/test-output/${target}"
        OBOL_RENDER_TEST_MAIN)
endfunction()

# OSMesa is the deterministic software lane.  In a dual build its selection
# remains runtime-controlled through OBOL_TEST_RENDER_BACKEND.
if(OBOL_USE_SWRAST)
    obol_add_gtest(obol_render_tests "render;osmesa"
        ENVIRONMENT "OBOL_TEST_RENDER_BACKEND=swrast"
        ${OBOL_PORTABLE_RENDER_SOURCES})
    obol_configure_render_target(obol_render_tests)
else()
    message(STATUS "Software-rasterizer render lane: skipped (OBOL_USE_SWRAST=OFF)")
endif()

# Exercise native dispatch and GLX with the same portable contracts.
if(OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE)
    obol_add_gtest(obol_system_gl_render_tests "render;system-gl"
        ENVIRONMENT "OBOL_TEST_RENDER_BACKEND=system"
        SKIP_REGULAR_EXPRESSION
            "system OpenGL (backend is unavailable|has no framebuffer-object support)"
        ${OBOL_PORTABLE_RENDER_SOURCES})
    obol_configure_render_target(obol_system_gl_render_tests)
elseif(OBOL_USE_SYSTEM_GL AND WIN32 AND FLTK_FOUND)
    obol_add_gtest(obol_system_gl_render_tests "render;system-gl"
        ENVIRONMENT "OBOL_TEST_RENDER_BACKEND=system"
        SKIP_REGULAR_EXPRESSION
            "system OpenGL (backend is unavailable|has no framebuffer-object support)"
        ${OBOL_PORTABLE_RENDER_SOURCES})
    target_compile_definitions(obol_system_gl_render_tests PRIVATE OBOL_TEST_WGL)
    obol_configure_render_target(obol_system_gl_render_tests)
elseif(OBOL_USE_SYSTEM_GL)
    message(STATUS
        "System-GL render lane: skipped (requires GLX or Windows FLTK/WGL)")
endif()

if(NOT OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
    obol_add_gtest(obol_nogl_tests "nogl"
        ${PROJECT_SOURCE_DIR}/tests/rendering/nogl_scenegraph_test.cpp)
endif()

option(OBOL_BUILD_STRESS_TESTS
    "Build opt-in stress and sanitizer-oriented tests"
    OFF)
if(OBOL_BUILD_STRESS_TESTS)
    obol_add_gtest(obol_stress_tests "stress;threads" TIMEOUT 120
        ${PROJECT_SOURCE_DIR}/tests/threads/test_thread_safety.cpp
    )
    # The sanitizer suite exercises private cache-ID infrastructure directly.
    target_compile_definitions(obol_stress_tests PRIVATE OBOL_INTERNAL)
endif()

# A compact in-process contract for the documented concurrent-render model.
# Keep it separate from the large visual suite so TSan can exercise it without
# compiling or running hundreds of unrelated image comparisons.
if(OBOL_USE_SWRAST)
    obol_add_gtest(obol_concurrent_render_tests "render;threads" TIMEOUT 120
        ENVIRONMENT "OBOL_TEST_RENDER_BACKEND=swrast"
        ${PROJECT_SOURCE_DIR}/tests/rendering/concurrent_render_test.cpp)
    obol_configure_render_target(obol_concurrent_render_tests)
endif()

target_include_directories(obol_unit_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/tests/utils
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)


# -----------------------------------------------------------------------------
# Rendering cases remain in a reviewable manifest.  A new test is an
# intentional CMake change, every source registers an ordinary GTest case, and
# every enabled GL backend receives the same source list.  The shared headless
# helper selects system GL or OSMesa from the executable's
# OBOL_TEST_RENDER_BACKEND environment.
include(cmake/ObolRenderScenarioSources.cmake)

function(obol_add_render_scenario_sources target)
    target_sources(${target} PRIVATE
        ${OBOL_RENDER_SCENARIO_SOURCES})
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/examples/demo_support
        ${PROJECT_SOURCE_DIR}/tests/utils
        ${PROJECT_SOURCE_DIR}/tests/testlib
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
    )
    if(EXISTS "${PROJECT_SOURCE_DIR}/fonts/Iosevka/IosevkaAile-Regular.ttc")
        target_compile_definitions(${target} PRIVATE
            OBOL_FONTS_DIR="${PROJECT_SOURCE_DIR}/fonts")
    endif()
endfunction()

if(TARGET obol_render_tests)
    obol_add_render_scenario_sources(obol_render_tests)
    target_include_directories(obol_render_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/external/osmesa/include)
    target_compile_definitions(obol_render_tests PRIVATE OBOL_SWRAST_BUILD)
    if(OBOL_USE_SYSTEM_GL)
        target_compile_definitions(obol_render_tests PRIVATE OBOL_DUAL_GL_BUILD)
    endif()
endif()

if(TARGET obol_system_gl_render_tests)
    obol_add_render_scenario_sources(obol_system_gl_render_tests)
endif()
