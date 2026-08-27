# Standalone performance characterization targets.  These are intentionally
# not registered with CTest: timing output is useful data, not a pass/fail
# contract.

add_executable(obol_scalability_benchmark
    scalability_main.cpp
    scalability_benchmark.cpp
)
target_link_libraries(obol_scalability_benchmark
    PRIVATE
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES}
)
target_include_directories(obol_scalability_benchmark PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src
)
set_target_properties(obol_scalability_benchmark PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Keep the historical CAD renderer comparison available as characterization
# tooling. It writes images and timings selected by command-line arguments,
# so it is intentionally a benchmark executable rather than a CTest case.
if(OBOL_USE_SWRAST OR
   (OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE))
    add_executable(obol_cad_render_benchmark
        cad_render_benchmark.cpp)
    set_target_properties(obol_cad_render_benchmark PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    target_include_directories(obol_cad_render_benchmark PRIVATE
        ${PROJECT_SOURCE_DIR}/examples/demo_support
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_BINARY_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_BINARY_DIR}/src
        ${OBOL_TARGET_INCLUDE_DIRECTORIES})
    target_link_libraries(obol_cad_render_benchmark PRIVATE
        ${PROJECT_NAME}
        ${OBOL_TARGET_LINK_LIBRARIES})

    if(OBOL_USE_SWRAST)
        target_compile_definitions(obol_cad_render_benchmark PRIVATE
            OBOL_SWRAST_BUILD)
        target_include_directories(obol_cad_render_benchmark PRIVATE
            ${PROJECT_SOURCE_DIR}/external/osmesa/include)
        if(OBOL_USE_SYSTEM_GL)
            target_compile_definitions(obol_cad_render_benchmark PRIVATE
                OBOL_DUAL_GL_BUILD)
        endif()
        if(OBOL_OSMESA_LIBRARIES)
            target_link_libraries(obol_cad_render_benchmark PRIVATE
                ${OBOL_OSMESA_LIBRARIES})
        endif()
    endif()

    if(OBOL_USE_SYSTEM_GL AND HAVE_GLX AND UNIX AND NOT APPLE)
        if(TARGET OpenGL::GLX)
            target_link_libraries(obol_cad_render_benchmark PRIVATE
                OpenGL::GLX)
        elseif(OPENGL_glx_LIBRARY)
            target_link_libraries(obol_cad_render_benchmark PRIVATE
                ${OPENGL_glx_LIBRARY})
        endif()
    endif()
endif()
