# source files
set(OBOL_RENDERING_FILES
	SoGL.cpp
	SoGLBigImage.cpp
	SoGLDriverDatabase.cpp
	SoGLImage.cpp
	SoGLCubeMapImage.cpp
	SoRenderManager.cpp
	SoRenderManagerP.cpp
	SoOffscreenRenderer.cpp
	SoVBO.cpp
	SoVertexArrayIndexer.cpp
	CoinOffscreenGLCanvas.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_RENDERING_INTERNAL_FILES
	SoGL.h
	SoGL.cpp
	SoRenderManagerP.h
	SoRenderManagerP.cpp
	SoVBO.h
	SoVBO.cpp
	SoVertexArrayIndexer.h
	SoVertexArrayIndexer.cpp
	CoinOffscreenGLCanvas.h
	CoinOffscreenGLCanvas.cpp
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_RENDERING_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} rendering/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(rendering OBJECT ${OBOL_RENDERING_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_RENDERING_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/rendering/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_RENDERING_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/rendering/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
