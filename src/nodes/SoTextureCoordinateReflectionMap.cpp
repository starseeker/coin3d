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

/*!
  \class SoTextureCoordinateReflectionMap SoTextureCoordinateReflectionMap.h Inventor/nodes/SoTextureCoordinateReflectionMap.h
  \brief The SoTextureCoordinateReflectionMap class generates 3D reflection texture coordinates.

  \ingroup coin_nodes

  This node is usually used along with a SoCubeMapTexture node...
  
  FIXME: more doc.

  <b>FILE FORMAT/DEFAULTS:</b>
  \code
    TextureCoordinateReflectionMap {
    }
  \endcode
*/

// *************************************************************************

#include <Inventor/nodes/SoTextureCoordinateReflectionMap.h>
#include "glue/glp.h"
#include "config.h"

#include <cstdlib>
#include <cfloat>


#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoPickAction.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/elements/SoGLMultiTextureCoordinateElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoTextureUnitElement.h>
#include <Inventor/system/gl.h>

#include "nodes/SoSubNodeP.h"

// *************************************************************************

SO_NODE_SOURCE(SoTextureCoordinateReflectionMap);

/*!
  Constructor.
*/
SoTextureCoordinateReflectionMap::SoTextureCoordinateReflectionMap()
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoTextureCoordinateReflectionMap);
}

/*!
  Destructor.
*/
SoTextureCoordinateReflectionMap::~SoTextureCoordinateReflectionMap()
{
}

// doc in super
/*!
  \copybrief SoBase::initClass(void)
*/
void
SoTextureCoordinateReflectionMap::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoTextureCoordinateReflectionMap, SO_FROM_INVENTOR_1);

  SO_ENABLE(SoGLRenderAction, SoGLMultiTextureCoordinateElement);
  SO_ENABLE(SoCallbackAction, SoMultiTextureCoordinateElement);
  SO_ENABLE(SoPickAction, SoMultiTextureCoordinateElement);
}

// generates texture coordinates for GLRender, callback and pick actions
const SbVec4f &
SoTextureCoordinateReflectionMap::generate(void *userdata,
                                         const SbVec3f & /* p */,
                                         const SbVec3f &n)
{
  static thread_local SbVec4f texcoords(0.0f, 0.0f, 0.0f, 1.0f);
  SoState *state = (SoState*)userdata;
  SbVec3f wn; // normal in world (eye) coordinates
  SoModelMatrixElement::get(state).multDirMatrix(n, wn);
  SbVec3f u = n;

  u.normalize();
  wn.normalize();

  // reflection vector
  SbVec3f r = u - SbVec3f(2.0f*wn[0]*wn[0]*u[0],
                          2.0f*wn[1]*wn[1]*u[1],
                          2.0f*wn[2]*wn[2]*u[2]);
  r.normalize();

  texcoords.setValue(r[0], r[1], r[2], 1.0f);
  return texcoords;
}

// doc from parent
void
SoTextureCoordinateReflectionMap::doAction(SoAction * action)
{
  SoState * state = action->getState();
  int unit = SoTextureUnitElement::get(state);

  SoMultiTextureCoordinateElement::setFunction(action->getState(), this, unit,
                                               generate,
                                               action->getState());
}

// doc from parent
void
SoTextureCoordinateReflectionMap::GLRender(SoGLRenderAction * action)
{
  SoState * state = action->getState();
  int unit = SoTextureUnitElement::get(state);
  const SoGLContext * glue =
    SoGLContext_instance(SoGLCacheContextElement::get(state));
  SoMultiTextureCoordinateElement::setFunction(action->getState(), this,
                                               unit,
                                               generate,
                                               action->getState());
  SoGLMultiTextureCoordinateElement::setTexGen(action->getState(),
                                               this, unit, handleTexgen, 
                                               const_cast<SoGLContext *>(glue),
                                                 generate,
                                               action->getState());
  
}

// doc from parent
void
SoTextureCoordinateReflectionMap::callback(SoCallbackAction * action)
{
  SoTextureCoordinateReflectionMap::doAction((SoAction *)action);
}

// doc from parent
void
SoTextureCoordinateReflectionMap::pick(SoPickAction * action)
{
  SoTextureCoordinateReflectionMap::doAction((SoAction *)action);
}

void
SoTextureCoordinateReflectionMap::handleTexgen(void * data)
{
  const SoGLContext * glue = static_cast<const SoGLContext *>(data);
  SoGLContext_glTexGeni(glue, GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP);
  SoGLContext_glTexGeni(glue, GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP);
  SoGLContext_glTexGeni(glue, GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP);
}
