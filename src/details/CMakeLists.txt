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

# build module object library
obol_add_source_module(details ${OBOL_DETAILS_FILES})
