# source files
set(OBOL_ENGINES_FILES
	SoBoolOperation.cpp
	SoCalculator.cpp
	SoComposeMatrix.cpp
	SoComposeRotation.cpp
	SoComposeRotationFromTo.cpp
	SoComposeVec2f.cpp
	SoComposeVec3f.cpp
	SoComposeVec4f.cpp
	SoComputeBoundingBox.cpp
	SoConcatenate.cpp
	SoConvertAll.cpp
	SoCounter.cpp
	SoDecomposeMatrix.cpp
	SoDecomposeRotation.cpp
	SoDecomposeVec2f.cpp
	SoDecomposeVec3f.cpp
	SoDecomposeVec4f.cpp
	SoElapsedTime.cpp
	SoEngine.cpp
	SoEngineOutput.cpp
	SoFieldConverter.cpp
	SoGate.cpp
	SoInterpolate.cpp
	SoInterpolateFloat.cpp
	SoInterpolateRotation.cpp
	SoInterpolateVec2f.cpp
	SoInterpolateVec3f.cpp
	SoInterpolateVec4f.cpp
	SoNodeEngine.cpp
	SoOnOff.cpp
	SoOneShot.cpp
	SoOutputData.cpp
	SoSelectOne.cpp
	SoTimeCounter.cpp
	SoTransformVec3f.cpp
	SoTriggerAny.cpp
	SoTexture2Convert.cpp
	SoHeightMapToNormalMap.cpp
	evaluator.cpp
	evaluator_tab.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_ENGINES_INTERNAL_FILES
	SoConvertAll.h
	SoConvertAll.cpp
	evaluator.h
	evaluator.cpp
	evaluator_tab.cpp
	SoSubEngineP.h
	SoSubNodeEngineP.h
)

# build library
foreach(SRCFILE ${OBOL_ENGINES_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} engines/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_ENGINES_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/engines/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_ENGINES_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/engines/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
