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

# build library
foreach(SRCFILE ${OBOL_PROJECTORS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} projectors/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_PROJECTORS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/projectors/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
