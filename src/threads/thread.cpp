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
  \enum cc_retval {
    CC_ERROR = 0,
    CC_OK = 1,
    CC_TIMEOUT,
    CC_BUSY
  }
  \ingroup coin_threads
  \brief The enumerator for return values of thread related functions.
*/

/*!
  \typedef enum cc_retval cc_retval
  \ingroup coin_threads
  \brief The type definition for the return value enumerator.
*/

#include "threads/threads.h"

#include <atomic>
#include <cstdlib>

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <thread>
#include <chrono>

#include "threads/mutexp.h"
#include "threads/recmutexp.h"

/* ********************************************************************** */

void
cc_sleep(float seconds)
{
  // Use C++17 std::this_thread::sleep_for for portable, precise sleep
  auto duration = std::chrono::duration<float>(seconds);
  std::this_thread::sleep_for(duration);
};

cc_thread_id_t
cc_thread_id(void)
{
  // Storage and recursive mutex ownership require an identity, not a hash.
  // Zero remains reserved for the no-threads storage key. If the counter ever
  // exhausts the platform's identifier space, stop before an ID can be reused.
  static std::atomic<cc_thread_id_t> next_id{1};
  thread_local const cc_thread_id_t id = [] {
    const cc_thread_id_t allocated =
      next_id.fetch_add(1, std::memory_order_relaxed);
    if (allocated == 0) std::abort();
    return allocated;
  }();
  return id;
}


void
cc_thread_init(void)
{
  cc_mutex_init();
  cc_recmutex_init();
}

/* ********************************************************************** */

/*!
  \page coin_multithreading_support Multithreading Support in Coin

  The support in Coin for using multiple threads in application
  programs and the Coin library itself, consists of two main features:

  <ul>

  <li>
  Coin provides platform independent thread handling abstraction
  classes. These are classes that the application programmer can
  freely use in her application code to start new threads, control
  their execution, work with mutexes and do other tasks related to
  handling multiple threads.

  The classes in question are SbThread, SbMutex, SbStorage, SbBarrier,
  SbCondVar, SbFifo, SbThreadAutoLock, SbRWMutex, and
  SbTypedStorage. See their respective documentation for the detailed
  information.

  The classes fully hides the system specific implementation, which is
  either done on top of native Win32 (if on Microsoft Windows), or
  over POSIX threads (on UNIX and UNIX-like systems).
  </li>

  <li>
  The other aspect of our multi-threading support is that Coin can be
  specially configured so that rendering traversals of the scene graph
  are done in a thread-safe manner. This means e.g. that it is
  possible to have Coin render the scene in parallel on multiple CPUs
  for multiple rendering pipes, to better take advantage of such
  high-end systems (like CAVE environments, for instance).

  Thread-safe render traversals are \e off by default, because there
  is a small overhead involved which would make rendering (very)
  slightly slower on single-threaded invocations.

  To get a Coin library built with thread-safe rendering, one must
  actively reconfigure Coin and build a special, local version. For
  configure-based builds (UNIX and UNIX-like systems, or with Cygwin
  on Microsoft Windows) this is done with the option
  "--enable-threadsafe" to Autoconf configure. To change the
  configuration and rebuild with Visual Studio, you will need to
  change the preprocessor directive OBOL_THREADSAFE to defined in the
  file src/setup.h located in the same folder as you found your
  solution file.</li>

  </ul>

  There are some restrictions and other issues which it is important
  to be aware of:
  
  <ul>

  <li> We do not yet provide any support for binding the
  multi-threaded rendering support into the SoQt / SoWin / etc GUI
  bindings, and neither do we provide bindings against any specific
  library that handles multi-pipe rendering. This means the
  application programmer will have to possess some expertise, and put
  in some effort, to be able to utilize multi-pipe rendering with
  Coin. </li>

  <li> Rendering traversals are currently the only operation which we
  publicly support to be thread-safe. There are other aspects of Coin
  that we know are thread-safe, like most other action traversals
  beside just rendering, but we make no guarantees in this
  regard. </li>

  <li> Be careful about using a separate thread for changing Coin
  structures versus what is used for the applications GUI event
  thread.

  We are aware of at least issues with Qt3 (and thereby SoQt), where
  you should not modify the scene graph in any way in a thread
  separate from the main Qt thread. This because it will trigger
  operations where Qt3 is not thread-safe. For Qt4, we have not been
  aware of such problems.</li>

  </ul>

  \since Coin 2.0
*/

/* ********************************************************************** */

// All the documentation below is obsolete - however it may be useful for re-writing
// So, for now, simply revert to a normal c++ comment.  walroy 20140613

/*
  \class SbThread Inventor/threads/SbThread.h
  \brief A class for managing threads.

  \ingroup coin_threads

  This class provides a portable framework around the tasks of
  instantiating, starting, stopping and joining threads.

  It wraps the underlying native thread-handling toolkit in a
  transparent manner, to make multiplatform threads programming
  straightforward for the application programmer.
*/

/*
  \fn static SbThread * SbThread::create(void *(*func)(void *), void * closure)

  This function creates a new thread, or returns NULL on failure.
*/

/*
  \fn static void SbThread::destroy(SbThread * thread)

  This function destroys a thread.
*/

/*
  \fn static int SbThread::join(SbThread * thread, void ** retval)

  This function waits on the death of the given thread, returning the thread's
  return value at the location pointed to by \c retval.
*/

/*
  \fn int SbThread::join(void ** retval)

  This function waits on the death of the given thread, returning the thread's
  return value at the location pointed to by \c retval.
*/

/*
  \fn SbThread::~SbThread(void)

  Destructor.

  \sa SbThread::destroy
*/

/* ********************************************************************** */
