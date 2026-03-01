# Macro to convert .iv files to .h headers containing geometry data
macro(strfy _input _postfix)
  get_filename_component(_basename "${_input}" NAME_WE)
  get_filename_component(_suffix "${_input}" EXT)
  get_filename_component(_path "${_input}" PATH)
  string(SUBSTRING ${_suffix} 1 -1 _suffix)
  string(TOUPPER "SO_${_basename}_${_suffix}_H" OBOL_HEADER_DEF)
  string(TOUPPER ${_basename} OBOL_TEXTVAR_NAME)
  set(OBOL_TEXTVAR_NAME "${OBOL_TEXTVAR_NAME}${_postfix}")
  #message(STATUS "Parse: ${CMAKE_CURRENT_SOURCE_DIR}/${_input}")
  FILE(READ ${CMAKE_CURRENT_SOURCE_DIR}/${_input} f0)
  STRING(REGEX REPLACE "\\\\" "\\\\\\\\" f1 "${f0}")
  STRING(REGEX REPLACE "\"" "\\\\\"" f2 "${f1}")
  STRING(REGEX REPLACE "\r?\n" "\\\\n\"\n  \"" OBOL_STR_SOURCE_CODE "${f2}")
  configure_file(${PROJECT_SOURCE_DIR}/cmake/strfytemplate.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/${_path}/${_basename}.h")
endmacro()

# Process dragger .iv files to generate header files
file(GLOB_RECURSE OBOL_DRAGGER_IV_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} defaults/*.iv)
foreach(_input_file ${OBOL_DRAGGER_IV_FILES})
  strfy(${_input_file} "_draggergeometry")
endforeach()

# Set common resources (for macOS framework builds)
file(GLOB_RECURSE COMMON_DRAGGER_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/defaults/*.iv)
set(COMMON_RESOURCES ${COMMON_DRAGGER_RESOURCES} PARENT_SCOPE)

# Add binary directory to include path for generated headers
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# source files
set(OBOL_DRAGGERS_FILES
	SoDragger.cpp
	SoCenterballDragger.cpp
	SoDirectionalLightDragger.cpp
	SoDragPointDragger.cpp
	SoHandleBoxDragger.cpp
	SoJackDragger.cpp
	SoPointLightDragger.cpp
	SoRotateCylindricalDragger.cpp
	SoRotateDiscDragger.cpp
	SoRotateSphericalDragger.cpp
	SoScale1Dragger.cpp
	SoScale2Dragger.cpp
	SoScale2UniformDragger.cpp
	SoScaleUniformDragger.cpp
	SoSpotLightDragger.cpp
	SoTabBoxDragger.cpp
	SoTabPlaneDragger.cpp
	SoTrackballDragger.cpp
	SoTransformBoxDragger.cpp
	SoTransformerDragger.cpp
	SoTranslate1Dragger.cpp
	SoTranslate2Dragger.cpp
)

# build library
foreach(SRCFILE ${OBOL_DRAGGERS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} draggers/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_DRAGGERS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/draggers/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
