# source files
set(OBOL_SHADERS_FILES
	SoFragmentShader.cpp
	SoGeometryShader.cpp
	SoGLARBShaderParameter.cpp
	SoGLARBShaderObject.cpp
	SoGLARBShaderProgram.cpp
	SoGLSLShaderParameter.cpp
	SoGLSLShaderObject.cpp
	SoGLSLShaderProgram.cpp
	SoGLShaderObject.cpp
	SoGLShaderParameter.cpp
	SoGLShaderProgram.cpp
	SoGLShaderProgramElement.cpp
	SoShaderObject.cpp
	SoShaderParameter.cpp
	SoShaderProgram.cpp
	SoShader.cpp
	SoVertexShader.cpp
)


# build module object library
obol_add_source_module(shaders ${OBOL_SHADERS_FILES})
target_include_directories(obol_shaders_objects PRIVATE
  "${PROJECT_BINARY_DIR}/src/shadows")
