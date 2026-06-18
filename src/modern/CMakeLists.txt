# source files
set(OBOL_MODERN_FILES
    Picking.cpp
    Scene.cpp
    SceneIO.cpp
    Renderer.cpp
)

# build library
foreach(SRCFILE ${OBOL_MODERN_FILES})
    set(OBOL_SOURCE ${OBOL_SOURCE} modern/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_MODERN_FILES})
    set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/modern/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
