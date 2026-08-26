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
  \class SoContextHandler SoContextHandler.h Inventor/misc/SoContextHandler.h
  \brief The SoContextHandler class is for now to be treated as an internal class.

  \ingroup coin_general

  \since Coin 2.0
*/

// FIXME: should be documented and be part of the Doxygen API doc,
// since it's a public class (and possibly useful
// externally). 20030225 mortene.
//
// UPDATE: there also a function in SoGLCacheContextElement which
// looks like it might do the same thing: scheduleDeleteCallback()..?
// Investigate.
//
// 20040723 mortene.

//
// UPDATE: No, scheduleDeleteCallback() will not do the same thing.
// It's only used for deleting SoGLDisplayLists.
//
// 20050209 pederb
//

// *************************************************************************

#include <Inventor/misc/SoContextHandler.h>

#include <cstdlib>
#include <algorithm>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <vector>

#include <Inventor/errors/SoDebugError.h>

#include "CoinTidbits.h"
#include "glue/glp.h"

// *************************************************************************

class socontexthandler_cbitem {
public:
  socontexthandler_cbitem(SoContextHandler::ContextDestructionCB * callback,
                          void * userdata)
    : func(callback), closure(userdata) { }

  bool matches(SoContextHandler::ContextDestructionCB * callback,
               void * userdata) const {
    return this->func == callback && this->closure == userdata;
  }

  SoContextHandler::ContextDestructionCB * func;
  void * closure;
  std::mutex state_mutex;
  std::condition_variable state_changed;
  bool active = true;
  size_t running = 0;
};

using ContextCallbackPtr = std::shared_ptr<socontexthandler_cbitem>;
static std::vector<ContextCallbackPtr> * socontexthandler_callbacks;
static std::mutex socontexthandler_mutex;

struct socontexthandler_invocation {
  socontexthandler_cbitem * callback;
  socontexthandler_invocation * previous;
};
static thread_local socontexthandler_invocation *
  socontexthandler_current_invocation;

static size_t
socontexthandler_current_count(const socontexthandler_cbitem * callback)
{
  size_t count = 0;
  for (socontexthandler_invocation * invocation =
         socontexthandler_current_invocation;
       invocation; invocation = invocation->previous) {
    if (invocation->callback == callback) ++count;
  }
  return count;
}

// *************************************************************************

static void
socontexthandler_cleanup(void)
{
  std::vector<ContextCallbackPtr> callbacks;
#if OBOL_DEBUG
  size_t len;
#endif
  {
    std::lock_guard<std::mutex> lock(socontexthandler_mutex);
    if (socontexthandler_callbacks) {
      callbacks.swap(*socontexthandler_callbacks);
    }
#if OBOL_DEBUG
    len = callbacks.size();
#endif
    delete socontexthandler_callbacks;
    socontexthandler_callbacks = NULL;
    for (const ContextCallbackPtr & callback : callbacks) {
      std::lock_guard<std::mutex> statelock(callback->state_mutex);
      callback->active = false;
    }
  }
  for (const ContextCallbackPtr & callback : callbacks) {
    std::unique_lock<std::mutex> lock(callback->state_mutex);
    const size_t selfcount = socontexthandler_current_count(callback.get());
    callback->state_changed.wait(lock, [&] {
      return callback->running <= selfcount;
    });
  }
#if OBOL_DEBUG
  if (len > 0) {
    // Can't use SoDebugError here, as SoError et al might have been
    // "cleaned up" already.
    (void)printf("Coin debug: socontexthandler_cleanup(): %d context-bound "
                 "resources not free'd before exit.\n", static_cast<int>(len));
  }
#endif // OBOL_DEBUG
}

// *************************************************************************

/*!
  This method \e must be called by client code which destructs a
  context, to guarantee that there are no memory leaks upon context
  destruction.

  This will take care of correctly freeing context-bound resources,
  like OpenGL texture objects and display lists.

  Before calling this function, the context \e must be made current.

  Note that if you are using one of the standard GUI-binding libraries from
  Kongsberg Oil & Gas Technologies, this is taken care of automatically for
  contexts for canvases set up by SoQt, SoWin, etc.
*/
void
SoContextHandler::destructingContext(uint32_t contextid)
{
  std::vector<ContextCallbackPtr> callbacks;
  {
    std::lock_guard<std::mutex> lock(socontexthandler_mutex);
    if (socontexthandler_callbacks) {
      callbacks = *socontexthandler_callbacks;
    }
  }
  // process callbacks FILO-style so that callbacks registered first
  // are called last. HACK WARNING: SoGLCacheContextElement will add a
  // callback in initClass(). It's quite important that this callback
  // is called after all other callbacks (since the other callbacks
  // might schedule destruction of GL resources through the methods in
  // SoGLCacheContextElement). This criteria is met as it is now,
  // since it's the only callback added while initializing Coin
  // (SoDB::init()).

  // FIXME: We should probably add a new method in
  // SoGLCacheContextElement which this class can call after all the
  // regular callbacks though. pederb, 2004-10-27
  std::exception_ptr callbackexception;
  for (auto iter = callbacks.rbegin(); iter != callbacks.rend(); ++iter) {
    const ContextCallbackPtr & callback = *iter;
    {
      std::lock_guard<std::mutex> lock(callback->state_mutex);
      // A callback earlier in this FILO dispatch may have removed this entry.
      // Reserve only immediately before invocation; reserving the whole
      // snapshot up front would make cross-removal deadlock while waiting for
      // a later callback that the blocked dispatcher had not reached yet.
      if (!callback->active) continue;
      ++callback->running;
    }
    socontexthandler_invocation invocation = {
      callback.get(), socontexthandler_current_invocation
    };
    socontexthandler_current_invocation = &invocation;
    try {
      callback->func(contextid, callback->closure);
    }
    catch (...) {
      if (!callbackexception) callbackexception = std::current_exception();
    }
    socontexthandler_current_invocation = invocation.previous;
    {
      std::lock_guard<std::mutex> lock(callback->state_mutex);
      --callback->running;
    }
    callback->state_changed.notify_all();
  }

  // tell glglue that this context is dead
  SoGLContext_destruct(contextid);
  if (callbackexception) std::rethrow_exception(callbackexception);
}

// *************************************************************************

/*!
  Add a callback which will be called every time a GL context is
  destructed. The callback should delete all GL resources tied to that
  context.

  All nodes/classes that allocate GL resources should set up a callback
  like this. Add the callback in the constructor of the node/class,
  and remove it in the destructor.

  \sa removeContextDestructionCallback()
*/
void
SoContextHandler::addContextDestructionCallback(ContextDestructionCB * func,
                                                void * closure)
{
  std::lock_guard<std::mutex> lock(socontexthandler_mutex);
  if (socontexthandler_callbacks == NULL) {
    socontexthandler_callbacks = new std::vector<ContextCallbackPtr>;
    // make this callback trigger after the SoGLCacheContext cleanup function
    // by setting priority to -1
    coin_atexit((coin_atexit_f *)socontexthandler_cleanup, CC_ATEXIT_NORMAL_LOWPRIORITY);
  }
  const auto existing = std::find_if(
    socontexthandler_callbacks->begin(), socontexthandler_callbacks->end(),
    [func, closure](const ContextCallbackPtr & callback) {
      return callback->matches(func, closure);
    });
  if (existing == socontexthandler_callbacks->end()) {
    socontexthandler_callbacks->push_back(
      std::make_shared<socontexthandler_cbitem>(func, closure));
  }
}

/*!
  Remove a context destruction callback.

  \sa addContextDestructionCallback()
*/
void
SoContextHandler::removeContextDestructionCallback(ContextDestructionCB * func, void * closure)
{
  ContextCallbackPtr removed;
  {
    std::lock_guard<std::mutex> lock(socontexthandler_mutex);
    assert(socontexthandler_callbacks);
    const auto iter = std::find_if(
      socontexthandler_callbacks->begin(), socontexthandler_callbacks->end(),
      [func, closure](const ContextCallbackPtr & callback) {
        return callback->matches(func, closure);
      });
    assert(iter != socontexthandler_callbacks->end());
    if (iter == socontexthandler_callbacks->end()) return;
    removed = *iter;
    {
      std::lock_guard<std::mutex> statelock(removed->state_mutex);
      removed->active = false;
    }
    socontexthandler_callbacks->erase(iter);
  }

  const size_t selfcount = socontexthandler_current_count(removed.get());
  std::unique_lock<std::mutex> lock(removed->state_mutex);
  removed->state_changed.wait(lock, [&] {
    return removed->running <= selfcount;
  });
}

// *************************************************************************
