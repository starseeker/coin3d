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
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_EVENTS_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} events/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(events OBJECT ${OBOL_EVENTS_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_EVENTS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/events/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
