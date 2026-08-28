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
