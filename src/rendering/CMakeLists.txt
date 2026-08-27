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
	SoNanoRTContextManager.cpp
	SoVBO.cpp
	SoVertexArrayIndexer.cpp
	CoinOffscreenGLCanvas.cpp
)


# build module object library
obol_add_source_module(rendering ${OBOL_RENDERING_FILES})
target_include_directories(obol_rendering_objects PRIVATE
	${PROJECT_SOURCE_DIR}/external/nanort)
