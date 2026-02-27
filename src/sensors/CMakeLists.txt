# source files
set(OBOL_SENSORS_FILES
	SoAlarmSensor.cpp
	SoDataSensor.cpp
	SoDelayQueueSensor.cpp
	SoFieldSensor.cpp
	SoIdleSensor.cpp
	SoNodeSensor.cpp
	SoOneShotSensor.cpp
	SoPathSensor.cpp
	SoSensor.cpp
	SoSensorManager.cpp
	SoTimerQueueSensor.cpp
	SoTimerSensor.cpp
)

# build library
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_SENSORS_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} sensors/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(sensors OBJECT ${OBOL_SENSORS_FILES})
endif()

# define documentation
foreach(SRCFILE ${OBOL_SENSORS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/sensors/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
