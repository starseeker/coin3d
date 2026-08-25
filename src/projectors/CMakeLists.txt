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

# define documentation
foreach(SRCFILE ${OBOL_PROJECTORS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/projectors/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
