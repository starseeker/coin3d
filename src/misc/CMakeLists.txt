# source files
set(COIN_MISC_FILES
	CoinStaticObjectInDLL.cpp
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
)

# Files excluded from public API documentation, included in complete documentation.
set(COIN_MISC_INTERNAL_FILES
	CoinStaticObjectInDLL.h
	CoinStaticObjectInDLL.cpp
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

# build library
if(COIN_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${COIN_MISC_FILES})
		set(COIN_SOURCE ${COIN_SOURCE} misc/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
	endforeach(SRCFILE)
	if(COIN3D_USE_OSMESA)
		# Pure-OSMesa build: whole library uses OSMesa headers, compile normally.
		set(COIN_SOURCE ${COIN_SOURCE} misc/SoDBOSMesa.cpp CACHE INTERNAL "COIN_SOURCE")
	elseif(COIN3D_BUILD_DUAL_GL)
		# Dual-GL build: SoDBOSMesa.cpp needs the OSMesa include paths.
		# Use a separate object target (same pattern as glue_osmesa).
		add_library(misc_osmesa OBJECT SoDBOSMesa.cpp)
		target_compile_definitions(misc_osmesa PRIVATE
			COIN_INTERNAL
			COIN3D_BUILD_DUAL_GL
		)
		target_include_directories(misc_osmesa PRIVATE
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/include/Inventor/annex
			${PROJECT_BINARY_DIR}/include
			${COIN_TARGET_INCLUDE_DIRECTORIES}
		)
		if(COIN_OSMESA_INCLUDE_DIRS)
			target_include_directories(misc_osmesa PRIVATE ${COIN_OSMESA_INCLUDE_DIRS})
		endif()
	endif()
else()
	add_library(misc OBJECT ${COIN_MISC_FILES})
	if(COIN3D_USE_OSMESA)
		# Pure-OSMesa build: all sources already have OSMesa in their include
		# path via the main build settings; add SoDBOSMesa.cpp to the same TU.
		target_sources(misc PRIVATE SoDBOSMesa.cpp)
	elseif(COIN3D_BUILD_DUAL_GL)
		# Dual-GL: separate object with OSMesa includes.
		add_library(misc_osmesa OBJECT SoDBOSMesa.cpp)
		target_compile_definitions(misc_osmesa PRIVATE
			COIN_INTERNAL
			COIN3D_BUILD_DUAL_GL
		)
		target_include_directories(misc_osmesa PRIVATE
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/include/Inventor/annex
			${PROJECT_BINARY_DIR}/include
			${COIN_TARGET_INCLUDE_DIRECTORIES}
		)
		if(COIN_OSMESA_INCLUDE_DIRS)
			target_include_directories(misc_osmesa PRIVATE ${COIN_OSMESA_INCLUDE_DIRS})
		endif()
	endif()
endif()

# define documentation
foreach(SRCFILE ${COIN_MISC_FILES})
	set(COIN_DOCUMENTATION_FILES ${COIN_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "COIN_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${COIN_MISC_INTERNAL_FILES})
	set(COIN_INTERNAL_DOCUMENTATION_FILES ${COIN_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "COIN_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
