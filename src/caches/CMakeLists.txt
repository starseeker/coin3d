# source files
set(OBOL_CACHES_FILES
	SoBoundingBoxCache.cpp
	SoCache.cpp
	SoConvexDataCache.cpp
	SoGLCacheList.cpp
	SoGLRenderCache.cpp
	SoNormalCache.cpp
	SoTextureCoordinateCache.cpp
	SoPrimitiveVertexCache.cpp
	SoGlyphCache.cpp
	SoShaderProgramCache.cpp
	SoVBOCache.cpp
)


# build module object library
obol_add_source_module(caches ${OBOL_CACHES_FILES})
