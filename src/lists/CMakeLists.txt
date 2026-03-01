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

# build library
foreach(SRCFILE ${OBOL_LISTS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} lists/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_LISTS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/lists/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
