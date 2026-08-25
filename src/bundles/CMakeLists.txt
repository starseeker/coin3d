# source files
set(OBOL_BUNDLES_FILES
	SoBundle.cpp
	SoMaterialBundle.cpp
	SoNormalBundle.cpp
	SoVertexAttributeBundle.cpp
	SoTextureCoordinateBundle.cpp
)

# build library
foreach(SRCFILE ${OBOL_BUNDLES_FILES})
	set_property(GLOBAL APPEND PROPERTY OBOL_SOURCE_FILES bundles/${SRCFILE})
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_BUNDLES_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/bundles/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
