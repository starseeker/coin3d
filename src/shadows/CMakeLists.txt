# glsl2c macro for converting GLSL shader files to C header files
macro(glsl2c _input _postfix)
  get_filename_component(_basename "${_input}" NAME_WE)
  get_filename_component(_suffix "${_input}" EXT)
  get_filename_component(_path "${_input}" PATH)
  string(SUBSTRING ${_suffix} 1 -1 _suffix)
  string(TOUPPER "SO_${_basename}_${_suffix}_H" COIN_HEADER_DEF)
  string(TOUPPER ${_basename} COIN_TEXTVAR_NAME)
  set(COIN_TEXTVAR_NAME "${COIN_TEXTVAR_NAME}${_postfix}")
  #message(STATUS "Parse: ${PROJECT_SOURCE_DIR}/${_input}")
  FILE(READ ${PROJECT_SOURCE_DIR}/${_input} f0)
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" f1 "${f0}")
  STRING(REGEX REPLACE "\"" "\\\\\"" f2 "${f1}")
  STRING(REGEX REPLACE "\r?\n" "\\\\n\"\n  \"" COIN_STR_SOURCE_CODE "${f2}")
  # Remove the src/shadows prefix from the path since we're already in that directory
  string(REPLACE "src/shadows/" "" _relative_path "${_path}")
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/glsl2ctemplate.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${_relative_path}/${_basename}.h")
endmacro()

# process shader files in src/shadows
file(GLOB_RECURSE COIN_SHADOWS_SHADER_GLSL RELATIVE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.glsl)
foreach(_input_file ${COIN_SHADOWS_SHADER_GLSL})
  glsl2c(${_input_file} "_shadersource")
endforeach()

# source files
set(COIN_SHADOWS_FILES
	SoShadowGroup.cpp
	SoShadowStyle.cpp
	SoShadowSpotLight.cpp
	SoShadowDirectionalLight.cpp
	SoShadowStyleElement.cpp
	SoShadowCulling.cpp
	SoGLShadowCullingElement.cpp
)

# build library
if(COIN_BUILD_SINGLE_LIB)
	foreach(SRCFILE ${COIN_SHADOWS_FILES})
		set(COIN_SOURCE ${COIN_SOURCE} shadows/${SRCFILE} CACHE INTERNAL "COIN_SOURCE")
	endforeach(SRCFILE)
else()
	add_library(shadows OBJECT ${COIN_SHADOWS_FILES})
endif()

# define documentation
foreach(SRCFILE ${COIN_SHADOWS_FILES})
	set(COIN_DOCUMENTATION_FILES ${COIN_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/shadows/${SRCFILE} CACHE INTERNAL "COIN_DOCUMENTATION_FILES")
endforeach(SRCFILE)
