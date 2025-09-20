# Full profiler implementation - expanded to include all components
set(COIN_PROFILER_FILES
	SbProfilingData.cpp
	SoProfiler.cpp
	SoProfilerElement.cpp
	# Additional profiler components as requested
	SoScrollingGraphKit.cpp
	SoProfilingReportGenerator.cpp
	SoProfilerTopKit.cpp
	SoProfilerStats.cpp
	SoNodeVisualize.cpp
	SoProfilerVisualizeKit.cpp
	SoProfilerTopEngine.cpp
)

# build library
if(COIN_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${COIN_PROFILER_FILES})
		set(COIN_SOURCE ${COIN_SOURCE} profiler/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(profiler OBJECT ${COIN_PROFILER_FILES})
endif()

# define documentation
foreach(SRCFILE ${COIN_PROFILER_FILES})
	set(COIN_DOCUMENTATION_FILES ${COIN_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/profiler/${SRCFILE} CACHE INTERNAL "COIN_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${COIN_PROFILER_INTERNAL_FILES})
	set(COIN_INTERNAL_DOCUMENTATION_FILES ${COIN_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/profiler/${SRCFILE} CACHE INTERNAL "COIN_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)

# SoProfilerP is not quite private, it is included from VRML nodes. The symbol is excluded from public API in doc/CMakeList.txt.
set(COIN_DOCUMENTATION_FILES
	${COIN_DOCUMENTATION_FILES}
	${PROJECT_SOURCE_DIR}/src/profiler/SoProfilerP.h
	CACHE INTERNAL "COIN_DOCUMENTATION_FILES"
)
