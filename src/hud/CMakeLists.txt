# source files
set(OBOL_HUD_FILES
    SoHUD.cpp
    SoHUDKit.cpp
    SoHUDLabel.cpp
    SoHUDButton.cpp
)

# build module object library
obol_add_source_module(hud ${OBOL_HUD_FILES})

# define documentation
foreach(SRCFILE ${OBOL_HUD_FILES})
    set(OBOL_DOCUMENTATION_FILES
        ${OBOL_DOCUMENTATION_FILES}
        ${PROJECT_SOURCE_DIR}/src/hud/${SRCFILE}
        CACHE INTERNAL "OBOL_DOCUMENTATION_FILES"
    )
endforeach(SRCFILE)
