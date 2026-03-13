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
  \class SoSceneTexture2 SoSceneTexture2.h Inventor/nodes/SoSceneTexture2.h
  \brief The SoSceneTexture2 class is used to create a 2D texture from a Coin scene graph.

  \ingroup coin_nodes

  Lets the rendering of a scene graph be specified as a texture image
  to be used in another scene graph. Set up the scene graph used for a
  texture in the SoSceneTexture2::scene field.

  This node behaves exactly like SoTexture2 when it comes mapping the
  actual texture onto subsequent geometry. Please read the SoTexture2
  documentation for more information about how textures are mapped
  onto shapes.

  A notable feature of this node is that it will use offscreen
  pbuffers for hardware accelerated rendering, if they are available
  from the OpenGL driver. WGL, GLX and AGL, for OpenGL drivers on
  Microsoft Windows, X11 and Mac OS X, respectively, all support the
  OpenGL Architecture Review Board (ARB) pbuffer extension in later
  incarnations from most OpenGL vendors.

  Note also that the offscreen pbuffer will be used directly on the
  card as a texture, with no costly round trip back and forth from CPU
  memory, if the OpenGL driver supports the recent ARB_render_texture
  extension.

  An important limitation is that textures should have dimensions that
  are equal to a whole power-of-two, see documentation for
  SoSceneTexture::size.

  <b>FILE FORMAT/DEFAULTS:</b>
  \code
    SceneTexture2 {
        size 256 256
        scene NULL
        sceneTransparencyType NULL
        type RGBA_UNSIGNED_BYTE
        backgroundColor 0 0 0 0
        transparencyFunction NONE
        wrapS REPEAT
        wrapT REPEAT
        model MODULATE
        blendColor 0 0 0
    }
  \endcode

  \since Coin 2.2
*/

// *************************************************************************

/*!
  \enum SoSceneTexture2::Model

  Texture mapping model, for deciding how to "merge" the texture map
  with the object it is mapped onto.
*/
/*!
  \var SoSceneTexture2::Model SoSceneTexture2::MODULATE

  Texture color is multiplied by the polygon color. The result will
  be Phong shaded (if light model is PHONG).
*/
/*!
  \var SoSceneTexture2::Model SoSceneTexture2::DECAL

  Texture image overwrites polygon shading. Textured pixels will
  not be Phong shaded. Has undefined behaviour for grayscale and
  grayscale-alpha textures.
*/
/*!
  \var SoSceneTexture2::Model SoSceneTexture2::BLEND

  This model is normally used with monochrome textures (i.e. textures
  with one or two components). The first component, the intensity, is
  then used to blend between the shaded color of the polygon and the
  SoSceneTexture2::blendColor.
*/
/*!
  \var SoSceneTexture2::Model SoSceneTexture2::REPLACE

  Texture image overwrites polygon shading. Textured pixels will not
  be Phong shaded. Supports grayscale and grayscale alpha
  textures. This feature requires OpenGL 1.1. MODULATE will be used if
  OpenGL version < 1.1 is detected.
*/

/*!
  \enum SoSceneTexture2::Wrap

  Enumeration of wrapping strategies which can be used when the
  texture map doesn't cover the full extent of the geometry.
*/
/*!
  \var SoSceneTexture2::Wrap SoSceneTexture2::REPEAT
  Repeat texture when coordinate is not between 0 and 1.
*/
/*!
  \var SoSceneTexture2::Wrap SoSceneTexture2::CLAMP
  Clamp coordinate between 0 and 1.
*/
/*!
  \var SoSceneTexture2::Wrap SoSceneTexture2::CLAMP_TO_BORDER
  Clamp coordinate to range [1/2N, 1 - 1/2N], where N is the size of the texture in the direction of clamping.
*/

/*!
  \enum SoSceneTexture2::TransparencyFunction

  For deciding how the texture's alpha channel is handled. It's not
  possible to automatically detect this, since the texture is stored
  only on the graphics card's memory, and it'd be too slow to fetch
  the image to test the alpha channel like Coin does for regular
  textures.
*/

/*!
  \var SoSceneTexture2::Transparency SoSceneTexture2::NONE
  The alpha channel is ignored.
*/

/*!
  \var SoSceneTexture2::Transparency SoSceneTexture2::ALPHA_TEST
  An alpha test function is used.
*/

/*!
  \var SoSceneTexture2::Transparency SoSceneTexture2::ALPHA_BLEND
  Alpha blending is used.
*/

/*!
  \var SoSFEnum SoSceneTexture2::wrapS

  Wrapping strategy for the S coordinate when the texture map is
  narrower than the object to map onto.

  Default value is SoSceneTexture2::REPEAT.
*/
/*!
  \var SoSFEnum SoSceneTexture2::wrapT

  Wrapping strategy for the T coordinate when the texture map is
  shorter than the object to map onto.

  Default value is SoSceneTexture2::REPEAT.
*/
/*!
  \var SoSFEnum SoSceneTexture2::model

  Texture mapping model for how the texture map is "merged" with the
  polygon primitives it is applied to. Default value is
  SoSceneTexture2::MODULATE.
*/
/*!
  \var SoSFColor SoSceneTexture2::blendColor

  Blend color. Used when SoSceneTexture2::model is SoSceneTexture2::BLEND.

  Default color value is [0, 0, 0], black, which means no contribution
  to the blending is made.
*/

/*!
  \var SoSFVec2s SoSceneTexture2::size

  The size of the texture.

  This node currently only supports power of two textures.  If the
  size is not a power of two, the value will be rounded upwards to the
  next power of two.
*/

/*!
  \var SoSFNode SoSceneTexture2::scene

  The scene graph that is rendered into the texture.
*/

/*!
  \var SoSFNode SoSceneTexture2::sceneTransparencyType

  Used for overriding the transparency type for the sub scene graph.
  Should contain an instance of the SoTransparencyType node, or NULL to
  inherit the transparency type from the current viewer.

  Please note that if you want to render the texture using frame
  buffer objects, you need to use of of the NONE, SCREEN_DOOR, ADD or
  BLEND transparency types.

*/

/*!
  \var SoSFVec4f SoSceneTexture2::backgroundColor

  The color the color buffer is cleared to before rendering the scene.
  Default value is (0.0f, 0.0f, 0.0f, 0.0f).
*/

/*!
  \var SoSFEnum SoSceneTexture2::transparencyFunction

  The transparency function used. Default value is NONE.
*/

/*!
  \var SoSFNode SoSceneTexture2::type

  The type of texture to generate. RGBA8 for normal texture, DEPTH for
  a depth buffer texture, RGBA32F for a floating point RGBA texture.
  texture. Default is RGBA_UNSIGNED_BYTE.

*/

/*!
  \var SoSceneTexture2::Type SoSceneTexture2::RGBA8
  Specifies an RGBA texture with 8 bits per component.
*/

/*!
  \var SoSceneTexture2::Type SoSceneTexture2::DEPTH
  Specifies a depth buffer texture.
*/

/*!
  \var SoSceneTexture2::Type SoSceneTexture2::RGBA32F
  Specifies a RGBA texture with floating point components.
*/

#include <Inventor/nodes/SoSceneTexture2.h>
#include "config.h"

#include <cassert>
#include <cstring>

#include <Inventor/errors/SoDebugError.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoOneShotSensor.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/misc/SoNotification.h>

#include <Inventor/SoInput.h>
#include <Inventor/nodes/SoTransparencyType.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/elements/SoTextureQualityElement.h>
#include <Inventor/elements/SoGLShaderProgramElement.h>
#include <Inventor/elements/SoTextureOverrideElement.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/elements/SoCacheElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/elements/SoContextManagerElement.h>
#include <Inventor/elements/SoTextureUnitElement.h>
#include <Inventor/elements/SoGLMultiTextureImageElement.h>
#include <Inventor/elements/SoGLMultiTextureEnabledElement.h>
#include <Inventor/elements/SoShapeStyleElement.h>
#include <Inventor/elements/SoGLDisplayList.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoLightElement.h>
#include <Inventor/elements/SoGLLightIdElement.h>
#include <Inventor/elements/SoGLLazyElement.h>
#include <Inventor/elements/SoShapeStyleElement.h>
#include <Inventor/elements/SoTextureQualityElement.h>
#include <Inventor/errors/SoReadError.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/lists/SbStringList.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/SbImage.h>
#include "glue/glp.h"
#include <Inventor/SoDB.h>
#include <Inventor/misc/SoGLImage.h>
#include "CoinTidbits.h"
#include <Inventor/system/gl.h>
#include <Inventor/misc/SoGLDriverDatabase.h>


#ifdef OBOL_THREADSAFE
#include <Inventor/threads/SbMutex.h>
#endif // OBOL_THREADSAFE

#include "nodes/SoSubNodeP.h"
#include "elements/SoTextureScalePolicyElement.h"


// FIXME: The multicontex handling in this class is very messy. Clean
// it up.  pederb, 2010-07-12

namespace {
  struct fbo_deletedata {
    GLuint frameBuffer;
    GLuint depthBuffer;
  };

  void fbo_delete_cb(void * closure, uint32_t contextid) {
    const SoGLContext * glue = SoGLContext_instance(contextid);
    fbo_deletedata * local_fbodata = reinterpret_cast<fbo_deletedata*> (closure);
    if (local_fbodata->frameBuffer != GL_INVALID_VALUE) {
      SoGLContext_glDeleteFramebuffers(glue, 1, &local_fbodata->frameBuffer);
    }
    if (local_fbodata->depthBuffer != GL_INVALID_VALUE) {
      SoGLContext_glDeleteRenderbuffers(glue, 1, &local_fbodata->depthBuffer);
    }
    delete local_fbodata;
  }

};

// *************************************************************************

class SoSceneTexture2P {
  struct fbo_data {
    GLuint fbo_frameBuffer;
    GLuint fbo_depthBuffer;
    SbVec2s fbo_size;
    SbBool fbo_mipmap;
    SoGLDisplayList * fbo_texture;
    SoGLDisplayList * fbo_depthmap;
    int32_t cachecontext;

    fbo_data(int32_t cc) {
        this->cachecontext = cc;
        this->fbo_frameBuffer = GL_INVALID_VALUE;
        this->fbo_depthBuffer = GL_INVALID_VALUE;
        this->fbo_texture = NULL;
        this->fbo_depthmap = NULL;
        this->fbo_size.setValue(-1,-1);
        this->fbo_mipmap = FALSE;
    }
  };

public:
  SoSceneTexture2P(SoSceneTexture2 * api);
  ~SoSceneTexture2P();

  // FIXME: move the updateBuffer and SoGLImage handling into a new
  // class called CoinOffscreenTexture (or something similar).
  // pederb, 2007-03-07

  SoSceneTexture2 * api;
  SoDB::ContextManager * contextManager;
  void * glcontext;
  SbVec2s glcontextsize;
  int contextid;

  fbo_data * fbodata;
  SoGLImage * glimage;
  int32_t glimagecontext;
  
  SbBool buffervalid;

  SbBool glimagevalid;
  SbBool glrectangle;

  void updateBuffer(SoState * state, const float quality);
  void updateFrameBuffer(SoState * state, const float quality);
  void updatePBuffer(SoState * state, const float quality);
  SoGLRenderAction * glaction;
  static void prerendercb(void * userdata, SoGLRenderAction * action);

  SbBool createFramebufferObjects(const SoGLContext * glue, SoState * state,
                                  const SoSceneTexture2::Type type,
                                  const SbBool warn);
  void deleteFrameBufferObjects(const SoGLContext * glue, SoState * state);
  SbBool checkFramebufferStatus(const SoGLContext * glue, const SbBool warn);

  SoGLRenderAction::TransparencyType getTransparencyType(SoState * state);
  SbBool shouldCreateMipmap(SoState * state) {
    float q = SoTextureQualityElement::get(state);
    return q > 0.5f;
  }

#ifdef OBOL_THREADSAFE
  SbMutex mutex;
#endif // OBOL_THREADSAFE
  SbBool canrendertotexture;
  unsigned char * offscreenbuffer;
  int offscreenbuffersize;
};

// *************************************************************************

#define PRIVATE(obj) obj->pimpl


#ifdef OBOL_THREADSAFE
#define LOCK_GLIMAGE(_thisp_) (PRIVATE(_thisp_)->mutex.lock())
#define UNLOCK_GLIMAGE(_thisp_) (PRIVATE(_thisp_)->mutex.unlock())
#else // OBOL_THREADSAFE
#define LOCK_GLIMAGE(_thisp_)
#define UNLOCK_GLIMAGE(_thisp_)
#endif // OBOL_THREADSAFE

SO_NODE_SOURCE(SoSceneTexture2);

// Documented in superclass.
/*!
  \copybrief SoBase::initClass(void)
*/
void
SoSceneTexture2::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoSceneTexture2, SO_FROM_OBOL_2_2);

  SO_ENABLE(SoGLRenderAction, SoGLMultiTextureImageElement);
  SO_ENABLE(SoGLRenderAction, SoGLMultiTextureEnabledElement);

  SO_ENABLE(SoCallbackAction, SoMultiTextureImageElement);
  SO_ENABLE(SoCallbackAction, SoMultiTextureEnabledElement);

  SO_ENABLE(SoRayPickAction, SoMultiTextureImageElement);
  SO_ENABLE(SoRayPickAction, SoMultiTextureEnabledElement);
}

static SoGLImage::Wrap
translateWrap(const SoSceneTexture2::Wrap wrap)
{
  if (wrap == SoSceneTexture2::CLAMP_TO_BORDER) return SoGLImage::CLAMP_TO_BORDER;
  if (wrap == SoSceneTexture2::REPEAT) return SoGLImage::REPEAT;
  return SoGLImage::CLAMP;
}

SoSceneTexture2::SoSceneTexture2(void)
{
  this->pimpl = new SoSceneTexture2P(this);

  SO_NODE_INTERNAL_CONSTRUCTOR(SoSceneTexture2);
  SO_NODE_ADD_FIELD(size, (256, 256));
  SO_NODE_ADD_FIELD(scene, (NULL));
  SO_NODE_ADD_FIELD(sceneTransparencyType, (NULL));
  SO_NODE_ADD_FIELD(backgroundColor, (0.0f, 0.0f, 0.0f, 0.0f));
  SO_NODE_ADD_FIELD(transparencyFunction, (NONE));

  SO_NODE_ADD_FIELD(wrapS, (REPEAT));
  SO_NODE_ADD_FIELD(wrapT, (REPEAT));
  SO_NODE_ADD_FIELD(model, (MODULATE));
  SO_NODE_ADD_FIELD(blendColor, (0.0f, 0.0f, 0.0f));
  SO_NODE_ADD_FIELD(type, (RGBA8));

  SO_NODE_DEFINE_ENUM_VALUE(Model, MODULATE);
  SO_NODE_DEFINE_ENUM_VALUE(Model, DECAL);
  SO_NODE_DEFINE_ENUM_VALUE(Model, BLEND);
  SO_NODE_DEFINE_ENUM_VALUE(Model, REPLACE);

  SO_NODE_DEFINE_ENUM_VALUE(Wrap, REPEAT);
  SO_NODE_DEFINE_ENUM_VALUE(Wrap, CLAMP);
  SO_NODE_DEFINE_ENUM_VALUE(Wrap, CLAMP_TO_BORDER);

  SO_NODE_DEFINE_ENUM_VALUE(TransparencyFunction, NONE);
  SO_NODE_DEFINE_ENUM_VALUE(TransparencyFunction, ALPHA_BLEND);
  SO_NODE_DEFINE_ENUM_VALUE(TransparencyFunction, ALPHA_TEST);

  SO_NODE_SET_SF_ENUM_TYPE(wrapS, Wrap);
  SO_NODE_SET_SF_ENUM_TYPE(wrapT, Wrap);
  SO_NODE_SET_SF_ENUM_TYPE(model, Model);
  SO_NODE_SET_SF_ENUM_TYPE(transparencyFunction, TransparencyFunction);

  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA8);
  SO_NODE_DEFINE_ENUM_VALUE(Type, DEPTH);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA32F);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB32F);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA16F);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB16F);

  SO_NODE_DEFINE_ENUM_VALUE(Type, R3_G3_B2);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB4);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB5);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB8);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB10);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB12);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB16);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA2);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA4);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB5_A1);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGB10_A2);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA12);
  SO_NODE_DEFINE_ENUM_VALUE(Type, RGBA16);

  SO_NODE_SET_SF_ENUM_TYPE(type, Type);
}

SoSceneTexture2::~SoSceneTexture2(void)
{
  delete PRIVATE(this);
}


// Documented in superclass.
void
SoSceneTexture2::GLRender(SoGLRenderAction * action)
{
  SoState * state = action->getState();
  if (SoTextureOverrideElement::getImageOverride(state))
    return;

  float quality = SoTextureQualityElement::get(state);

  int32_t cachecontext = SoGLCacheContextElement::get(state);
  const SoGLContext * glue = SoGLContext_instance(cachecontext);
  SoNode * root = this->scene.getValue();

  // this is not a very good solution, but just recreate the
  // pbuffer/framebuffer if the context has changed since it was
  // created.
  if (cachecontext != PRIVATE(this)->glimagecontext) {
    PRIVATE(this)->glimagevalid = FALSE;
    PRIVATE(this)->buffervalid = FALSE;
    // this will force the pbuffers and/or framebuffers to be recreated
    PRIVATE(this)->glcontextsize = SbVec2s(-1, -1);
    if (PRIVATE(this)->fbodata) {
      PRIVATE(this)->fbodata->fbo_size = SbVec2s(-1, -1);
    }
  }
  LOCK_GLIMAGE(this);

  if (root && (!PRIVATE(this)->buffervalid || !PRIVATE(this)->glimagevalid)) {
    PRIVATE(this)->updateBuffer(state, quality);

    // don't cache when we change the glimage
    SoCacheElement::setInvalid(TRUE);
    if (state->isCacheOpen()) {
      SoCacheElement::invalidate(state);
    }
  }
  UNLOCK_GLIMAGE(this);

  SoMultiTextureImageElement::Model glmodel = (SoMultiTextureImageElement::Model)
    this->model.getValue();

  if (glmodel == SoMultiTextureImageElement::REPLACE) {
    if (!SoGLContext_glversion_matches_at_least(glue, 1, 1, 0)) {
      static int didwarn = 0;
      if (!didwarn) {
        SoDebugError::postWarning("SoSceneTexture2::GLRender",
                                  "Unable to use the GL_REPLACE texture model. "
                                  "Your OpenGL version is < 1.1. "
                                  "Using GL_MODULATE instead.");
        didwarn = 1;
      }
      // use MODULATE and not DECAL, since DECAL only works for RGB
      // and RGBA textures
      glmodel = SoMultiTextureImageElement::MODULATE;
    }
  }

  int unit = SoTextureUnitElement::get(state);
  int maxunits = SoGLContext_max_texture_units(glue);
  if (unit < maxunits) {
    SoGLMultiTextureImageElement::set(state, this, unit,
                                      PRIVATE(this)->glimage,
                                      glmodel,
                                      this->blendColor.getValue());

    SoGLMultiTextureEnabledElement::set(state, this, unit,
                                        PRIVATE(this)->glimage != NULL &&
                                        quality > 0.0f);
  }
  else {
    // we already warned in SoTextureUnit. I think it's best to just
    // ignore the texture here so that all texture for non-supported
    // units will be ignored. pederb, 2003-11-04
  }
}


// Documented in superclass.
void
SoSceneTexture2::doAction(SoAction * OBOL_UNUSED_ARG(action))
{
}

// Documented in superclass.
void
SoSceneTexture2::callback(SoCallbackAction * action)
{
  SoSceneTexture2::doAction(action);
}

// Documented in superclass.
void
SoSceneTexture2::rayPick(SoRayPickAction * action)
{
  SoSceneTexture2::doAction(action);
}

// Documented in superclass.
void
SoSceneTexture2::notify(SoNotList * list)
{
  SoField * f = list->getLastField();
  if (f == &this->scene ||
      f == &this->size ||
      f == &this->type) {
    // rerender scene
    PRIVATE(this)->buffervalid = FALSE;
  }
  else if (f == &this->wrapS ||
           f == &this->wrapT ||
           f == &this->model ||
           f == &this->transparencyFunction ||
           f == &this->sceneTransparencyType) {
    // no need to render scene again, but update the texture object
    PRIVATE(this)->glimagevalid = FALSE;
  }
  inherited::notify(list);
}

// Documented in superclass.
void
SoSceneTexture2::write(SoWriteAction * action)
{
  inherited::write(action);
}


#undef PRIVATE

// *************************************************************************

#define PUBLIC(obj) obj->api

SoSceneTexture2P::SoSceneTexture2P(SoSceneTexture2 * apiptr)
{
  this->api = apiptr;
  this->contextManager = NULL;
  this->glcontext = NULL;
  this->buffervalid = FALSE;
  this->glimagevalid = FALSE;
  this->glimage = NULL;
  this->glimagecontext = 0;
  this->glaction = NULL;
  this->glcontextsize.setValue(-1,-1);
  this->glrectangle = FALSE;
  this->offscreenbuffer = NULL;
  this->offscreenbuffersize = 0;
  this->canrendertotexture = FALSE;
  this->contextid = -1;
  this->fbodata = NULL;
}

SoSceneTexture2P::~SoSceneTexture2P()
{
  this->deleteFrameBufferObjects(NULL, NULL);
  delete this->fbodata;

  if (this->glimage) this->glimage->unref(NULL);
  if (this->glcontext != NULL) {
    if (this->contextManager) this->contextManager->destroyContext(this->glcontext);
  }
  delete[] this->offscreenbuffer;
  if (this->glaction) {
    coingl_unregister_context_manager(this->contextid);
    coingl_unregister_osmesa_context(this->contextid);
  }
  delete this->glaction;
}

void
SoSceneTexture2P::updateBuffer(SoState * state, const float quality)
{
  // Update the context manager from the state element pushed by SoOffscreenRenderer.
  // This ensures we use the correct backend without calling SoDB::getContextManager().
  SoDB::ContextManager * stateMgr = SoContextManagerElement::get(state);
  if (stateMgr) {
    if (stateMgr != this->contextManager) {
      // The context manager has changed (e.g. the same SoSceneTexture2 node is
      // rendered by a different backend such as system-GL then OSMesa).  Any
      // inner context that was created by the OLD manager must be destroyed
      // through that same manager before we adopt the new one.  Mixing context
      // pointers across managers causes type confusion and crashes.
      if (this->glcontext && this->contextManager) {
        this->contextManager->destroyContext(this->glcontext);
        this->glcontext = NULL;
        this->glcontextsize.setValue(-1, -1);
      }
      if (this->glimage) {
        this->glimage->unref(NULL);
        this->glimage = NULL;
        this->glimagecontext = 0;
      }
      if (this->glaction) {
        coingl_unregister_context_manager(this->contextid);
        coingl_unregister_osmesa_context(this->contextid);
        delete this->glaction;
        this->glaction = NULL;
      }
      this->glimagevalid = FALSE;
      this->buffervalid = FALSE;
    }
    this->contextManager = stateMgr;
  }

  // make sure we've finished rendering to this context
  const SoGLContext * glue = SoGLContext_instance(SoGLCacheContextElement::get(state));
  SoGLContext_glFlush(glue);

  SbBool candofbo = SoGLDriverDatabase::isSupported(glue, SO_GL_FRAMEBUFFER_OBJECT);
  if (candofbo) {
    // can't render to a FBO if we have a delayed transparency type
    // involving path traversal in a second pass.
    switch (this->getTransparencyType(state)) {
    case SoGLRenderAction::NONE:
    case SoGLRenderAction::BLEND:
    case SoGLRenderAction::ADD:
    case SoGLRenderAction::SCREEN_DOOR:
      break;
    default:
      candofbo = FALSE;
      break;
    }
  }

  if (!candofbo) {
    this->updatePBuffer(state, quality);
  }
  else {
    this->updateFrameBuffer(state, quality);
  }
}

void
SoSceneTexture2P::updateFrameBuffer(SoState * state, const float OBOL_UNUSED_ARG(quality))
{
  int i;
  SbVec2s size = PUBLIC(this)->size.getValue();
  SoNode * scene = PUBLIC(this)->scene.getValue();
  assert(scene);

  int cachecontext = SoGLCacheContextElement::get(state);
  const SoGLContext * glue = SoGLContext_instance(cachecontext);
  SbBool mipmap = this->shouldCreateMipmap(state);

  fbo_data * local_fbodata = this->fbodata;
  if (!local_fbodata) {
    this->fbodata = local_fbodata = new fbo_data(0);
  }

  if ((local_fbodata->fbo_size != size) || (mipmap != local_fbodata->fbo_mipmap) || 
      (cachecontext != local_fbodata->cachecontext)) {
    local_fbodata->fbo_mipmap = mipmap;
    local_fbodata->fbo_size = size;
    local_fbodata->cachecontext = cachecontext;
    
    if (this->glimage) {
      this->glimage->unref(NULL);
      this->glimage = NULL;
      this->glimagecontext = 0;
    }
    if (this->glimage == NULL) {
      this->glimage = new SoGLImage;
      this->glimagecontext = SoGLCacheContextElement::get(state);
      uint32_t flags = this->glimage->getFlags();
      switch ((SoSceneTexture2::TransparencyFunction) (PUBLIC(this)->transparencyFunction.getValue())) {
      case SoSceneTexture2::NONE:
        flags |= SoGLImage::FORCE_TRANSPARENCY_FALSE|SoGLImage::FORCE_ALPHA_TEST_FALSE;
        break;
      case SoSceneTexture2::ALPHA_TEST:
        flags |= SoGLImage::FORCE_TRANSPARENCY_TRUE|SoGLImage::FORCE_ALPHA_TEST_TRUE;
        break;
      case SoSceneTexture2::ALPHA_BLEND:
        flags |= SoGLImage::FORCE_TRANSPARENCY_TRUE|SoGLImage::FORCE_ALPHA_TEST_FALSE;
        break;
      default:
        assert(0 && "should not get here");
        break;
      }
      this->glimage->setFlags(flags);
    }

    SbBool finished = FALSE;

    SoSceneTexture2::Type type = (SoSceneTexture2::Type) PUBLIC(this)->type.getValue();
    while (!finished) {
      this->deleteFrameBufferObjects(glue, state);
      finished = TRUE;
      SbBool warn = (type == SoSceneTexture2::RGBA32F || type == SoSceneTexture2::RGBA16F) ? FALSE : TRUE;

      if (!this->createFramebufferObjects(glue, state, type, warn)) {
        if (type == SoSceneTexture2::RGBA32F) { // Fall back to 16-bit floating point
          type = SoSceneTexture2::RGBA16F;
          finished = FALSE;
        }
        else if (type == SoSceneTexture2::RGBA16F) { // Fall back to 8-bit RGBA (e.g. OSMesa without float textures)
          type = SoSceneTexture2::RGBA8;
          finished = FALSE;
        }
      }
    }

    if (local_fbodata->fbo_frameBuffer == GL_INVALID_VALUE) {
      // FBO creation failed for all attempted formats; skip shadow-map rendering.
      return;
    }

  }

  // Refresh the glimage's GL display list association on every frame.
  // The DL can be removed from the glimage's internal list by context-cleanup
  // callbacks (e.g. when a sibling GL context is destroyed) even while the FBO
  // itself is still valid.  Without this call, SoGLImage::getGLDisplayList()
  // falls through to createGLDisplayList() which — for an FBO-backed image
  // with no pixel data — creates a new texture object that is never populated,
  // corrupting the GL driver's heap metadata on the next allocation.
  if (this->glimage && local_fbodata->fbo_frameBuffer != GL_INVALID_VALUE) {
    if (PUBLIC(this)->type.getValue() == SoSceneTexture2::DEPTH) {
      assert(local_fbodata->fbo_depthmap != NULL);
      this->glimage->setGLDisplayList(local_fbodata->fbo_depthmap, state,
                                      SoGLImage::CLAMP, SoGLImage::CLAMP);
    }
    else {
      assert(local_fbodata->fbo_texture != NULL);
      this->glimage->setGLDisplayList(local_fbodata->fbo_texture, state);
    }
  }

  state->push();

  // reset OpenGL/Coin state
  SoGLShaderProgramElement::enable(state, FALSE);
  SoLazyElement::setToDefault(state);
  SoShapeStyleElement::setTransparencyType(state, (int32_t) this->getTransparencyType(state));
  SoLazyElement::setTransparencyType(state, (int32_t) this->getTransparencyType(state));

  // disable all active textures
  SoMultiTextureEnabledElement::disableAll(state);

  // just disable all active light source
  int numlights = SoLightElement::getLights(state).getLength();
  for (i = 0; i < numlights; i++) {
    SoGLContext_glDisable(glue, (GLenum) (GL_LIGHT0 + i));
  }
  float oldclearcolor[4];
  SoGLContext_glGetFloatv(glue, GL_COLOR_CLEAR_VALUE, oldclearcolor);

  SoModelMatrixElement::set(state, PUBLIC(this), SbMatrix::identity());

  // store current framebuffer
  GLint oldfb;
  SoGLContext_glGetIntegerv(glue, GL_FRAMEBUFFER_BINDING_EXT, &oldfb);

  // set up framebuffer for rendering
  SoGLContext_glBindFramebuffer(glue, GL_FRAMEBUFFER_EXT, local_fbodata->fbo_frameBuffer);
  this->checkFramebufferStatus(glue, TRUE);

  SoViewportRegionElement::set(state, SbViewportRegion(local_fbodata->fbo_size));
  SbVec4f col = PUBLIC(this)->backgroundColor.getValue();
  SoGLContext_glClearColor(glue, col[0], col[1], col[2], col[3]);
  SoGLContext_glClear(glue, GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);

  SoGLRenderAction * local_glaction = (SoGLRenderAction*) state->getAction();
  // traverse the new scene graph

  // clear the abort callback before traversing the internal scene
  // graph. This will make the FBO version behave like the pbuffer
  // version, and avoid problems with the offscreen renderer which
  // uses the abort callback to adjust the original viewport.

  SoGLRenderAction::SoGLRenderAbortCB * old_func;
  void * old_userdata;
  local_glaction->getAbortCallback(old_func, old_userdata);
  local_glaction->setAbortCallback(NULL, NULL);
  local_glaction->switchToNodeTraversal(scene);
  local_glaction->setAbortCallback(old_func, old_userdata);

  // make sure rendering has completed before switching back to the previous context
  SoGLContext_glFlush(glue);

  if (PUBLIC(this)->type.getValue() == SoSceneTexture2::DEPTH) {
    // need to copy the depth buffer into the depth texture object
    SoGLContext_glBindTexture(glue,GL_TEXTURE_2D, local_fbodata->fbo_depthmap->getFirstIndex());
    SoGLContext_glCopyTexSubImage2D(glue, GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                        local_fbodata->fbo_size[0], local_fbodata->fbo_size[1]);
    SoGLContext_glBindTexture(glue, GL_TEXTURE_2D, 0);
  }
  else {
    SoGLContext_glBindTexture(glue,GL_TEXTURE_2D, local_fbodata->fbo_texture->getFirstIndex());
    if (local_fbodata->fbo_mipmap) {
      SoGLContext_glGenerateMipmap(glue, GL_TEXTURE_2D);
    }
    SoGLContext_glBindTexture(glue,GL_TEXTURE_2D, 0);
  }

  SoGLContext_glBindFramebuffer(glue, GL_FRAMEBUFFER_EXT, (GLuint)oldfb);
  this->checkFramebufferStatus(glue, TRUE);


  // restore old clear color
  SoGLContext_glClearColor(glue, oldclearcolor[0], oldclearcolor[1], oldclearcolor[2], oldclearcolor[3]);

  // enable lights again
  for (i = 0; i < numlights; i++) {
    SoGLContext_glEnable(glue, (GLenum) (GL_LIGHT0 + i));
  }
  state->pop();

  SoGLLazyElement::getInstance(state)->reset(state,
                                             SoLazyElement::LIGHT_MODEL_MASK|
                                             SoLazyElement::TWOSIDE_MASK|
                                             SoLazyElement::SHADE_MODEL_MASK);

  this->buffervalid = TRUE;
  this->glimagevalid = TRUE;
}

void
SoSceneTexture2P::updatePBuffer(SoState * state, const float quality)
{
  SbVec2s size = PUBLIC(this)->size.getValue();

  SoNode * scene = PUBLIC(this)->scene.getValue();
  assert(scene);

  if ((this->glcontext && this->glcontextsize != size) || (size == SbVec2s(0,0))) {
    if (this->glimage) {
      this->glimage->unref(state);
      this->glimage = NULL;
      this->glimagecontext = 0;
    }
    if (this->glcontext) {
      if (this->contextManager) this->contextManager->destroyContext(this->glcontext);
      this->glcontextsize.setValue(-1,-1);
      this->glcontext = NULL;
    }
    // Note: Recreating the glaction (below) will also get us a new contextid.
    if (this->glaction) {
      coingl_unregister_context_manager(this->contextid);
      coingl_unregister_osmesa_context(this->contextid);
    }
    delete this->glaction;
    this->glaction = NULL;
    this->glimagevalid = FALSE;
  }
  if (size == SbVec2s(0,0)) return;

  // FIXME: temporary until non power of two textures are supported,
  // pederb 2003-12-05
  size[0] = (short) coin_geq_power_of_two(size[0]);
  size[1] = (short) coin_geq_power_of_two(size[1]);

  if (this->glcontext == NULL) {
    this->glcontextsize = size;
     // disabled until an pbuffer extension is available to create a
    // render-to-texture pbuffer that has a non power of two size.
    // pederb, 2003-12-05
    if (1) {
      this->glcontextsize[0] = (short) coin_geq_power_of_two(size[0]);
      this->glcontextsize[1] = (short) coin_geq_power_of_two(size[1]);

      if (this->glcontextsize != size) {
        static int didwarn = 0;
        if (!didwarn) {
          SoDebugError::postWarning("SoSceneTexture2P::updatePBuffer",
                                    "Requested non power of two size, but your OpenGL "
                                    "driver lacks support for such pbuffer textures.");
          didwarn = 1;
        }
      }
    }
    this->glrectangle = FALSE;
    if (!coin_is_power_of_two(this->glcontextsize[0]) ||
        !coin_is_power_of_two(this->glcontextsize[1])) {
      // we only get here if the OpenGL driver can handle non power of
      // two textures/pbuffers.
      this->glrectangle = TRUE;
    }
    // FIXME: make it possible to specify what kind of context you want
    // (RGB or RGBA, I guess). pederb, 2003-11-27
    this->glcontext = this->contextManager ? this->contextManager->createOffscreenContext(this->glcontextsize[0], this->glcontextsize[1]) : nullptr;
    this->canrendertotexture = SoGLContext_context_can_render_to_texture(this->glcontext);

    if (!this->glaction) {
      this->contextid = (int) SoGLCacheContextElement::getUniqueCacheContext();
      this->glaction = new SoGLRenderAction(SbViewportRegion(this->glcontextsize));
      this->glaction->addPreRenderCallback(SoSceneTexture2P::prerendercb,
                                           (void*) PUBLIC(this));
      /* Register the inner context ID so SoGLContext_instance() can resolve
       * GL capabilities without needing a current system-GL context. */
      if (this->contextManager) {
        coingl_register_context_manager(this->contextid, this->contextManager);
        /* In dual-GL builds the dispatch layer needs to know which backend
         * this context ID belongs to so SoGLContext_instance() routes to the
         * correct GL dispatch (OSMesa vs system GL). */
        if (this->contextManager->isOSMesaContext(this->glcontext))
          coingl_register_osmesa_context(this->contextid);
      }
    } else {
      this->glaction->setViewportRegion(SbViewportRegion(this->glcontextsize));
    }

    this->glaction->setTransparencyType(this->getTransparencyType(state));
    this->glaction->setCacheContext(this->contextid);
    this->glimagevalid = FALSE;
  }

  /* If context creation failed there is nothing useful we can render. */
  if (!this->glcontext) return;

  if (!this->buffervalid) {
    assert(this->glaction != NULL);
    assert(this->glcontext != NULL);
    this->glaction->setTransparencyType((SoGLRenderAction::TransparencyType)
                                        SoShapeStyleElement::getTransparencyType(state));

    if (this->contextManager) this->contextManager->makeContextCurrent(this->glcontext);
    const SoGLContext * pbglue = SoGLContext_instance(this->contextid);

    // Some context managers (e.g. FLTKContextManager) provide a window-backed
    // context that may be physically much smaller than the requested texture
    // dimensions (e.g. a hidden 1×1 window used solely to own a GL context).
    // Rendering the sub-scene directly to that tiny window framebuffer produces
    // wrong pixel data and causes glReadPixels to overrun the valid framebuffer
    // region, corrupting the heap.  When the inner context supports FBOs,
    // create a temporary FBO of the exact texture size so that both rendering
    // and the subsequent readback target the correct surface.
    GLuint tmpFbo = 0, tmpColorTex = 0, tmpDepthRbo = 0;
    GLint savedFbo = 0;
    SbBool hasTmpFbo = FALSE;
    if (!this->canrendertotexture &&
        SoGLDriverDatabase::isSupported(pbglue, SO_GL_FRAMEBUFFER_OBJECT)) {
      SoGLContext_glGetIntegerv(pbglue, GL_FRAMEBUFFER_BINDING_EXT, &savedFbo);

      // Colour attachment: plain RGBA8 texture (no mipmaps needed).
      SoGLContext_glGenTextures(pbglue, 1, &tmpColorTex);
      SoGLContext_glBindTexture(pbglue, GL_TEXTURE_2D, tmpColorTex);
      SoGLContext_glTexImage2D(pbglue, GL_TEXTURE_2D, 0, GL_RGBA8,
                               this->glcontextsize[0], this->glcontextsize[1],
                               0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      SoGLContext_glTexParameteri(pbglue, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      SoGLContext_glTexParameteri(pbglue, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      SoGLContext_glBindTexture(pbglue, GL_TEXTURE_2D, 0);

      // Depth attachment: 24-bit renderbuffer.
      SoGLContext_glGenRenderbuffers(pbglue, 1, &tmpDepthRbo);
      SoGLContext_glBindRenderbuffer(pbglue, GL_RENDERBUFFER_EXT, tmpDepthRbo);
      SoGLContext_glRenderbufferStorage(pbglue, GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24,
                                        this->glcontextsize[0], this->glcontextsize[1]);
      SoGLContext_glBindRenderbuffer(pbglue, GL_RENDERBUFFER_EXT, 0);

      // Assemble and validate the FBO.
      SoGLContext_glGenFramebuffers(pbglue, 1, &tmpFbo);
      SoGLContext_glBindFramebuffer(pbglue, GL_FRAMEBUFFER_EXT, tmpFbo);
      SoGLContext_glFramebufferTexture2D(pbglue, GL_FRAMEBUFFER_EXT,
                                         GL_COLOR_ATTACHMENT0_EXT,
                                         GL_TEXTURE_2D, tmpColorTex, 0);
      SoGLContext_glFramebufferRenderbuffer(pbglue, GL_FRAMEBUFFER_EXT,
                                            GL_DEPTH_ATTACHMENT_EXT,
                                            GL_RENDERBUFFER_EXT, tmpDepthRbo);

      if (SoGLContext_glCheckFramebufferStatus(pbglue, GL_FRAMEBUFFER_EXT) ==
          GL_FRAMEBUFFER_COMPLETE_EXT) {
        hasTmpFbo = TRUE;
      } else {
        // FBO setup failed; fall back to window framebuffer rendering.
        // Only restore a non-zero saved FBO explicitly; binding FBO 0 on
        // drivers backed by a window surface (e.g. radeonsi on AMD) can
        // crash _mesa_bind_framebuffers when the window is not fully exposed.
        if (savedFbo != 0)
          SoGLContext_glBindFramebuffer(pbglue, GL_FRAMEBUFFER_EXT, (GLuint)savedFbo);
        SoGLContext_glDeleteFramebuffers(pbglue, 1, &tmpFbo);
        SoGLContext_glDeleteRenderbuffers(pbglue, 1, &tmpDepthRbo);
        SoGLContext_glDeleteTextures(pbglue, 1, &tmpColorTex);
        tmpFbo = tmpColorTex = tmpDepthRbo = 0;
      }
    }

    SoGLContext_glEnable(pbglue, GL_DEPTH_TEST);
    this->glaction->apply(scene);
    // Make sure that rendering to pBuffer is completed to avoid
    // flickering. DON'T REMOVE THIS. You have been warned.
    SoGLContext_glFlush(pbglue);

    if (!this->canrendertotexture) {
      // Ensure the temp FBO is still bound before glReadPixels: apply() may
      // have changed the framebuffer binding internally (e.g. via nested
      // SoSceneTexture2 nodes).
      if (hasTmpFbo)
        SoGLContext_glBindFramebuffer(pbglue, GL_FRAMEBUFFER_EXT, tmpFbo);
      SbVec2s ctx_size = this->glcontextsize;
      int reqbytes = ctx_size[0]*ctx_size[1]*4;
      if (reqbytes > this->offscreenbuffersize) {
        delete[] this->offscreenbuffer;
        // Allocate with extra padding: proprietary GPU drivers (NVIDIA,
        // radeonsi) use vectorised/DMA stores during glReadPixels that
        // overrun the exact pixel-data size.  Valgrind on NVIDIA 535
        // confirms writes at reqbytes+256 and reqbytes+272, meaning the
        // driver overshoots the 256-byte zone entirely.  4096 bytes (one
        // page) gives a safe margin for any conceivable DMA burst size
        // without wasting meaningful memory.
        this->offscreenbuffer = new unsigned char[reqbytes + 4096];
        this->offscreenbuffersize = reqbytes;
      }
      SoGLContext_glPixelStorei(pbglue, GL_PACK_ALIGNMENT, 1);
      SoGLContext_glReadPixels(pbglue, 0, 0, ctx_size[0], ctx_size[1], GL_RGBA, GL_UNSIGNED_BYTE,
                   this->offscreenbuffer);
      SoGLContext_glPixelStorei(pbglue, GL_PACK_ALIGNMENT, 4);
    }

    // Clean up temporary FBO now that pixel readback is complete.
    if (hasTmpFbo) {
      // Per the OpenGL spec, deleting a currently-bound FBO automatically
      // reverts the binding to 0, so an explicit glBindFramebuffer(0) is
      // not needed when savedFbo == 0.  Avoid calling it in that case:
      // on some drivers (e.g. radeonsi on AMD) glBindFramebuffer(0) crashes
      // inside _mesa_bind_framebuffers when the context is backed by a
      // window surface that was never fully exposed.
      if (savedFbo != 0)
        SoGLContext_glBindFramebuffer(pbglue, GL_FRAMEBUFFER_EXT, (GLuint)savedFbo);
      SoGLContext_glDeleteFramebuffers(pbglue, 1, &tmpFbo);
      SoGLContext_glDeleteRenderbuffers(pbglue, 1, &tmpDepthRbo);
      SoGLContext_glDeleteTextures(pbglue, 1, &tmpColorTex);
    }

    if (this->contextManager) this->contextManager->restorePreviousContext(this->glcontext);
  }
  if (!this->glimagevalid || (this->glimage == NULL)) {
    // just delete old glimage
    if (this->glimage) {
      this->glimage->unref(state);
      this->glimage = NULL;
    }
    this->glimage = new SoGLImage;
    this->glimagecontext = SoGLCacheContextElement::get(state);
    uint32_t flags = this->glimage->getFlags();
    if (this->glrectangle) {
      flags |= SoGLImage::RECTANGLE;
    }
    switch ((SoSceneTexture2::TransparencyFunction) (PUBLIC(this)->transparencyFunction.getValue())) {
    case SoSceneTexture2::NONE:
      flags |= SoGLImage::FORCE_TRANSPARENCY_FALSE|SoGLImage::FORCE_ALPHA_TEST_FALSE;
      break;
    case SoSceneTexture2::ALPHA_TEST:
      flags |= SoGLImage::FORCE_TRANSPARENCY_TRUE|SoGLImage::FORCE_ALPHA_TEST_TRUE;
      break;
    case SoSceneTexture2::ALPHA_BLEND:
      flags |= SoGLImage::FORCE_TRANSPARENCY_TRUE|SoGLImage::FORCE_ALPHA_TEST_FALSE;
      break;
    default:
      assert(0 && "should not get here");
      break;
    }
    if (this->canrendertotexture) {
      // bind texture to pbuffer
      this->glimage->setPBuffer(state, this->glcontext,
                                translateWrap((SoSceneTexture2::Wrap)PUBLIC(this)->wrapS.getValue()),
                                translateWrap((SoSceneTexture2::Wrap)PUBLIC(this)->wrapT.getValue()),
                                quality);
    }
    this->glimage->setFlags(flags);
  }
  if (!this->canrendertotexture) {
    assert(this->glimage);
    assert(this->offscreenbuffer);
    this->glimage->setData(this->offscreenbuffer,
                           this->glcontextsize,
                           4,
                           translateWrap((SoSceneTexture2::Wrap)PUBLIC(this)->wrapS.getValue()),
                           translateWrap((SoSceneTexture2::Wrap)PUBLIC(this)->wrapT.getValue()),
                           quality);
  }
  this->glimagevalid = TRUE;
  this->buffervalid = TRUE;
}

void
SoSceneTexture2P::prerendercb(void * userdata, SoGLRenderAction * action)
{
  SoSceneTexture2 * thisp = (SoSceneTexture2*) userdata;
  SbVec4f col = thisp->backgroundColor.getValue();
  const SoGLContext * glue = SoGLContext_instance(
      SoGLCacheContextElement::get(action->getState()));
  SoGLContext_glClearColor(glue, col[0], col[1], col[2], col[3]);
  SoGLContext_glClear(glue, GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
}

static void soscenetexture2_translate_type(SoSceneTexture2::Type type, GLenum & internalformat, GLenum & format)
{
  format = GL_RGBA;
  internalformat = GL_RGBA8;

  switch (type) {
  case SoSceneTexture2::DEPTH:
  case SoSceneTexture2::RGBA8:
    internalformat = GL_RGBA8;
    break;
  case SoSceneTexture2::RGBA32F:
    internalformat = GL_RGBA32F_ARB;
    break;
  case SoSceneTexture2::RGB32F:
    internalformat = GL_RGB32F_ARB;
    break;
  case SoSceneTexture2::RGBA16F:
    internalformat = GL_RGBA16F_ARB;
    break;
  case SoSceneTexture2::RGB16F:
    internalformat = GL_RGB16F_ARB;
    break;
  case SoSceneTexture2::R3_G3_B2:
    internalformat = GL_R3_G3_B2;
    break;
  case SoSceneTexture2::RGB:
    internalformat = GL_RGB;
    break;
  case SoSceneTexture2::RGB4:
    internalformat = GL_RGB4;
    break;
  case SoSceneTexture2::RGB5:
    internalformat = GL_RGB5;
    break;
  case SoSceneTexture2::RGB8:
    internalformat = GL_RGB8;
    break;
  case SoSceneTexture2::RGB10:
    internalformat = GL_RGB10;
    break;
  case SoSceneTexture2::RGB12:
    internalformat = GL_RGB12;
    break;
  case SoSceneTexture2::RGB16:
    internalformat = GL_RGB16;
    break;
  case SoSceneTexture2::RGBA:
    internalformat = GL_RGBA;
    break;
  case SoSceneTexture2::RGBA2:
    internalformat = GL_RGBA2;
    break;
  case SoSceneTexture2::RGBA4:
    internalformat = GL_RGBA4;
    break;
  case SoSceneTexture2::RGB5_A1:
    internalformat = GL_RGB5_A1;
    break;
  case SoSceneTexture2::RGB10_A2:
    internalformat = GL_RGB10_A2;
    break;
  case SoSceneTexture2::RGBA12:
    internalformat = GL_RGBA12;
    break;
  case SoSceneTexture2::RGBA16:
    internalformat = GL_RGBA16;
    break;
  default:
    assert(0 && "unknown type");
    break;
  }
}

SbBool
SoSceneTexture2P::createFramebufferObjects(const SoGLContext * glue, SoState * state,
                                           const SoSceneTexture2::Type type,
                                           const SbBool warn)
{
  fbo_data * local_fbodata = this->fbodata;
  assert(local_fbodata);  

  assert(local_fbodata->fbo_texture == NULL);
  assert(local_fbodata->fbo_depthmap == NULL);
  assert(local_fbodata->fbo_frameBuffer == GL_INVALID_VALUE);
  assert(local_fbodata->fbo_depthBuffer == GL_INVALID_VALUE);

  // store old framebuffer
  GLint oldfb;
  SoGLContext_glGetIntegerv(glue, GL_FRAMEBUFFER_BINDING_EXT, &oldfb);

  SoGLContext_glGenFramebuffers(glue, 1, &local_fbodata->fbo_frameBuffer);
  SoGLContext_glGenRenderbuffers(glue, 1, &local_fbodata->fbo_depthBuffer);
  SoGLContext_glBindFramebuffer(glue, GL_FRAMEBUFFER_EXT, local_fbodata->fbo_frameBuffer);

  local_fbodata->fbo_texture = new SoGLDisplayList(state, SoGLDisplayList::TEXTURE_OBJECT);
  local_fbodata->fbo_texture->ref();
  local_fbodata->fbo_texture->open(state);

  GLenum gltype = GL_FLOAT;
  GLenum internalformat = GL_RGBA8;
  GLenum format = GL_RGBA;

  soscenetexture2_translate_type(type, internalformat, format);

  switch (PUBLIC(this)->type.getValue()) {
  case SoSceneTexture2::RGBA8:
  case SoSceneTexture2::DEPTH:
    gltype = GL_UNSIGNED_BYTE;
    break;
  default:
    break;
  }

  SoGLContext_glTexImage2D(glue, GL_TEXTURE_2D, 0,
               internalformat,
               local_fbodata->fbo_size[0], local_fbodata->fbo_size[1],
               0, /* border */
               format,
               gltype, NULL);

  // for mipmaps
  // FIXME: add support for CLAMP_TO_BORDER in SoSceneTexture2 and SoTextureImageElement

  GLenum wraps = (GLenum) PUBLIC(this)->wrapS.getValue();
  GLenum wrapt = (GLenum) PUBLIC(this)->wrapT.getValue();

  SbBool clamptoborder_ok =
    SoGLDriverDatabase::isSupported(glue, "GL_ARB_texture_border_clamp") ||
    SoGLDriverDatabase::isSupported(glue, "GL_SGIS_texture_border_clamp");

  if (wraps == GL_CLAMP_TO_BORDER && !clamptoborder_ok) wraps = GL_CLAMP;
  if (wrapt == GL_CLAMP_TO_BORDER && !clamptoborder_ok) wrapt = GL_CLAMP;

  SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wraps);
  SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapt);

  SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, local_fbodata->fbo_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (local_fbodata->fbo_mipmap) {
    SoGLContext_glGenerateMipmap(glue, GL_TEXTURE_2D);
  }

  if (SoGLDriverDatabase::isSupported(glue, SO_GL_ANISOTROPIC_FILTERING)) {
    SoGLContext_glTexParameterf(glue, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    SoGLContext_get_max_anisotropy(glue));
  }

  local_fbodata->fbo_texture->close(state);

  if (type == SoSceneTexture2::DEPTH) {
    local_fbodata->fbo_depthmap = new SoGLDisplayList(state, SoGLDisplayList::TEXTURE_OBJECT);
    local_fbodata->fbo_depthmap->ref();
    local_fbodata->fbo_depthmap->open(state);

    SoGLContext_glTexImage2D(glue, GL_TEXTURE_2D, 0,
                 GL_DEPTH_COMPONENT, /* GL_DEPTH_COMPONENT24? */
                 local_fbodata->fbo_size[0], local_fbodata->fbo_size[1],
                 0, /* border */
                 GL_DEPTH_COMPONENT,
                 GL_UNSIGNED_BYTE, NULL);

    if (SoGLDriverDatabase::isSupported(glue, "GL_ARB_texture_border_clamp") ||
        SoGLDriverDatabase::isSupported(glue, "GL_SGIS_texture_border_clamp")) {
      SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }
    else {
      SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
      SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    }
    SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
    SoGLContext_glTexParameteri(glue, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    if (SoGLDriverDatabase::isSupported(glue, SO_GL_ANISOTROPIC_FILTERING)) {
      SoGLContext_glTexParameterf(glue, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                      SoGLContext_get_max_anisotropy(glue));
    }

    local_fbodata->fbo_depthmap->close(state);
  }

  if (local_fbodata->fbo_texture != NULL) {
    // attach texture to framebuffer color object
    SoGLContext_glFramebufferTexture2D(glue,
                                     GL_FRAMEBUFFER_EXT,
                                     GL_COLOR_ATTACHMENT0_EXT,
                                     GL_TEXTURE_2D,
                                     (GLuint) local_fbodata->fbo_texture->getFirstIndex(),
                                     0);
  }

  // create the render buffer
  SoGLContext_glBindRenderbuffer(glue, GL_RENDERBUFFER_EXT, local_fbodata->fbo_depthBuffer);
  SoGLContext_glRenderbufferStorage(glue, GL_RENDERBUFFER_EXT,
                                  GL_DEPTH_COMPONENT24,
                                  local_fbodata->fbo_size[0], local_fbodata->fbo_size[1]);
  // attach renderbuffer to framebuffer
  SoGLContext_glFramebufferRenderbuffer(glue,
                                      GL_FRAMEBUFFER_EXT,
                                      GL_DEPTH_ATTACHMENT_EXT,
                                      GL_RENDERBUFFER_EXT,
                                      local_fbodata->fbo_depthBuffer);

  SbBool ret = this->checkFramebufferStatus(glue, warn);
  SoGLContext_glBindFramebuffer(glue, GL_FRAMEBUFFER_EXT, (GLint)oldfb);

  return ret;
}

void
SoSceneTexture2P::deleteFrameBufferObjects(const SoGLContext * glue, SoState * state)
{
  fbo_data * local_fbodata = this->fbodata;
  if (!local_fbodata) return; // might happen if the scene texture isn't traversed
  
  if (local_fbodata->fbo_texture) {
    local_fbodata->fbo_texture->unref(state);
    local_fbodata->fbo_texture = NULL;
  }
  if (local_fbodata->fbo_depthmap) {
    local_fbodata->fbo_depthmap->unref(state);
    local_fbodata->fbo_depthmap = NULL;
  }
  if (glue && state && SoGLCacheContextElement::get(state) == local_fbodata->cachecontext) {
    if (local_fbodata->fbo_frameBuffer != GL_INVALID_VALUE) {
      SoGLContext_glDeleteFramebuffers(glue, 1, &local_fbodata->fbo_frameBuffer);
      local_fbodata->fbo_frameBuffer = GL_INVALID_VALUE;
    }
    if (local_fbodata->fbo_depthBuffer != GL_INVALID_VALUE) {
      SoGLContext_glDeleteRenderbuffers(glue, 1, &local_fbodata->fbo_depthBuffer);
      local_fbodata->fbo_depthBuffer = GL_INVALID_VALUE;
    }
  }
  else {
    fbo_deletedata * dd = new fbo_deletedata;
    dd->frameBuffer = local_fbodata->fbo_frameBuffer;
    dd->depthBuffer = local_fbodata->fbo_depthBuffer;
    SoGLCacheContextElement::scheduleDeleteCallback(local_fbodata->cachecontext,
                                                    fbo_delete_cb, dd);
  }
  local_fbodata->fbo_frameBuffer = GL_INVALID_VALUE;
  local_fbodata->fbo_depthBuffer = GL_INVALID_VALUE;
}

SbBool
SoSceneTexture2P::checkFramebufferStatus(const SoGLContext * glue, const SbBool warn)
{
  // check if the buffers have been successfully set up
  GLenum status = SoGLContext_glCheckFramebufferStatus(glue, GL_FRAMEBUFFER_EXT);
  SbString error("");
  switch (status){
  case GL_FRAMEBUFFER_COMPLETE_EXT:
    break;
  case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
    error = "GL_FRAMEBUFFER_UNSUPPORTED_EXT\n";
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
    error = "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT\n";
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
    error = "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT\n";
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
    error = "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT\n";
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
    error = "GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT\n";
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
    error = "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT\n";
    break;
  case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
    error = "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT\n";
    break;
  default: break;
  }
  if (error != "") {
    if (warn) {
      SoDebugError::post("SoSceneTexture2P::createFramebufferObjects",
                         "GL Framebuffer error: %s", error.getString());
    }
    return FALSE;
  }
  return TRUE;
}

SoGLRenderAction::TransparencyType
SoSceneTexture2P::getTransparencyType(SoState * state)
{
  SoNode * node = PUBLIC(this)->sceneTransparencyType.getValue();
  if (node && node->isOfType(SoTransparencyType::getClassTypeId())) {
    return (SoGLRenderAction::TransparencyType)
      ((SoTransparencyType*)node)->value.getValue();
  }
  return (SoGLRenderAction::TransparencyType)
    SoShapeStyleElement::getTransparencyType(state);
}


#undef PUBLIC

#undef LOCK_GLIMAGE
#undef UNLOCK_GLIMAGE

// **************************************************************
