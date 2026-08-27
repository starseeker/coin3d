# source files
set(OBOL_HUD_FILES
    SoHUD.cpp
    SoHUDKit.cpp
    SoHUDLabel.cpp
    SoHUDButton.cpp
)

# build module object library
obol_add_source_module(hud ${OBOL_HUD_FILES})
