# source files
set(OBOL_GLELEMENTS_FILES
	SoGLCacheContextElement.cpp
	SoGLClipPlaneElement.cpp
	SoGLColorIndexElement.cpp
	SoGLCoordinateElement.cpp
	SoGLDepthBufferElement.cpp
	SoGLDrawStyleElement.cpp
	SoGLEnvironmentElement.cpp
	SoGLLazyElement.cpp
	SoGLLightIdElement.cpp
	SoGLLinePatternElement.cpp
	SoGLLineWidthElement.cpp
	SoGLModelMatrixElement.cpp
	SoGLMultiTextureCoordinateElement.cpp
	SoGLMultiTextureEnabledElement.cpp
	SoGLMultiTextureMatrixElement.cpp
	SoGLMultiTextureImageElement.cpp
	SoGLNormalElement.cpp
	SoGLPointSizeElement.cpp
	SoGLPolygonOffsetElement.cpp
	SoGLProjectionMatrixElement.cpp
	SoGLRenderPassElement.cpp
	SoGLShapeHintsElement.cpp
	SoGLUpdateAreaElement.cpp
	SoGLVBOElement.cpp
	SoGLViewingMatrixElement.cpp
	SoGLViewportRegionElement.cpp
	SoGLDisplayList.cpp
	SoGLVertexAttributeElement.cpp
	SoResetMatrixElement.cpp
)

# build module object library
obol_add_source_module(elements_gl ${OBOL_GLELEMENTS_FILES})

# define documentation
foreach(SRCFILE ${OBOL_GLELEMENTS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/elements/GL/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
set(OBOL_DOCUMENTATION_FILES
	${OBOL_DOCUMENTATION_FILES}
	${PROJECT_SOURCE_DIR}/src/elements/GL/SoResetMatrixElement.h
	CACHE INTERNAL "OBOL_DOCUMENTATION_FILES"
)
