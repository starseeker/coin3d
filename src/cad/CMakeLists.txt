# =============================================================================
# src/cad/CMakeLists.txt
#
# CAD assembly rendering, picking, and LoD subsystem.
#
# Sources are added to the global OBOL_SOURCE list so they are compiled into
# the main Obol library (same pattern as all other src/ subdirectories).
# =============================================================================

set(CAD_SOURCES
    # Core CAD node
    ${CMAKE_CURRENT_SOURCE_DIR}/CadIds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadProjectedProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SoCADDetail.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SoCADViewState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SoCADAssembly.cpp

    # GL renderer (VBO + shader, GL 2.0 baseline with optional GL 3.1+ instancing)
    ${CMAKE_CURRENT_SOURCE_DIR}/CadGLCaps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadGpuResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadRendererGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadRendererGLExecutors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadShaderPrograms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadShaderSources.cpp

    # Picking
    ${CMAKE_CURRENT_SOURCE_DIR}/picking/CadPicking.cpp
)

set(OBOL_SOURCE ${OBOL_SOURCE} ${CAD_SOURCES} CACHE INTERNAL "OBOL_SOURCE")
