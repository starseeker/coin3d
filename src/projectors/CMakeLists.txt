# source files
set(OBOL_PROJECTORS_FILES
	SbCylinderPlaneProjector.cpp
	SbCylinderProjector.cpp
	SbCylinderSectionProjector.cpp
	SbCylinderSheetProjector.cpp
	SbLineProjector.cpp
	SbPlaneProjector.cpp
	SbProjector.cpp
	SbSpherePlaneProjector.cpp
	SbSphereProjector.cpp
	SbSphereSectionProjector.cpp
	SbSphereSheetProjector.cpp
)

# build module object library
obol_add_source_module(projectors ${OBOL_PROJECTORS_FILES})
