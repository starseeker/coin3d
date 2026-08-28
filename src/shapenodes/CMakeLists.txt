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


# build module object library
obol_add_source_module(shapenodes ${OBOL_SHAPENODES_FILES})
