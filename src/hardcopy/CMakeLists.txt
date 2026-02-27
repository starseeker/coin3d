# source files
set(OBOL_HARDCOPY_FILES
	HardCopy.cpp
	PSVectorOutput.cpp
	VectorOutput.cpp
	VectorizeAction.cpp
	VectorizeActionP.cpp
	VectorizePSAction.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_HARDCOPY_INTERNAL_FILES
	VectorizeActionP.h
	VectorizeActionP.cpp
	VectorizeItems.h
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_HARDCOPY_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} hardcopy/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(hardcopy OBJECT ${OBOL_HARDCOPY_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_HARDCOPY_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/hardcopy/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_HARDCOPY_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/hardcopy/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
