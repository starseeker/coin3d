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
	SoSceneRenderAction.cpp
	SoReorganizeAction.cpp
	SoSearchAction.cpp
	SoSimplifyAction.cpp
	SoWriteAction.cpp
)


# build module object library
obol_add_source_module(actions ${OBOL_ACTIONS_FILES})
