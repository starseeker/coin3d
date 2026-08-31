# =============================================================================
# src/cad/CMakeLists.txt
#
# CAD assembly rendering, picking, and LoD subsystem.
#
# Sources are compiled into a dedicated object library and folded into the
# installed Obol library by src/CMakeLists.txt.
# =============================================================================

set(CAD_SOURCES
    # Core CAD node
    ${CMAKE_CURRENT_SOURCE_DIR}/CadGeometryValidation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadIds.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadPickTolerance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadSceneMutation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadSceneReplacement.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadSceneValidation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadProjectedProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SoCADDetail.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SoCADViewState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadAssemblyClassification.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadAssemblyPlan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SoCADAssembly.cpp

    # GL renderer (VBO + shader, GL 2.0 baseline with optional GL 3.1+ instancing)
    ${CMAKE_CURRENT_SOURCE_DIR}/CadGLCaps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadGpuResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadRendererGL.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadRendererGLExecutors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadRendererGLFlat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadRendererGLIndirect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadRendererGLInstanced.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadShaderPrograms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadShaderSources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CadSoftwareWire.cpp

    # Picking
    ${CMAKE_CURRENT_SOURCE_DIR}/picking/CadPicking.cpp
)

obol_add_source_module(cad ${CAD_SOURCES})
set_source_files_properties(
    CadAssemblyClassification.cpp
    CadAssemblyPlan.cpp
    SoCADAssembly.cpp
    CadRendererGLExecutors.cpp
    CadRendererGLFlat.cpp
    CadRendererGLIndirect.cpp
    CadRendererGLInstanced.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
target_include_directories(obol_cad_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lod
    ${CMAKE_CURRENT_SOURCE_DIR}/picking)
