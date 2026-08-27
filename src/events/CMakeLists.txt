# source files
set(OBOL_EVENTS_FILES
	SoButtonEvent.cpp
	SoEvent.cpp
	SoKeyboardEvent.cpp
	SoLocation2Event.cpp
	SoMotion3Event.cpp
	SoMouseButtonEvent.cpp
	SoSpaceballButtonEvent.cpp
)

# build module object library
obol_add_source_module(events ${OBOL_EVENTS_FILES})

# define documentation
foreach(SRCFILE ${OBOL_EVENTS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/events/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
