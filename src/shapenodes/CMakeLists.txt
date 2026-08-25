# source files
set(OBOL_SHAPENODES_FILES
	SoAsciiText.cpp
	SoCone.cpp
	SoCube.cpp
	SoCylinder.cpp
	SoFaceSet.cpp
	SoImage.cpp
	SoIndexedFaceSet.cpp
	SoIndexedLineSet.cpp
	SoIndexedMarkerSet.cpp
	SoIndexedPointSet.cpp
	SoIndexedShape.cpp
	SoIndexedTriangleStripSet.cpp
	SoLineSet.cpp
	SoMarkerSet.cpp
	SoNonIndexedShape.cpp
	SoPointSet.cpp
	SoProceduralShape.cpp
	SoQuadMesh.cpp
	SoShape.cpp
	SoSphere.cpp
	SoText2.cpp
	SoText3.cpp
	SoTriangleStripSet.cpp
	SoVertexShape.cpp
	soshape_bigtexture.cpp
	soshape_bumprender.cpp
	soshape_primdata.cpp
	soshape_trianglesort.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_SHAPENODES_INTERNAL_FILES
	soshape_bigtexture.h
	soshape_bigtexture.cpp
	soshape_bumprender.h
	soshape_bumprender.cpp
	soshape_primdata.h
	soshape_primdata.cpp
	soshape_trianglesort.h
	soshape_trianglesort.cpp
)

# build module object library
obol_add_source_module(shapenodes ${OBOL_SHAPENODES_FILES})

# define documentation
foreach(SRCFILE ${OBOL_SHAPENODES_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/shapenodes/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_SHAPENODES_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/shapenodes/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
