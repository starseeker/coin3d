# source files
set(OBOL_BUNDLES_FILES
	SoBundle.cpp
	SoMaterialBundle.cpp
	SoNormalBundle.cpp
	SoVertexAttributeBundle.cpp
	SoTextureCoordinateBundle.cpp
)

# build module object library
obol_add_source_module(bundles ${OBOL_BUNDLES_FILES})
