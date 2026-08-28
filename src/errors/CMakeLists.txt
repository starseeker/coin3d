# source files
set(OBOL_ERRORS_FILES
	error.cpp
	debugerror.cpp
	SoDebugError.cpp
	SoError.cpp
	SoMemoryError.cpp
	SoReadError.cpp
)


# build module object library
obol_add_source_module(errors ${OBOL_ERRORS_FILES})
