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
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_GLUE_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} glue/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
	if(OBOL_BUILD_DUAL_GL)
		set(OBOL_SOURCE ${OBOL_SOURCE} glue/gl_osmesa.cpp CACHE INTERNAL "OBOL_SOURCE")
	endif()
else()
	add_library(glue OBJECT ${OBOL_GLUE_FILES})
	if(HAVE_LIBSIMAGE)
		target_compile_definitions(glue PRIVATE $<TARGET_PROPERTY:simage::simage,INTERFACE_COMPILE_DEFINITIONS>) 
		target_include_directories(glue PRIVATE $<TARGET_PROPERTY:simage::simage,INTERFACE_INCLUDE_DIRECTORIES>)
	endif()
	if(OBOL_BUILD_DUAL_GL)
		# gl_osmesa.cpp is a separate object library so we can set its
		# compile definitions and include paths independently from the main
		# glue objects (it needs OBOL_GLHEADERS_OSMESA_OVERRIDE, USE_MGL_NAMESPACE,
		# and the OSMesa include directories without affecting the rest of the build).
		add_library(glue_osmesa OBJECT gl_osmesa.cpp)
		target_compile_definitions(glue_osmesa PRIVATE
			OBOL_INTERNAL
			OBOL_BUILD_DUAL_GL
		)
		target_include_directories(glue_osmesa PRIVATE
			${CMAKE_CURRENT_SOURCE_DIR}   # for glp.h, dl.h, etc.
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/include/Inventor/annex
			${PROJECT_BINARY_DIR}/include
			${OBOL_TARGET_INCLUDE_DIRECTORIES}
		)
		if(OBOL_OSMESA_INCLUDE_DIRS)
			target_include_directories(glue_osmesa PRIVATE ${OBOL_OSMESA_INCLUDE_DIRS})
		endif()
	endif()
endif()

# define documentation
foreach(SRCFILE ${OBOL_GLUE_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_GLUE_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
