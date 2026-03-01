# source files
set(OBOL_NODEKITS_FILES
	SoNodeKit.cpp
	SoNodeKitPath.cpp
	SoNodeKitListPart.cpp
	SoNodekitCatalog.cpp
	SoBaseKit.cpp
	SoAppearanceKit.cpp
	SoCameraKit.cpp
	SoInteractionKit.cpp
	SoLightKit.cpp
	SoSceneKit.cpp
	SoSeparatorKit.cpp
	SoShapeKit.cpp
	SoWrapperKit.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_NODEKITS_INTERNAL_FILES
	SoSubKitP.h
)

# build library
foreach(SRCFILE ${OBOL_NODEKITS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} nodekits/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_NODEKITS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/nodekits/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_NODEKITS_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/nodekits/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
