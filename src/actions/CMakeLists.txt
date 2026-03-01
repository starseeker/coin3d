# source files
set(OBOL_ACTIONS_FILES
	SoAction.cpp
	SoActionP.cpp
	SoBoxHighlightRenderAction.cpp
	SoCallbackAction.cpp
	SoGLRenderAction.cpp
	SoGetBoundingBoxAction.cpp
	SoGetMatrixAction.cpp
	SoGetPrimitiveCountAction.cpp
	SoHandleEventAction.cpp
	SoLineHighlightRenderAction.cpp
	SoPickAction.cpp
	SoRayPickAction.cpp
	SoRaytraceRenderAction.cpp
	SoReorganizeAction.cpp
	SoSearchAction.cpp
	SoSimplifyAction.cpp
	SoWriteAction.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_ACTIONS_INTERNAL_FILES
	SoActionP.h
	SoActionP.cpp
	SoSubActionP.h
)

# build library
foreach(SRCFILE ${OBOL_ACTIONS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} actions/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_ACTIONS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/actions/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_ACTIONS_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/actions/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
