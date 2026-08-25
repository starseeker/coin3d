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

# build library
foreach(SRCFILE ${OBOL_EVENTS_FILES})
	set_property(GLOBAL APPEND PROPERTY OBOL_SOURCE_FILES events/${SRCFILE})
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_EVENTS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/events/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
