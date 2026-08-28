# source files
set(OBOL_IO_FILES
	SoInput.cpp
	SoInputP.cpp
	SoInput_FileInfo.cpp
	SoInput_Reader.cpp
	SoOutput.cpp
	SoOutput_Writer.cpp
	SoWriterefCounter.cpp
)


# build module object library
obol_add_source_module(io ${OBOL_IO_FILES})
