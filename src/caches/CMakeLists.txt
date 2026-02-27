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

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_CACHES_INTERNAL_FILES
	SoGlyphCache.h
	SoGlyphCache.cpp
	SoShaderProgramCache.h
	SoShaderProgramCache.cpp
	SoVBOCache.h
	SoVBOCache.cpp
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_CACHES_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} caches/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(caches OBJECT ${OBOL_CACHES_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_CACHES_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/caches/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_CACHES_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/caches/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
