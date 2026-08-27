# source files
set(OBOL_HARDCOPY_FILES
	HardCopy.cpp
	PSVectorOutput.cpp
	VectorOutput.cpp
	VectorizeAction.cpp
	VectorizeActionP.cpp
	VectorizePSAction.cpp
)


# build module object library
obol_add_source_module(hardcopy ${OBOL_HARDCOPY_FILES})
