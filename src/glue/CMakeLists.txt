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


set(_obol_glue_sources ${OBOL_GLUE_FILES})
if(OBOL_USE_SWRAST AND OBOL_USE_SYSTEM_GL)
	list(APPEND _obol_glue_sources gl_osmesa.cpp)
	# This wrapper deliberately instantiates gl_impl.inc with a different
	# symbol prefix and GL header set.  It must remain its own translation unit:
	# placing it in the same unity batch as gl.cpp produces duplicate definitions
	# and leaks the OSMesa preprocessor configuration into neighbouring sources.
	set_source_files_properties(gl_osmesa.cpp PROPERTIES
		SKIP_UNITY_BUILD_INCLUSION ON
		INCLUDE_DIRECTORIES "${OBOL_OSMESA_INCLUDE_DIRS}")
endif()
obol_add_source_module(glue ${_obol_glue_sources})
