# =============================================================================
# tests/cad/CMakeLists.txt
#
# Unit tests for the CAD assembly subsystem:
#   - test_cad_ids        : ID generation and hashing
#   - test_segment_lod    : SegmentPopLod POP-quantisation
#   - test_triangle_lod   : TrianglePopLod POP-quantisation
#   - test_cad_picking    : CPU BVH ray-picking
#   - test_cad_benchmark  : Rendering comparison: SG vs. SoCADAssembly (needs GL)
#
# The first four tests are standalone (no GL context, no BRL-CAD dependency).
# test_cad_benchmark requires a rendering context and uses headless_utils.h.
# =============================================================================

# Common include paths for CAD tests
set(CAD_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/tests                       # test_utils.h
    ${PROJECT_SOURCE_DIR}/include                   # Inventor headers
    ${PROJECT_SOURCE_DIR}/include/Inventor/annex
    ${PROJECT_BINARY_DIR}/include
    ${OBOL_TARGET_INCLUDE_DIRECTORIES}
    # Internal CAD headers needed directly by tests
    ${PROJECT_SOURCE_DIR}/src/cad/lod
    ${PROJECT_SOURCE_DIR}/src/cad/picking
    ${PROJECT_SOURCE_DIR}/src/cad
)

# ---------------------------------------------------------------------------
# test_cad_ids
# ---------------------------------------------------------------------------
add_executable(test_cad_ids test_cad_ids.cpp)
target_link_libraries(test_cad_ids
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_cad_ids PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_cad_ids COMMAND test_cad_ids)

# ---------------------------------------------------------------------------
# test_segment_lod
# ---------------------------------------------------------------------------
add_executable(test_segment_lod test_segment_lod.cpp)
target_link_libraries(test_segment_lod
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_segment_lod PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_segment_lod COMMAND test_segment_lod)

# ---------------------------------------------------------------------------
# test_triangle_lod
# ---------------------------------------------------------------------------
add_executable(test_triangle_lod test_triangle_lod.cpp)
target_link_libraries(test_triangle_lod
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_triangle_lod PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_triangle_lod COMMAND test_triangle_lod)

# ---------------------------------------------------------------------------
# test_cad_picking
# ---------------------------------------------------------------------------
add_executable(test_cad_picking test_cad_picking.cpp)
target_link_libraries(test_cad_picking
    ${PROJECT_NAME}
    ${OBOL_TARGET_LINK_LIBRARIES}
    simple_test_utils
)
target_include_directories(test_cad_picking PRIVATE ${CAD_TEST_INCLUDES})
add_test(NAME test_cad_picking COMMAND test_cad_picking)

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
    message(STATUS "CAD benchmark test: enabled (swrast=${_bench_use_swrast})")

    # ---------------------------------------------------------------------------
    # test_cad_mesh_lod_benchmark  (triangle-mesh LoD stress test; requires GL)
    # ---------------------------------------------------------------------------
    add_executable(test_cad_mesh_lod_benchmark test_cad_mesh_lod_benchmark.cpp)

    target_include_directories(test_cad_mesh_lod_benchmark PRIVATE
        ${CAD_TEST_INCLUDES}
        ${CMAKE_SOURCE_DIR}/tests/utils    # headless_utils.h
        ${CMAKE_SOURCE_DIR}/tests          # test_utils.h
    )

    target_link_libraries(test_cad_mesh_lod_benchmark PRIVATE
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
        simple_test_utils
    )

    if(_bench_use_swrast)
        target_compile_definitions(test_cad_mesh_lod_benchmark PRIVATE OBOL_SWRAST_BUILD)
        if(_bench_osmesa_inc)
            target_include_directories(test_cad_mesh_lod_benchmark PRIVATE ${_bench_osmesa_inc})
        endif()
        if(_bench_osmesa_link)
            target_link_libraries(test_cad_mesh_lod_benchmark PRIVATE ${_bench_osmesa_link})
        endif()
    else()
        if(X11_FOUND)
            target_link_libraries(test_cad_mesh_lod_benchmark PRIVATE ${X11_X11_LIB})
            target_include_directories(test_cad_mesh_lod_benchmark PRIVATE ${X11_INCLUDE_DIR})
        endif()
        if(OpenGL_FOUND AND TARGET OpenGL::GL)
            target_link_libraries(test_cad_mesh_lod_benchmark PRIVATE OpenGL::GL)
        elseif(OPENGL_gl_LIBRARY)
            target_link_libraries(test_cad_mesh_lod_benchmark PRIVATE ${OPENGL_gl_LIBRARY})
        endif()
    endif()

    if(Threads_FOUND)
        target_link_libraries(test_cad_mesh_lod_benchmark PRIVATE Threads::Threads)
    endif()

    add_test(NAME test_cad_mesh_lod_benchmark COMMAND test_cad_mesh_lod_benchmark)
    message(STATUS "CAD mesh LoD benchmark: enabled (swrast=${_bench_use_swrast})")
else()
    message(STATUS "CAD benchmark test: skipped (no GL backend available)")
endif()
