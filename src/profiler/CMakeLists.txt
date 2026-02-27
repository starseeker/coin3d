# source files
set(OBOL_PROFILER_FILES
	SoProfiler.cpp
	SoProfilerElement.cpp
	SoProfilerOverlayKit.cpp
	SoProfilerStats.cpp
	SoProfilingReportGenerator.cpp
	SoProfilerTopEngine.cpp
	SoScrollingGraphKit.cpp
	SoNodeVisualize.cpp
	SoProfilerTopKit.cpp
	SoProfilerVisualizeKit.cpp
	SbProfilingData.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_PROFILER_INTERNAL_FILES
	SoNodeProfiling.h
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_PROFILER_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} profiler/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(profiler OBJECT ${OBOL_PROFILER_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_PROFILER_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/profiler/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_PROFILER_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/profiler/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)

# SoProfilerP is not quite private, it is included from VRML nodes. The symbol is excluded from public API in doc/CMakeList.txt.
set(OBOL_DOCUMENTATION_FILES
	${OBOL_DOCUMENTATION_FILES}
	${PROJECT_SOURCE_DIR}/src/profiler/SoProfilerP.h
	CACHE INTERNAL "OBOL_DOCUMENTATION_FILES"
)
