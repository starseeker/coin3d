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
