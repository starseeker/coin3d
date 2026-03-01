# source files
set(OBOL_DETAILS_FILES
	SoDetail.cpp
	SoConeDetail.cpp
	SoCubeDetail.cpp
	SoCylinderDetail.cpp
	SoFaceDetail.cpp
	SoLineDetail.cpp
	SoPointDetail.cpp
	SoTextDetail.cpp
)

# Add nodekit-specific details if nodekits are enabled
if(HAVE_NODEKITS)
	list(APPEND OBOL_DETAILS_FILES SoNodeKitDetail.cpp)
endif()

# build library
foreach(SRCFILE ${OBOL_DETAILS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} details/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_DETAILS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/details/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
