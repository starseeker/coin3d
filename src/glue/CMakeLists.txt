# source files
if(NOT OBOL_USE_SWRAST AND NOT OBOL_USE_SYSTEM_GL)
  # No-OpenGL build: use the no-op stub instead of the real GL dispatch layer.
  set(OBOL_GLUE_FILES
    dl.cpp
    gl_stub.cpp
    toojpeg.cpp
  )
else()
  set(OBOL_GLUE_FILES
    dl.cpp
    gl.cpp
    toojpeg.cpp
  )
endif()

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_GLUE_INTERNAL_FILES
	toojpeg.h
	toojpeg.cpp
)

# build library
foreach(SRCFILE ${OBOL_GLUE_FILES})
	set_property(GLOBAL APPEND PROPERTY OBOL_SOURCE_FILES glue/${SRCFILE})
endforeach(SRCFILE)
if(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
	set_property(GLOBAL APPEND PROPERTY OBOL_SOURCE_FILES glue/gl_osmesa.cpp)
endif()

# define documentation
foreach(SRCFILE ${OBOL_GLUE_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_GLUE_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
