# source files
set(OBOL_FONTS_FILES
	profont_data.cpp
	fontspec.cpp
)


# build module object library
obol_add_source_module(fonts ${OBOL_FONTS_FILES})
