add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_definitions(-DUNICODE -D_UNICODE)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/utf8/include)

# source files
set(OBOL_BASE_FILES
	dict.cpp
	heap.cpp
	list.cpp
	memalloc.cpp
	namemap.cpp
	SbBSPTree.cpp
	SbByteBuffer.cpp
	SbBox2s.cpp
	SbBox2i32.cpp
	SbBox2f.cpp
	SbBox2d.cpp
	SbBox3s.cpp
	SbBox3i32.cpp
	SbBox3f.cpp
	SbBox3d.cpp
	SbClip.cpp
	SbColor.cpp
	SbColor4f.cpp
	SbCylinder.cpp
	SbDict.cpp
	SbDPLine.cpp
	SbDPMatrix.cpp
	SbDPPlane.cpp
	SbDPRotation.cpp
	SbHeap.cpp
	SbFont.cpp
	SbImage.cpp
	SbImageFormatHandler.cpp
	SbImageResize.cpp
	SbJpegImageHandler.cpp
	SbLine.cpp
	SbMatrix.cpp
	SbName.cpp
	SbOctTree.cpp
	SbPlane.cpp
	SbRotation.cpp
	SbSphere.cpp
	SbString.cpp
	SbTesselator.cpp
	SbTime.cpp
	SbUtf8.cpp
	SbVec2b.cpp
	SbVec2ub.cpp
	SbVec2s.cpp
	SbVec2us.cpp
	SbVec2i32.cpp
	SbVec2ui32.cpp
	SbVec2f.cpp
	SbVec2d.cpp
	SbVec3b.cpp
	SbVec3ub.cpp
	SbVec3s.cpp
	SbVec3us.cpp
	SbVec3i32.cpp
	SbVec3ui32.cpp
	SbVec3f.cpp
	SbVec3d.cpp
	SbVec4b.cpp
	SbVec4ub.cpp
	SbVec4s.cpp
	SbVec4us.cpp
	SbVec4i32.cpp
	SbVec4ui32.cpp
	SbVec4f.cpp
	SbVec4d.cpp
	SbViewVolume.cpp
	SbDPViewVolume.cpp
	SbViewportRegion.cpp
	SbXfBox3f.cpp
	SbXfBox3d.cpp
	utf8/casecvt.cpp
	utf8/utf8.cpp
)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	set(OBOL_BASE_FILES ${OBOL_BASE_FILES}
		buffer.cpp
		win.cpp
	)
endif()

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_BASE_INTERNAL_FILES
	coinString.h
	dict.h
	dict.cpp
	dictp.h
	heapp.h
	namemap.h
	namemap.cpp
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_BASE_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} base/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(base OBJECT ${OBOL_BASE_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_BASE_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/base/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_BASE_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/base/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
