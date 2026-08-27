# source files
set(OBOL_TOOLS_FILES
	SbModernUtils.cpp
)

# build module object library
obol_add_source_module(tools ${OBOL_TOOLS_FILES})

# define documentation
foreach(SRCFILE ${OBOL_TOOLS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/tools/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
