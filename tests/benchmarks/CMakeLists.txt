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
