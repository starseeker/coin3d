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

set(_obol_glue_sources ${OBOL_GLUE_FILES})
if(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
	list(APPEND _obol_glue_sources gl_osmesa.cpp)
endif()
obol_add_source_module(glue ${_obol_glue_sources})

# define documentation
foreach(SRCFILE ${OBOL_GLUE_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_GLUE_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/glue/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
