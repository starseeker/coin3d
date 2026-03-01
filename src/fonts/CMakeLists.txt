# source files
set(OBOL_FONTS_FILES
	profont_data.cpp
	fontspec.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_FONTS_INTERNAL_FILES
	profont_data.h
	profont_data.cpp
	fontspec.h
	fontspec.cpp
)

# build library
foreach(SRCFILE ${OBOL_FONTS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} fonts/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_FONTS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/fonts/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_FONTS_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/fonts/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
