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
  \class SoTextureCoordinateEnvironment SoTextureCoordinateEnvironment.h Inventor/nodes/SoTextureCoordinateEnvironment.h
  \brief The SoTextureCoordinateEnvironment class generates texture coordinates by projecting onto a surrounding texture.

  \ingroup coin_nodes

  The texture specifying the environment will be mapped around the 
  scene graph below this node using a sphere. The texture will be mapped
  onto the scene graph taking camera position into account. This will
  lead to an object reflecting its environment.

  Here is a scene graph example showing how environment mapping can be
  applied to an object:

  \code
  #Inventor V2.1 ascii

  Separator {

    Texture2 {
      filename "ocean.jpg" # the environment, in this case ocean
    }
    TextureCoordinateEnvironment {}

    Cube {} # the environmentally mapped object
  }
  \endcode

  <b>FILE FORMAT/DEFAULTS:</b>
  \code
    TextureCoordinateEnvironment {
    }
  \endcode
*/

// *************************************************************************

// FIXME: Can this somehow relate to 3D textures? (kintel 20020203)

#include <Inventor/nodes/SoTextureCoordinateEnvironment.h>

#include <cstdlib>
#include <cfloat>

#include "config.h"

#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/elements/SoGLMultiTextureCoordinateElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoTextureUnitElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/system/gl.h>
#include "glue/glp.h"

#include "CoinTidbits.h"
#include "nodes/SoSubNodeP.h"

// *************************************************************************

class SoTextureCoordinateEnvironmentP {
public:
  static SbVec4f * dummy_texcoords;
  static void cleanup_func(void);
};

SbVec4f * SoTextureCoordinateEnvironmentP::dummy_texcoords = NULL;

void
SoTextureCoordinateEnvironmentP::cleanup_func(void)
{
  delete SoTextureCoordinateEnvironmentP::dummy_texcoords;
}

// *************************************************************************

SO_NODE_SOURCE(SoTextureCoordinateEnvironment);

/*!
  Constructor.
*/
SoTextureCoordinateEnvironment::SoTextureCoordinateEnvironment()
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoTextureCoordinateEnvironment);
}

/*!
  Destructor.
*/
SoTextureCoordinateEnvironment::~SoTextureCoordinateEnvironment()
{
}

// doc in super
/*!
  \copybrief SoBase::initClass(void)
*/
void
SoTextureCoordinateEnvironment::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoTextureCoordinateEnvironment, SO_FROM_INVENTOR_1);

  SoTextureCoordinateEnvironmentP::dummy_texcoords = new SbVec4f(0.0f, 0.0f, 0.0f, 1.0f);
  coin_atexit((coin_atexit_f *)SoTextureCoordinateEnvironmentP::cleanup_func, CC_ATEXIT_NORMAL);
}

// Software sphere-map coordinate generator.  Used by callback and pick
// actions, and registered as the fallback path for GLRender.  Produces
// coordinates equivalent to OpenGL GL_SPHERE_MAP hardware texgen (set up
// in handleTexgen()).  The formula is from the OpenGL Programming Guide
// (Red Book), section on sphere mapping.
const SbVec4f &
SoTextureCoordinateEnvironment::generate(void *userdata,
                                         const SbVec3f & /* p */,
                                         const SbVec3f &n)
{
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

  float tmp = 1.0f + r[2];
  float m = 2.0f * (float)sqrt(r[0]*r[0] + r[1]*r[1] + tmp*tmp);

  // in case an empty normal was supplied
  if (fabs(m) <= FLT_EPSILON) m = 1.0f;

  (*SoTextureCoordinateEnvironmentP::dummy_texcoords)[0] = r[0] / m + 0.5f;
  (*SoTextureCoordinateEnvironmentP::dummy_texcoords)[1] = r[1] / m + 0.5f;
  return *SoTextureCoordinateEnvironmentP::dummy_texcoords;
}

// doc from parent
void
SoTextureCoordinateEnvironment::doAction(SoAction * action)
{
  SoState * state = action->getState();
  int unit = SoTextureUnitElement::get(state);
  SoMultiTextureCoordinateElement::setFunction(action->getState(), this, unit,
                                               generate,
                                               action->getState());
}

// doc from parent
void
SoTextureCoordinateEnvironment::GLRender(SoGLRenderAction * action)
{
  SoState * state = action->getState();
  int unit = SoTextureUnitElement::get(state);
  const SoGLContext * glue = SoGLContext_instance(SoGLCacheContextElement::get(state));
  int maxunits = SoGLContext_max_texture_units(glue);
  if (unit < maxunits) {
    SoMultiTextureCoordinateElement::setFunction(action->getState(), this,
                                                 unit,
                                                 generate,
                                                 action->getState());
    SoGLMultiTextureCoordinateElement::setTexGen(action->getState(),
                                                 this, unit, handleTexgen, 
                                                 NULL,
                                                 generate,
                                                 action->getState());
  }
}

// doc from parent
void
SoTextureCoordinateEnvironment::callback(SoCallbackAction * action)
{
  SoTextureCoordinateEnvironment::doAction((SoAction *)action);
}

// doc from parent
void
SoTextureCoordinateEnvironment::pick(SoPickAction * action)
{
  SoTextureCoordinateEnvironment::doAction((SoAction *)action);
}

void
SoTextureCoordinateEnvironment::handleTexgen(void * /* data */)
{
  // Configure OpenGL hardware sphere-map texture coordinate generation
  // for the S and T coordinates.  GL_TEXTURE_GEN_MODE + GL_SPHERE_MAP is
  // the correct API: SoGLContext_glTexGenf(sogl_current_render_glue(), coord, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP).
  // Using GL_SPHERE_MAP directly as the pname (as a naive reading of some
  // older documentation suggests) is invalid and produces GL_INVALID_ENUM.
  SoGLContext_glTexGenf(sogl_current_render_glue(), GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
  SoGLContext_glTexGenf(sogl_current_render_glue(), GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);

  // supply dummy plane for R and Q so that texture generation works
  // properly
  SoGLContext_glTexGeni(sogl_current_render_glue(), GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  SoGLContext_glTexGeni(sogl_current_render_glue(), GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  
  float plane[4];
  plane[0] = 0.0f;
  plane[1] = 0.0f;
  plane[2] = 0.0f;
  plane[3] = 1.0f;
  SoGLContext_glTexGenfv(sogl_current_render_glue(), GL_R, GL_OBJECT_PLANE, plane);
  SoGLContext_glTexGenfv(sogl_current_render_glue(), GL_Q, GL_OBJECT_PLANE, plane);
}
