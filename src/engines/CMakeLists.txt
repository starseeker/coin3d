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

# ############################################################################
# SoCalculator expression parser: bison + flex regeneration
#
# evaluator_tab.cpp and so_eval.ic are committed pre-built outputs so the
# project can be built on systems without bison/flex (e.g. Windows).  When
# both tools ARE present the build regenerates them from evaluator.y/.l to
# keep the grammar sources from going stale.
#
# Use -DOBOL_USE_BUILTIN_PARSER=ON to force the committed files even when
# bison/flex are available (useful for testing the shipped fallback).
# ############################################################################

option(OBOL_USE_BUILTIN_PARSER
  "Use the pre-generated evaluator_tab.cpp/so_eval.ic instead of regenerating from the grammar (default: OFF when bison+flex are found)"
  OFF)

if(NOT OBOL_USE_BUILTIN_PARSER)
  find_package(BISON 3.0)
  find_package(FLEX 2.6)
endif()

if(BISON_FOUND AND FLEX_FOUND AND NOT OBOL_USE_BUILTIN_PARSER)
  message(STATUS "Bison ${BISON_VERSION} and flex ${FLEX_VERSION} found - "
                 "regenerating evaluator_tab.cpp and so_eval.ic from grammar sources")

  # Generate the lexer first (so_eval.ic) because the bison output #includes it.
  # The output is written directly to the source tree so that:
  #   (a) it is always up-to-date for source-only distributions, and
  #   (b) we do not have to manage build-dir vs source-dir include paths.
  add_custom_command(
    OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/so_eval.ic"
    COMMAND ${FLEX_EXECUTABLE} -o so_eval.ic -L evaluator.l
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/evaluator.l"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Regenerating so_eval.ic from evaluator.l"
    VERBATIM
  )

  add_custom_command(
    OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/evaluator_tab.cpp"
    COMMAND ${BISON_EXECUTABLE} -o evaluator_tab.cpp -l evaluator.y
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/evaluator.y"
            "${CMAKE_CURRENT_SOURCE_DIR}/so_eval.ic"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Regenerating evaluator_tab.cpp from evaluator.y"
    VERBATIM
  )

  # Custom target so dependent targets rebuild when the grammar changes
  add_custom_target(obol_parser_gen
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/so_eval.ic"
      "${CMAKE_CURRENT_SOURCE_DIR}/evaluator_tab.cpp"
  )
else()
  if(OBOL_USE_BUILTIN_PARSER)
    message(STATUS "OBOL_USE_BUILTIN_PARSER=ON: using committed evaluator_tab.cpp/so_eval.ic")
  else()
    message(STATUS "bison/flex not found - using committed evaluator_tab.cpp/so_eval.ic")
  endif()
endif()

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
