# source files
set(OBOL_GLUE_FILES
	dl.cpp
	gl.cpp
	toojpeg.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_GLUE_INTERNAL_FILES
	toojpeg.h
	toojpeg.cpp
)

# build library
foreach(SRCFILE ${OBOL_GLUE_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} glue/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)
if(OBOL_BUILD_DUAL_GL)
	set(OBOL_SOURCE ${OBOL_SOURCE} glue/gl_osmesa.cpp CACHE INTERNAL "OBOL_SOURCE")
endif()

# define documentation
foreach(SRCFILE ${OBOL_GLUE_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_GLUE_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
