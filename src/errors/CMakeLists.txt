# source files
set(OBOL_ERRORS_FILES
	error.cpp
	debugerror.cpp
	SoDebugError.cpp
	SoError.cpp
	SoMemoryError.cpp
	SoReadError.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_ERRORS_INTERNAL_FILES
	error.cpp
	debugerror.cpp
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_ERRORS_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} errors/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(errors OBJECT ${OBOL_ERRORS_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_ERRORS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/errors/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_ERRORS_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/errors/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
