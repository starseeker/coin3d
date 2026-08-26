/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#include "SoGLARBShaderParameter.h"
#include "SoGLARBShaderObject.h"
#include "config.h"

#include <cstring>
#include <climits>

#include <Inventor/errors/SoDebugError.h>

// *************************************************************************

SoGLARBShaderParameter::SoGLARBShaderParameter(void)
{
}

SoGLARBShaderParameter::~SoGLARBShaderParameter()
{
}

SoShader::Type
SoGLARBShaderParameter::shaderType(void) const
{
  return SoShader::ARB_SHADER;
}

void
SoGLARBShaderParameter::set1f(const SoGLShaderObject * shader,
                              const float value, const char*, const int idx)
{
  GLenum target;
  if (this->isValid(shader, idx, 1, target))
    SoGLContext_glProgramLocalParameter4f(shader->GLContext(),
                                        target, static_cast<GLuint>(idx),
                                        value, value, value, value);
}

void
SoGLARBShaderParameter::set2f(const SoGLShaderObject * shader,
                              const float * value, const char*, const int idx)
{
  GLenum target;
  if (this->isValid(shader, idx, 1, target))
    SoGLContext_glProgramLocalParameter4f(shader->GLContext(),
                                        target, static_cast<GLuint>(idx),
                                        value[0], value[1], value[0], value[0]);
}

void
SoGLARBShaderParameter::set3f(const SoGLShaderObject * shader,
                              const float * value, const char*, const int idx)
{
  GLenum target;
  if (this->isValid(shader, idx, 1, target))
    SoGLContext_glProgramLocalParameter4f(shader->GLContext(),
                                        target, static_cast<GLuint>(idx),
                                        value[0], value[1], value[2], value[0]);
}

void
SoGLARBShaderParameter::set4f(const SoGLShaderObject * shader,
                              const float * value, const char*, const int idx)
{
  GLenum target;
  if (this->isValid(shader, idx, 1, target))
    SoGLContext_glProgramLocalParameter4f(shader->GLContext(),
                                        target, static_cast<GLuint>(idx),
                                        value[0], value[1], value[2], value[3]);
}

void
SoGLARBShaderParameter::set1fv(const SoGLShaderObject * shader, const int num,
                               const float * value, const char *, const int idx)
{
  this->setFloats(shader, num, value, 1, idx);
}

void
SoGLARBShaderParameter::set2fv(const SoGLShaderObject * shader, const int num,
                               const float * value, const char *, const int idx)
{
  this->setFloats(shader, num, value, 2, idx);
}

void
SoGLARBShaderParameter::set3fv(const SoGLShaderObject * shader, const int num,
                               const float * value, const char *, const int idx)
{
  this->setFloats(shader, num, value, 3, idx);
}

void
SoGLARBShaderParameter::set4fv(const SoGLShaderObject * shader, const int num,
                               const float * value, const char *, const int idx)
{
  this->setFloats(shader, num, value, 4, idx);
}

void
SoGLARBShaderParameter::setMatrix(const SoGLShaderObject * shader,
                                  const float * value, const char *, const int idx)
{
  this->setFloats(shader, 4, value, 4, idx);
}


void
SoGLARBShaderParameter::setMatrixArray(const SoGLShaderObject * shader,
                                       const int num, const float * value,
                                       const char *, const int idx)
{
  if (num > INT_MAX / 4) {
    SoDebugError::postWarning("SoGLARBShaderParameter::setMatrixArray",
                              "matrix array is too large");
    return;
  }
  this->setFloats(shader, num * 4, value, 4, idx);
}

void
SoGLARBShaderParameter::set1i(const SoGLShaderObject * shader,
                              const int32_t value, const char *, const int idx)
{
  this->setIntegers(shader, 1, &value, 1, idx);
}

void
SoGLARBShaderParameter::set2i(const SoGLShaderObject * shader,
                              const int32_t * value, const char *, const int idx)
{
  this->setIntegers(shader, 1, value, 2, idx);
}

void
SoGLARBShaderParameter::set3i(const SoGLShaderObject * shader,
                              const int32_t * value, const char *, const int idx)
{
  this->setIntegers(shader, 1, value, 3, idx);
}

void
SoGLARBShaderParameter::set4i(const SoGLShaderObject * shader,
                              const int32_t * value, const char *, const int idx)
{
  this->setIntegers(shader, 1, value, 4, idx);
}

void
SoGLARBShaderParameter::set1iv(const SoGLShaderObject * shader, const int num,
                               const int32_t * value, const char *, const int idx)
{
  this->setIntegers(shader, num, value, 1, idx);
}

void
SoGLARBShaderParameter::set2iv(const SoGLShaderObject * shader, const int num,
                               const int32_t * value, const char *, const int idx)
{
  this->setIntegers(shader, num, value, 2, idx);
}

void
SoGLARBShaderParameter::set3iv(const SoGLShaderObject * shader, const int num,
                               const int32_t * value, const char *, const int idx)
{
  this->setIntegers(shader, num, value, 3, idx);
}

void
SoGLARBShaderParameter::set4iv(const SoGLShaderObject * shader, const int num,
                               const int32_t * value, const char *, const int idx)
{
  this->setIntegers(shader, num, value, 4, idx);
}

SbBool
SoGLARBShaderParameter::isValid(const SoGLShaderObject * shader, const int idx,
                                const int count, GLenum & target) const
{
  assert(shader);
  assert(shader->shaderType() == SoShader::ARB_SHADER);
  if (shader == NULL || shader->shaderType() != SoShader::ARB_SHADER ||
      idx < 0 || count <= 0) {
    SoDebugError::postWarning("SoGLARBShaderParameter::isValid",
                              "invalid ARB parameter index %d", idx);
    return FALSE;
  }

  target = static_cast<const SoGLARBShaderObject *>(shader)->target;
  GLint maximum = 0;
  SoGLContext_glGetIntegerv(shader->GLContext(),
                           GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB, &maximum);
  if (maximum <= 0 || idx >= maximum || count > maximum - idx) {
    SoDebugError::postWarning("SoGLARBShaderParameter::isValid",
                              "ARB parameter range [%d, %d] exceeds the "
                              "implementation limit of %d",
                              idx, idx + count - 1, maximum);
    return FALSE;
  }
  return TRUE;
}

void
SoGLARBShaderParameter::setFloats(const SoGLShaderObject * shader, int count,
                                  const float * values, int components,
                                  int idx) const
{
  if (count <= 0 || values == NULL || components < 1 || components > 4) return;
  if (idx < 0 || count - 1 > INT_MAX - idx) {
    SoDebugError::postWarning("SoGLARBShaderParameter::setFloats",
                              "ARB parameter range overflows its integer index");
    return;
  }
  GLenum target;
  if (!this->isValid(shader, idx, count, target)) return;

  for (int element = 0; element < count; ++element) {
    const float * value = values + element * components;
    float packed[4] = { value[0], value[0], value[0], value[0] };
    for (int component = 1; component < components; ++component) {
      packed[component] = value[component];
    }
    SoGLContext_glProgramLocalParameter4fv(
      shader->GLContext(), target, static_cast<GLuint>(idx + element), packed);
  }
}

void
SoGLARBShaderParameter::setIntegers(const SoGLShaderObject * shader, int count,
                                    const int32_t * values, int components,
                                    int idx) const
{
  if (count <= 0 || values == NULL || components < 1 || components > 4) return;
  if (idx < 0 || count - 1 > INT_MAX - idx) {
    SoDebugError::postWarning("SoGLARBShaderParameter::setIntegers",
                              "ARB parameter range overflows its integer index");
    return;
  }
  GLenum target;
  if (!this->isValid(shader, idx, count, target)) return;

  for (int element = 0; element < count; ++element) {
    const int32_t * value = values + element * components;
    float packed[4] = {
      static_cast<float>(value[0]), static_cast<float>(value[0]),
      static_cast<float>(value[0]), static_cast<float>(value[0])
    };
    for (int component = 1; component < components; ++component) {
      packed[component] = static_cast<float>(value[component]);
    }
    SoGLContext_glProgramLocalParameter4fv(
      shader->GLContext(), target, static_cast<GLuint>(idx + element), packed);
  }
}
