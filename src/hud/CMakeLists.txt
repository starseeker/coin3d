# source files
set(COIN_HUD_FILES
    SoHUD.cpp
    SoHUDKit.cpp
    SoHUDLabel.cpp
    SoHUDButton.cpp
)

# build library
if(COIN_BUILD_SINGLE_LIB)
    foreach(SRCFILE ${COIN_HUD_FILES})
        set(COIN_SOURCE ${COIN_SOURCE} hud/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
    endforeach(SRCFILE)
else()
    add_library(hud OBJECT ${COIN_HUD_FILES})
endif()

# define documentation
foreach(SRCFILE ${COIN_HUD_FILES})
    set(COIN_DOCUMENTATION_FILES
        ${COIN_DOCUMENTATION_FILES}
        ${PROJECT_SOURCE_DIR}/src/hud/${SRCFILE}
        CACHE INTERNAL "COIN_DOCUMENTATION_FILES"
    )
endforeach(SRCFILE)
