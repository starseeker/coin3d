# source files
set(OBOL_LISTS_FILES
	SbList.cpp
	SbPList.cpp
	SbIntList.cpp
	SbVec3fList.cpp
	SbStringList.cpp
	SoActionMethodList.cpp
	SoAuditorList.cpp
	SoBaseList.cpp
	SoCallbackList.cpp
	SoDetailList.cpp
	SoEnabledElementsList.cpp
	SoEngineList.cpp
	SoEngineOutputList.cpp
	SoFieldList.cpp
	SoNodeList.cpp
	SoPathList.cpp
	SoPickedPointList.cpp
	SoTypeList.cpp
)

# build module object library
obol_add_source_module(lists ${OBOL_LISTS_FILES})
