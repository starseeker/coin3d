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
  \class SoGLDisplayList Inventor/elements/SoGLDisplayList.h
  \brief The SoGLDisplayList class stores and manages OpenGL display lists.

  \ingroup coin_elements

  The TEXTURE_OBJECT type is not directly supported in Coin. We handle
  textures differently in a more flexible class called SoGLImage,
  which also stores some information about the texture used when
  rendering. Old code which use this element should not stop
  working though. The texture object extension will just not be used,
  and the texture will be stored in a display list instead.
*/

// *************************************************************************

#include <Inventor/elements/SoGLDisplayList.h>

#include <cstring>
#include <cassert>

#include "glue/glp.h"
#include <Inventor/caches/SoGLRenderCache.h>
#include <Inventor/elements/SoCacheElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/errors/SoDebugError.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/misc/SoGLDriverDatabase.h>

#include "glue/glp.h"
#include "rendering/SoGL.h"
#include "config.h"

#ifndef OBOL_WORKAROUND_NO_USING_STD_FUNCS
using std::strcmp;
#endif // !OBOL_WORKAROUND_NO_USING_STD_FUNCS

class SoGLDisplayListP {
 public:
  SoGLDisplayList::Type type;
  int numalloc;
  unsigned int firstindex;
  int context;
  int refcount;
  int openindex;
  SbBool mipmap;
  GLenum texturetarget;
};

#define PRIVATE(obj) obj->pimpl

// *************************************************************************

/*!
  Constructor.
*/
SoGLDisplayList::SoGLDisplayList(SoState * state, Type type, int allocnum,
                                 SbBool mipmaptexobj)
{
  PRIVATE(this) = new SoGLDisplayListP;
  PRIVATE(this)->type = type;
  PRIVATE(this)->numalloc = allocnum;
  PRIVATE(this)->context = SoGLCacheContextElement::get(state);
  PRIVATE(this)->refcount = 0;
  PRIVATE(this)->mipmap = mipmaptexobj;
  PRIVATE(this)->texturetarget = 0;

#if OBOL_DEBUG && 0 // debug
  SoDebugError::postInfo("SoGLDisplayList::SoGLDisplayList", "%p", this);
#endif // debug

  // Reserve displaylist IDs.

  if (PRIVATE(this)->type == TEXTURE_OBJECT) {
    assert(allocnum == 1 && "it is only possible to create one texture object at a time");
    const SoGLContext * glw = SoGLContext_instance(PRIVATE(this)->context);
    if (SoGLDriverDatabase::isSupported(glw, SO_GL_TEXTURE_OBJECT)) {
      // use temporary variable, in case GLuint is typedef'ed to
      // something other than unsigned int
      GLuint tmpindex;
      SoGLContext_glGenTextures(glw, 1, &tmpindex);
      PRIVATE(this)->firstindex = (unsigned int )tmpindex;
    }
    else { // Fall back to display list, allocation happens further down below.
      PRIVATE(this)->type = DISPLAY_LIST;
    }
  }

  if (PRIVATE(this)->type == DISPLAY_LIST) {
    const SoGLContext * glw = SoGLContext_instance(PRIVATE(this)->context);
    PRIVATE(this)->firstindex = (unsigned int) SoGLContext_glGenLists(glw, allocnum);
    if (PRIVATE(this)->firstindex == 0) {
      SoDebugError::post("SoGLDisplayList::SoGLDisplayList",
                         "Could not reserve %d displaylist%s. "
                         "Expect flawed rendering.",
                         allocnum, allocnum==1 ? "" : "s");
      // FIXME: be more robust in handling this -- the rendering will
      // gradually go bonkers after we hit this problem. 20020619 mortene.
    }
#if OBOL_DEBUG && 0 // debug
    SoDebugError::postInfo("SoGLDisplayList::SoGLDisplayList",
                           "firstindex==%d", PRIVATE(this)->firstindex);
#endif // debug
  }
}

// private destructor. Use ref()/unref()
SoGLDisplayList::~SoGLDisplayList()
{
#if OBOL_DEBUG && 0 // debug
  SoDebugError::postInfo("SoGLDisplayList::~SoGLDisplayList", "%p", this);
#endif // debug

  if (PRIVATE(this)->type == DISPLAY_LIST) {
    SoGLContext_glDeleteLists(SoGLContext_instance(PRIVATE(this)->context), (GLuint) PRIVATE(this)->firstindex, PRIVATE(this)->numalloc);
  }
  else {
    // Should be a TEXTURE_OBJECT — clean it up if the GL context still has
    // texture-object support.  A stripped-down context (e.g. one supplied by
    // a legacy application) might have revoked or never had this capability;
    // skip silently in that case rather than crashing inside the destructor.
    const SoGLContext * glw = SoGLContext_instance(PRIVATE(this)->context);
    if (glw && SoGLContext_has_texture_objects(glw)) {
      GLuint tmpindex = (GLuint) PRIVATE(this)->firstindex;
      SoGLContext_glDeleteTextures(glw, 1, &tmpindex);
    }
  }
  delete PRIVATE(this);
}

/*!
  Increase reference count for this display list/texture object.
*/
void
SoGLDisplayList::ref(void)
{
  PRIVATE(this)->refcount++;
}

/*!
  Decrease reference count for this instance. When reference count
  reaches 0, the instance is deleted.
*/
void
SoGLDisplayList::unref(SoState * state)
{
  assert(PRIVATE(this)->refcount > 0);
  if (--PRIVATE(this)->refcount == 0) {
    // Let SoGLCacheContext delete this instance the next time context is current.
    SoGLCacheContextElement::scheduleDelete(state, this);
  }
}

/*!
  Open this display list/texture object.
*/
void
SoGLDisplayList::open(SoState * state, int index)
{
  if (PRIVATE(this)->type == DISPLAY_LIST) {
    PRIVATE(this)->openindex = index;
    // using GL_COMPILE here instead of GL_COMPILE_AND_EXECUTE will
    // lead to much higher performance on nVidia cards, and doesn't
    // hurt performance for other vendors.
    SoGLContext_glNewList(SoGLContext_instance(PRIVATE(this)->context), (GLuint) (PRIVATE(this)->firstindex+PRIVATE(this)->openindex), GL_COMPILE);
  }
  else {
    assert(PRIVATE(this)->type == TEXTURE_OBJECT);
    assert(index == 0);
    this->bindTexture(state);
  }
}

/*!
  Close this display list/texture object.
*/
void
SoGLDisplayList::close(SoState * OBOL_UNUSED_ARG(state))
{
  if (PRIVATE(this)->type == DISPLAY_LIST) {
    SoGLContext_glEndList(SoGLContext_instance(PRIVATE(this)->context));
    GLenum err = sogl_glerror_debugging() ? SoGLContext_glGetError(SoGLContext_instance(PRIVATE(this)->context)) : GL_NO_ERROR;
    if (err == GL_OUT_OF_MEMORY) {
      SoDebugError::post("SoGLDisplayList::close",
                         "Not enough memory resources available on system "
                         "to store full display list. Expect flaws in "
                         "rendering.");
    }
    SoGLContext_glCallList(SoGLContext_instance(PRIVATE(this)->context), (GLuint) (PRIVATE(this)->firstindex + PRIVATE(this)->openindex));
  }
  else {
    const SoGLContext * glw = SoGLContext_instance(PRIVATE(this)->context);
    if (glw && SoGLContext_has_texture_objects(glw)) {
      GLenum target = PRIVATE(this)->texturetarget;
      if (target == 0) {
        target = GL_TEXTURE_2D;
      }
      SoGLContext_glBindTexture(glw, target, (GLuint) 0);
    }
  }
}

/*!
  Execute this display list/texture object.
*/
void
SoGLDisplayList::call(SoState * state, int index)
{
  if (PRIVATE(this)->type == DISPLAY_LIST) {
    SoGLContext_glCallList(SoGLContext_instance(PRIVATE(this)->context), (GLuint) (PRIVATE(this)->firstindex + index));
  }
  else {
    assert(PRIVATE(this)->type == TEXTURE_OBJECT);
    assert(index == 0);
    this->bindTexture(state);
  }
  this->addDependency(state);
}

/*!
  Create a dependency on the display list.
*/
void
SoGLDisplayList::addDependency(SoState * state)
{
  if (state->isCacheOpen()) {
    SoGLRenderCache * cache = (SoGLRenderCache*)
      SoCacheElement::getCurrentCache(state);
    if (cache) cache->addNestedCache(this);
  }
}

/*!
  Returns whether the texture object stored in this instance
  was created with mipmap data. This method is an extension
  versus the Open Inventor API.
*/
SbBool
SoGLDisplayList::isMipMapTextureObject(void) const
{
  return PRIVATE(this)->mipmap;
}

/*!
  Return type. Display list or texture object.
*/
SoGLDisplayList::Type
SoGLDisplayList::getType(void) const
{
  return PRIVATE(this)->type;
}

/*!
  Return number of display lists/texture objects allocated.
*/
int
SoGLDisplayList::getNumAllocated(void) const
{
  return PRIVATE(this)->numalloc;
}

/*!
  Return first GL index for this display list.
*/
unsigned int
SoGLDisplayList::getFirstIndex(void) const
{
  return PRIVATE(this)->firstindex;
}

/*!
  Return an id for the current context.
*/
int
SoGLDisplayList::getContext(void) const
{
  return PRIVATE(this)->context;
}

/*!
  Sets the texture object target
  \since Coin 2.5
*/
void
SoGLDisplayList::setTextureTarget(int target)
{
  PRIVATE(this)->texturetarget = (GLenum) target;
}

/*!
  Returns the texture target
  \since Coin 2.5
*/
int
SoGLDisplayList::getTextureTarget(void) const
{
  if (PRIVATE(this)->texturetarget)
    return (int) PRIVATE(this)->texturetarget;
  return GL_TEXTURE_2D;
}

/*!
  \COININTERNAL

  \OBOL_FUNCTION_EXTENSION

  \since Coin 2.0
*/
void
SoGLDisplayList::bindTexture(SoState * OBOL_UNUSED_ARG(state))
{
  const SoGLContext * glw = SoGLContext_instance(PRIVATE(this)->context);
  if (!glw || !SoGLContext_has_texture_objects(glw)) {
    SoDebugError::postWarning("SoGLDisplayList::bindTexture",
      "Cannot bind texture object: texture objects are not supported by "
      "this GL context (requires OpenGL >= 1.1 or GL_EXT_texture_object). "
      "Texture rendering will be skipped.");
    return;
  }

  GLenum target = PRIVATE(this)->texturetarget;
  if (target == 0) {
    target = GL_TEXTURE_2D;
  }
  SoGLContext_glBindTexture(glw, target, (GLuint)PRIVATE(this)->firstindex);
}

#undef PRIVATE
