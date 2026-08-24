# =============================================================================
# tests/cad/CMakeLists.txt
#
# Remaining legacy visual/benchmark checks for the CAD assembly subsystem.
# Deterministic IDs, instance records, resolved draw planning, and CPU picking
# are modern GTest coverage in tests/framework/CMakeLists.txt.  This file is
# retained only for the subpixel visual program and the rendering benchmark.
# =============================================================================

# Common include paths for CAD tests
set(CAD_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/tests                       # test_utils.h
    ${PROJECT_SOURCE_DIR}/include                   # Inventor headers
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    # Internal CAD headers needed directly by tests
    ${PROJECT_SOURCE_DIR}/src/cad/picking
    ${PROJECT_SOURCE_DIR}/src/cad
)

# test_cad_ids.cpp and test_cad_instance_records.cpp have been migrated to
# tests/framework/CMakeLists.txt.  Keep the remaining domain-specific legacy
# executables here until each has an explicit modern backend contract.

# test_cad_resolved_draw.cpp has been migrated to obol_integration_tests.

# The CPU picking suite is now tests/integration/cad_picking_test.cpp.

# ---------------------------------------------------------------------------
# test_cad_benchmark  (rendering comparison; requires a GL context)
# ---------------------------------------------------------------------------
# This test is only built when a rendering backend is available:
#   - swrast-only: OSMesa (OBOL_USE_SWRAST=ON, OBOL_USE_SYSTEM_GL=OFF)
#   - system GL:  X11+GLX or dual build
# The test is skipped (not built) in pure no-GL configurations.

if(OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
    # Swrast-only: software rasteriser (OSMesa) backend
    set(_bench_use_swrast ON)
    set(_bench_osmesa_inc "${CMAKE_SOURCE_DIR}/external/osmesa/include")
    if(TARGET osmesa_interface)
        set(_bench_osmesa_link osmesa_interface)
    elseif(TARGET osmesa)
        set(_bench_osmesa_link osmesa)
    endif()
    set(_bench_has_gl ON)
elseif(OBOL_USE_SYSTEM_GL)
    # System GL (GLX + Xvfb in CI) or dual build
    set(_bench_use_swrast OFF)
    find_package(X11 QUIET)
    find_package(OpenGL QUIET)
    if(X11_FOUND AND (OpenGL_FOUND OR OPENGL_gl_LIBRARY))
        set(_bench_has_gl ON)
    else()
        set(_bench_has_gl OFF)
    endif()
else()
    set(_bench_has_gl OFF)
endif()

if(_bench_has_gl)
    # ---------------------------------------------------------------------------
    # test_cad_subpixel_proxy  (view-local AABB/OBB point replacement)
    # ---------------------------------------------------------------------------
    add_executable(test_cad_subpixel_proxy test_cad_subpixel_proxy.cpp)

    target_include_directories(test_cad_subpixel_proxy PRIVATE
        ${CAD_TEST_INCLUDES}
        ${CMAKE_SOURCE_DIR}/tests/utils
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_cad_subpixel_proxy PRIVATE
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        simple_test_utils
    )
    if(_bench_use_swrast)
        target_compile_definitions(test_cad_subpixel_proxy PRIVATE OBOL_SWRAST_BUILD)
        if(_bench_osmesa_inc)
            target_include_directories(test_cad_subpixel_proxy PRIVATE ${_bench_osmesa_inc})
        endif()
        if(_bench_osmesa_link)
            target_link_libraries(test_cad_subpixel_proxy PRIVATE ${_bench_osmesa_link})
        endif()
    else()
        if(X11_FOUND)
            target_link_libraries(test_cad_subpixel_proxy PRIVATE ${X11_X11_LIB})
            target_include_directories(test_cad_subpixel_proxy PRIVATE ${X11_INCLUDE_DIR})
        endif()
        if(OpenGL_FOUND AND TARGET OpenGL::GL)
            target_link_libraries(test_cad_subpixel_proxy PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(test_cad_subpixel_proxy PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()
    add_test(NAME test_cad_subpixel_proxy COMMAND test_cad_subpixel_proxy)
    set_tests_properties(test_cad_subpixel_proxy PROPERTIES LABELS "legacy;visual;cad")

    add_executable(test_cad_benchmark test_cad_benchmark.cpp)

    target_include_directories(test_cad_benchmark PRIVATE
        ${CAD_TEST_INCLUDES}
        ${CMAKE_SOURCE_DIR}/tests/utils    # headless_utils.h
        ${CMAKE_SOURCE_DIR}/tests          # test_utils.h
    )

    target_link_libraries(test_cad_benchmark PRIVATE
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        simple_test_utils
    )

    if(_bench_use_swrast)
        target_compile_definitions(test_cad_benchmark PRIVATE OBOL_SWRAST_BUILD)
        if(_bench_osmesa_inc)
            target_include_directories(test_cad_benchmark PRIVATE ${_bench_osmesa_inc})
        endif()
        if(_bench_osmesa_link)
            target_link_libraries(test_cad_benchmark PRIVATE ${_bench_osmesa_link})
        endif()
    else()
        if(X11_FOUND)
            target_link_libraries(test_cad_benchmark PRIVATE ${X11_X11_LIB})
            target_include_directories(test_cad_benchmark PRIVATE ${X11_INCLUDE_DIR})
        endif()
        if(OpenGL_FOUND AND TARGET OpenGL::GL)
            target_link_libraries(test_cad_benchmark PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(test_cad_benchmark PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()

    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(test_cad_benchmark PRIVATE Threads::Threads)
    endif()

    add_test(NAME test_cad_benchmark COMMAND test_cad_benchmark)
    set_tests_properties(test_cad_benchmark PROPERTIES LABELS "legacy;benchmark;cad")
    message(STATUS "CAD benchmark test: enabled (swrast=${_bench_use_swrast})")

else()
    message(STATUS "CAD benchmark test: skipped (no GL backend available)")
endif()
