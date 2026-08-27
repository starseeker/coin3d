# source files
set(OBOL_MISC_FILES
	SoBase.cpp
	SoBaseP.cpp
	SoChildList.cpp
	SoCompactPathList.cpp
	SoConfigSettings.cpp
	SoContextHandler.cpp
	SoDB.cpp
	SoDebug.cpp
	SoFullPath.cpp
	SoGenerate.cpp
	SoInteraction.cpp
	SoLightPath.cpp
	SoLockManager.cpp
	SoNormalGenerator.cpp
	SoNotRec.cpp
	SoNotification.cpp
	SoPath.cpp
	SoPick.cpp
	SoPickedPoint.cpp
	SoPrimitiveVertex.cpp
	SoProto.cpp
	SoProtoInstance.cpp
	SoSceneManager.cpp
	SoSceneManagerP.cpp
	SoShaderGenerator.cpp
	SoState.cpp
	SoTempPath.cpp
	SoType.cpp
	CoinResources.cpp
	SoDBP.cpp
	SoEventManager.cpp
	SoViewport.cpp
	SoQuadViewport.cpp
	SoSceneCollector.cpp
)

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_MISC_INTERNAL_FILES
	SbHash.h
	SoBaseP.h
	SoBaseP.cpp
	SoCompactPathList.h
	SoCompactPathList.cpp
	SoConfigSettings.h
	SoConfigSettings.cpp
	SoDBP.h
	SoDBP.cpp
	SoGenerate.h
	SoGenerate.cpp
	SoPick.h
	SoPick.cpp
	SoSceneManagerP.h
	SoSceneManagerP.cpp
	SoShaderGenerator.h
	SoShaderGenerator.cpp
)

set(_obol_misc_sources ${OBOL_MISC_FILES})
if(OBOL_USE_SWRAST)
	list(APPEND _obol_misc_sources SoDBOSMesa.cpp)
	if(OBOL_USE_SYSTEM_GL)
		# Keep the OSMesa context implementation isolated from system-GL
		# translation units and give that source access to the private headers.
		set_source_files_properties(SoDBOSMesa.cpp PROPERTIES
			SKIP_UNITY_BUILD_INCLUSION ON
			INCLUDE_DIRECTORIES "${OBOL_OSMESA_INCLUDE_DIRS}")
	endif()
endif()
list(APPEND _obol_misc_sources CoinTidbits.cpp)
obol_add_source_module(misc ${_obol_misc_sources})

# define documentation
foreach(SRCFILE ${OBOL_MISC_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_MISC_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
