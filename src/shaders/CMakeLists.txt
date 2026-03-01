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

# Files excluded from public API documentation, included in complete documentation.
set(OBOL_SHADERS_INTERNAL_FILES
	SoGLARBShaderParameter.h
	SoGLARBShaderParameter.cpp
	SoGLARBShaderObject.h
	SoGLARBShaderObject.cpp
	SoGLARBShaderProgram.h
	SoGLARBShaderProgram.cpp
	SoGLSLShaderParameter.h
	SoGLSLShaderParameter.cpp
	SoGLSLShaderObject.h
	SoGLSLShaderObject.cpp
	SoGLSLShaderProgram.h
	SoGLSLShaderProgram.cpp
	SoGLShaderObject.h
	SoGLShaderObject.cpp
	SoGLShaderParameter.h
	SoGLShaderParameter.cpp
	SoGLShaderProgram.h
	SoGLShaderProgram.cpp
	SoGLShaderProgramElement.cpp
)

# build library
foreach(SRCFILE ${OBOL_SHADERS_FILES})
	set(OBOL_SOURCE ${OBOL_SOURCE} shaders/${SRCFILE} CACHE INTERNAL "OBOL_SOURCE")
endforeach(SRCFILE)

# define documentation
foreach(SRCFILE ${OBOL_SHADERS_FILES})
	set(OBOL_DOCUMENTATION_FILES ${OBOL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/shaders/${SRCFILE} CACHE INTERNAL "OBOL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
foreach(SRCFILE ${OBOL_SHADERS_INTERNAL_FILES})
	set(OBOL_INTERNAL_DOCUMENTATION_FILES ${OBOL_INTERNAL_DOCUMENTATION_FILES} ${PROJECT_SOURCE_DIR}/src/shaders/${SRCFILE} CACHE INTERNAL "OBOL_INTERNAL_DOCUMENTATION_FILES")
endforeach(SRCFILE)
