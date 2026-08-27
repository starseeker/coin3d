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

# build module object library
obol_add_source_module(sensors ${OBOL_SENSORS_FILES})

# define documentation
foreach(SRCFILE ${OBOL_SENSORS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/sensors/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
