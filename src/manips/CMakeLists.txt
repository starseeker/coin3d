# source files
set(OBOL_MANIPS_FILES
	SoClipPlaneManip.cpp
	SoDirectionalLightManip.cpp
	SoPointLightManip.cpp
	SoSpotLightManip.cpp
	SoTransformManip.cpp
	SoCenterballManip.cpp
	SoHandleBoxManip.cpp
	SoJackManip.cpp
	SoTabBoxManip.cpp
	SoTrackballManip.cpp
	SoTransformBoxManip.cpp
	SoTransformerManip.cpp
	commoncode.cpp
)

# build module object library
obol_add_source_module(manips ${OBOL_MANIPS_FILES})

# define documentation
foreach(SRCFILE ${OBOL_MANIPS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/manips/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
