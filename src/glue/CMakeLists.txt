# source files
set(COIN_GLUE_FILES
	dl.cpp
	gl.cpp
	simage_wrapper.cpp
	toojpeg.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(COIN_GLUE_INTERNAL_FILES
	simage_wrapper.h
	simage_wrapper.cpp
	toojpeg.h
	toojpeg.cpp
)

# build library
if(COIN_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${COIN_GLUE_FILES})
		set(COIN_SOURCE ${COIN_SOURCE} glue/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(glue OBJECT ${COIN_GLUE_FILES})
	if(HAVE_LIBSIMAGE)
		target_compile_definitions(glue PRIVATE $<TARGET_PROPERTY:simage::simage,INTERFACE_COMPILE_DEFINITIONS>) 
		target_include_directories(glue PRIVATE $<TARGET_PROPERTY:simage::simage,INTERFACE_INCLUDE_DIRECTORIES>)
	endif()
endif()

# define documentation
foreach(SRCFILE ${COIN_GLUE_FILES})
	set(COIN_DOCUMENTATION_FILES ${COIN_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "COIN_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${COIN_GLUE_INTERNAL_FILES})
	set(COIN_INTERNAL_DOCUMENTATION_FILES ${COIN_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "COIN_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
