# source files
set(OBOL_MODERN_FILES
    Camera.cpp
    Interaction.cpp
    Picking.cpp
    Scene.cpp
    ScenePacketCad.cpp
    ScenePacketExtraction.cpp
    ScenePacketGeometry.cpp
    ScenePacketLighting.cpp
    ScenePacketText.cpp
    SceneIO.cpp
    Renderer.cpp
    Time.cpp
)

# build library
foreach(SRCFILE ${OBOL_MODERN_FILES})
    set(OBOL_SOURCE ${OBOL_SOURCE} modern/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_MODERN_FILES})
    set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/modern/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
