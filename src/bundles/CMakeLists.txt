# source files
set(OBOL_BUNDLES_FILES
	SoBundle.cpp
	SoMaterialBundle.cpp
	SoNormalBundle.cpp
	SoVertexAttributeBundle.cpp
	SoTextureCoordinateBundle.cpp
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_BUNDLES_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} bundles/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(bundles OBJECT ${OBOL_BUNDLES_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_BUNDLES_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/bundles/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
