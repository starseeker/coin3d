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
