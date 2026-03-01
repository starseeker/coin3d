# source files
set(OBOL_HUD_FILES
    SoHUD.cpp
    SoHUDKit.cpp
    SoHUDLabel.cpp
    SoHUDButton.cpp
)

# build library
foreach(SRCFILE ${OBOL_HUD_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} hud/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_HUD_FILES})
    set(OBOL_DOCUMENTATION_FILES
        ${OBOL_DOCUMENTATION_FILES}
        ${PROJECT_SOURCE_DIR}/src/hud/${SRCFILE}
        CACHE INTERNAL "OBOL_DOCUMENTATION_FILES"
    )
endforeach(SRCFILE)
