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
  \class SoGLCacheContextElement Inventor/elements/SoGLCacheContextElement.h
  \brief The SoGLCacheContextElement class handles the OpenGL cache for a context.

  \ingroup coin_elements
*/

// *************************************************************************

#include "config.h"
#include <Inventor/elements/SoGLCacheContextElement.h>

#include <cstdlib>
#include <cstring>

#include <Inventor/SbName.h>
#include <Inventor/elements/SoGLDisplayList.h>
#include <Inventor/lists/SbList.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/system/gl.h>
#include <Inventor/misc/SoContextHandler.h>
#include <Inventor/misc/SoGLDriverDatabase.h>

#include "rendering/SoGL.h"
#include "CoinTidbits.h"

// *************************************************************************

static int biggest_cache_context_id = 0;

// *************************************************************************

SO_ELEMENT_SOURCE(SoGLCacheContextElement);

// *************************************************************************

typedef struct {
  SbName extname;
  SbList <int> context;
  SbList <SbBool> supported;
} so_glext_info;

typedef struct {
  int context;
  int handle;
} so_gltexhandle_info;

typedef struct {
  uint32_t contextid;
  SoScheduleDeleteCB * cb;
  void * closure;
} so_scheduledeletecb_info;

static SbList <so_glext_info *> * extsupportlist;
static SbList <SoGLDisplayList*> * scheduledeletelist;
static SbList <so_scheduledeletecb_info*> * scheduledeletecblist;

// needed to be able to remove the callback in the cleanup function
// (SoGLCacheContextElement::cleanupContext() is private)
static SoContextHandler::ContextDestructionCB * soglcache_contextdestructioncb;

static void soglcachecontext_cleanup(void)
{
  int i,n = extsupportlist->getLength();
  for (i = 0; i < n; i++) {
    delete (*extsupportlist)[i];
  }
  n = scheduledeletecblist->getLength();
  for (i = 0; i < n; i++) {
    // just delete callbacks, don't call them
    delete (*scheduledeletecblist)[i];
  }

  delete extsupportlist;
  delete scheduledeletelist;
  delete scheduledeletecblist;

  if (soglcache_contextdestructioncb) {
    SoContextHandler::removeContextDestructionCallback(soglcache_contextdestructioncb, NULL);
    soglcache_contextdestructioncb = NULL;
  }

  biggest_cache_context_id = 0;
}

//
// Used both as a callback from SoContextHandler and called directly
// from inside this class every time ::set() is called.
//
void
SoGLCacheContextElement::cleanupContext(uint32_t contextid, void * OBOL_UNUSED_ARG(userdata))
{
  int context = (int) contextid;

  int i = 0;
  int n = scheduledeletelist->getLength();

  while (i < n) {
    SoGLDisplayList * dl = (*scheduledeletelist)[i];
    if (dl->getContext() == context) {
      scheduledeletelist->removeFast(i);
      n--;
      delete dl;
    }
    else i++;
  }

  i = 0;
  n = scheduledeletecblist->getLength();
  while (i < n) {
    so_scheduledeletecb_info * info = (*scheduledeletecblist)[i];
    if (info->contextid == contextid) {
      info->cb(info->closure, info->contextid);
      scheduledeletecblist->removeFast(i);
      n--;
      delete info;
    }
    else i++;
  }

}

// *************************************************************************

/*!
  \copydetails SoElement::initClass(void)
*/

void
SoGLCacheContextElement::initClass(void)
{
  SO_ELEMENT_INIT_CLASS(SoGLCacheContextElement, inherited);

  extsupportlist = new SbList <so_glext_info *>;
  scheduledeletelist = new SbList <SoGLDisplayList*>;
  scheduledeletecblist = new SbList <so_scheduledeletecb_info*>;
  coin_atexit((coin_atexit_f *)soglcachecontext_cleanup, CC_ATEXIT_NORMAL);

  // add a callback which is called every time a GL-context is
  // destructed.  it's important that this callback is added in
  // initClass() to make it work properly when destructing a
  // context. See comments in SoContextHandler.cpp for more
  // information.
  SoContextHandler::addContextDestructionCallback(cleanupContext, NULL);
  soglcache_contextdestructioncb = cleanupContext;
}

/*!
  Destructor.
*/
SoGLCacheContextElement::~SoGLCacheContextElement()
{
}

// doc from parent
void
SoGLCacheContextElement::init(SoState * OBOL_UNUSED_ARG(state))
{
  // these values will be set up in set(), but initialize them anyway
  this->context = 0;
  this->twopass = FALSE;
  this->rendering = RENDERING_UNSET;
  this->autocachebits = 0;
  this->numshapes = 0;
  this->numseparators = 0;
}

// doc from parent
SbBool
SoGLCacheContextElement::matches(const SoElement * elt) const
{
  const SoGLCacheContextElement * elem = static_cast<const SoGLCacheContextElement*>(elt);

  return
    this->context == elem->context &&
    this->twopass == elem->twopass &&
    this->rendering == elem->rendering;
}

// doc from parent
SoElement *
SoGLCacheContextElement::copyMatchInfo(void) const
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement*)
    this->getTypeId().createInstance();
  elem->context = this->context;
  elem->twopass = this->twopass;
  elem->rendering = this->rendering;
  return elem;
}

/*!
  Sets data for context.
*/
void
SoGLCacheContextElement::set(SoState * state, int context,
                             SbBool twopasstransparency,
                             SbBool remoterendering)
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement *)
    state->getElementNoPush(classStackIndex);
  elem->twopass = twopasstransparency;
  elem->rendering = remoterendering ? RENDERING_SET_INDIRECT : RENDERING_SET_DIRECT;
  elem->autocachebits = 0;
  elem->context = context;
  if (context > biggest_cache_context_id) {
    biggest_cache_context_id = context;
  }

  if (remoterendering) elem->autocachebits = DO_AUTO_CACHE;

  // really delete GL resources scheduled for destruction
  SoGLCacheContextElement::cleanupContext((uint32_t) context, NULL);
}

/*!
  Returns context id.

  Note that the signature on this function is slightly wrong: the
  function should really return an \c uint32_t, like
  SoGLRenderAction::getCacheContext() does. It is kept like this for
  compatibility reasons.

  The value returned will always be a positive integer.
*/
int
SoGLCacheContextElement::get(const SoState * state)
{
  const SoGLCacheContextElement * elem = static_cast<const SoGLCacheContextElement *>(
    state->getConstElement(classStackIndex));
  return elem->context;
}

/*!
  Returns an extension id based on the GL extension string.
  The extension id can be used to quickly test for the availability
  of an extension later, using extSupported().
*/
int
SoGLCacheContextElement::getExtID(const char * str)
{
  SbName extname(str);
  int i, n = extsupportlist->getLength();
  for (i = 0; i < n; i++) {
    if ((*extsupportlist)[i]->extname == extname) break;
  }
  if (i == n) { // not found
    so_glext_info * info = new so_glext_info;
    info->extname = extname;
    extsupportlist->append(info);
  }

  return i;
}

/*!
  Returns TRUE if the extension is supported for the current context.
  \a extid must be an id returned from getExtId(). The test result
  is cached so this method is pretty fast and can be used at runtime.
*/
SbBool
SoGLCacheContextElement::extSupported(SoState * state, int extid)
{
  assert(extid >= 0 && extid < extsupportlist->getLength());

  so_glext_info * info = (*extsupportlist)[extid];

  int currcontext = SoGLCacheContextElement::get(state);
  int n = info->context.getLength();
  for (int i = 0; i < n; i++) {
    if (info->context[i] == currcontext) {
      return info->supported[i];
    }
  }
  const SoGLContext * w = sogl_glue_instance(state);
  SbBool supported = SoGLDriverDatabase::isSupported(w, info->extname.getString());
  info->context.append(currcontext);
  info->supported.append(supported);

  return supported;
}

/*!
  Returns the OpenGL version for the current context. This method is
  an extension versus the Open Inventor API.
*/
void
SoGLCacheContextElement::getOpenGLVersion(SoState * state,
                                          int & major, int & minor)
{
  int currcontext = SoGLCacheContextElement::get(state);
  const SoGLContext * w = SoGLContext_instance(currcontext);
  unsigned int majoru, minoru, dummy;
  SoGLContext_glversion(w, &majoru, &minoru, &dummy);
  major = (int)majoru;
  minor = (int)minoru;
}

/*!
  Returns if mipmapped textures are fast for the current context.
  In Coin, we just return TRUE for the moment.
*/
SbBool
SoGLCacheContextElement::areMipMapsFast(SoState * OBOL_UNUSED_ARG(state))
{
  return TRUE; // FIXME: how do we test this? pederb 20001003
}

/*!
  Update auto cache bits.
*/
void
SoGLCacheContextElement::shouldAutoCache(SoState * state, int bits)
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement*)
    state->getElementNoPush(classStackIndex);
  elem->autocachebits |= bits;
}

/*!
  Increment the number of shapes in a open cache.

  \since Coin 3.0
 */
void
SoGLCacheContextElement::incNumShapes(SoState * state)
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement*)
    state->getElementNoPush(classStackIndex);

  elem->numshapes++;
}

/*!
  Returns the number of shapes in an open cache.

  \since Coin 3.0
*/
int
SoGLCacheContextElement::getNumShapes(SoState * state)
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement*)
    state->getElementNoPush(classStackIndex);

  return elem->numshapes;
}

/*!
  Increment the number of separators in an open cache.

  \since Coin 3.0
 */
void
SoGLCacheContextElement::incNumSeparators(SoState * state)
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement*)
    state->getElementNoPush(classStackIndex);

  elem->numseparators++;
}

/*!
  Returns the number of separators in an open cache.

  \since Coin 3.0
*/
int
SoGLCacheContextElement::getNumSeparators(SoState * state)
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement*)
    state->getElementNoPush(classStackIndex);

  return elem->numseparators;
}

/*!
  Sets the auto cache bits.
*/
void
SoGLCacheContextElement::setAutoCacheBits(SoState * state, int bits)
{
  SoGLCacheContextElement * elem = (SoGLCacheContextElement*)
    state->getElementNoPush(classStackIndex);

  elem->autocachebits = bits;
}

// Private function which "unwinds" the real value of the "rendering"
// variable.
SbBool
SoGLCacheContextElement::isDirectRendering(SoState * state) const
{
  SbBool isdirect;
  if (this->rendering == RENDERING_UNSET) {
    const SoGLContext * w = sogl_glue_instance(state);
    isdirect = SoGLContext_isdirect(w);
  }
  else {
    isdirect = this->rendering == RENDERING_SET_DIRECT;
  }
  return isdirect;
}

/*!
  Not properly supported yet.
*/
int
SoGLCacheContextElement::resetAutoCacheBits(SoState * state)
{
  SoGLCacheContextElement *elem = (SoGLCacheContextElement *)
    state->getElementNoPush(classStackIndex);
  int ret = elem->autocachebits;

  elem->autocachebits = elem->isDirectRendering(state) ? 0 : DO_AUTO_CACHE;
  elem->numshapes = 0;
  elem->numseparators = 0;

  return ret;
}

/*!
  Returns \c TRUE if rendering is indirect / remote.
*/
SbBool
SoGLCacheContextElement::getIsRemoteRendering(SoState * state)
{
  const SoGLCacheContextElement *elem = (const SoGLCacheContextElement *)
    state->getConstElement(classStackIndex);
  return !elem->isDirectRendering(state);
}

//
// internal method used by SoGLDisplayList to delete list as soon as
// the display list context is current again.
//
void
SoGLCacheContextElement::scheduleDelete(SoState * state, class SoGLDisplayList * dl)
{
  if (state && dl->getContext() == SoGLCacheContextElement::get(state)) {
    delete dl;
  }
  else {
    scheduledeletelist->append(dl);
  }
}

/*!
  Can be used to receive a callback the next time Coin knows that the
  context (specified by \a contextid) is the current OpenGL context.

  This function can be used to free OpenGL resources for a context.

  Note that the callback will be invoked only once, and then removed
  from the internal list of scheduled callbacks.

  \since Coin 2.3
*/
void
SoGLCacheContextElement::scheduleDeleteCallback(const uint32_t contextid,
                                                SoScheduleDeleteCB * cb,
                                                void * closure)
{
  so_scheduledeletecb_info * info = new so_scheduledeletecb_info;
  info->contextid = contextid;
  info->cb = cb;
  info->closure = closure;

  scheduledeletecblist->append(info);
}

/*!
  Returns an unique cache context id, in the range [1, ->.

  If you render the same scene graph into two or different cache
  contexts, and you've not using display list and texture object
  sharing among contexts, the cache context id need to be unique for
  rendering to work.

  \OBOL_FUNCTION_EXTENSION

  \sa SoGLRenderAction::setCacheContext()
*/
uint32_t
SoGLCacheContextElement::getUniqueCacheContext(void)
{
  uint32_t id = ++biggest_cache_context_id;
  return id;
}
