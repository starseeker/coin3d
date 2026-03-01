# source files
set(OBOL_IO_FILES
	SoInput.cpp
	SoInputP.cpp
	SoInput_FileInfo.cpp
	SoInput_Reader.cpp
	SoOutput.cpp
	SoOutput_Writer.cpp
	SoByteStream.cpp
	SoTranSender.cpp
	SoTranReceiver.cpp
	SoWriterefCounter.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_IO_INTERNAL_FILES
	SoInputP.h
	SoInputP.cpp
	SoInput_FileInfo.h
	SoInput_FileInfo.cpp
	SoInput_Reader.h
	SoInput_Reader.cpp
	SoOutput_Writer.h
	SoOutput_Writer.cpp
	SoWriterefCounter.h
	SoWriterefCounter.cpp
)

# build library
foreach(SRCFILE ${OBOL_IO_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} io/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_IO_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/io/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_IO_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/io/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
