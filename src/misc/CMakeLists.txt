# source files
set(OBOL_MISC_FILES
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
set(OBOL_MISC_INTERNAL_FILES
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
if(OBOL_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${OBOL_MISC_FILES})
		set(OBOL_SOURCE ${OBOL_SOURCE} misc/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
	endforeach(SRCFILE)
	if(OBOL_USE_OSMESA)
		# Pure-OSMesa build: whole library uses OSMesa headers, compile normally.
		set(OBOL_SOURCE ${OBOL_SOURCE} misc/SoDBOSMesa.cpp CACHE INTERNAL "OBOL_SOURCE")
	elseif(OBOL_BUILD_DUAL_GL)
		# Dual-GL build: SoDBOSMesa.cpp needs the OSMesa include paths.
		# Use a separate object target (same pattern as glue_osmesa).
		add_library(misc_osmesa OBJECT SoDBOSMesa.cpp)
		target_compile_definitions(misc_osmesa PRIVATE
			OBOL_INTERNAL
			OBOL_BUILD_DUAL_GL
		)
		target_include_directories(misc_osmesa PRIVATE
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/include/Inventor/annex
			${PROJECT_BINARY_DIR}/include
			${OBOL_TARGET_INCLUDE_DIRECTORIES}
		)
		if(OBOL_OSMESA_INCLUDE_DIRS)
			target_include_directories(misc_osmesa PRIVATE ${OBOL_OSMESA_INCLUDE_DIRS})
		endif()
	endif()
else()
	add_library(misc OBJECT ${OBOL_MISC_FILES})
	if(OBOL_USE_OSMESA)
		# Pure-OSMesa build: all sources already have OSMesa in their include
		# path via the main build settings; add SoDBOSMesa.cpp to the same TU.
		target_sources(misc PRIVATE SoDBOSMesa.cpp)
	elseif(OBOL_BUILD_DUAL_GL)
		# Dual-GL: separate object with OSMesa includes.
		add_library(misc_osmesa OBJECT SoDBOSMesa.cpp)
		target_compile_definitions(misc_osmesa PRIVATE
			OBOL_INTERNAL
			OBOL_BUILD_DUAL_GL
		)
		target_include_directories(misc_osmesa PRIVATE
			${PROJECT_SOURCE_DIR}/include
			${PROJECT_SOURCE_DIR}/include/Inventor/annex
			${PROJECT_BINARY_DIR}/include
			${OBOL_TARGET_INCLUDE_DIRECTORIES}
		)
		if(OBOL_OSMESA_INCLUDE_DIRS)
			target_include_directories(misc_osmesa PRIVATE ${OBOL_OSMESA_INCLUDE_DIRS})
		endif()
	endif()
endif()

# define documentation
foreach(SRCFILE ${OBOL_MISC_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_MISC_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/misc/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
