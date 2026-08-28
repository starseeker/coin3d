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


# build module object library
obol_add_source_module(profiler ${OBOL_PROFILER_FILES})
