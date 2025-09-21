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

/* FIXME: should provide dummy implementations of the (few) internal
   public cc_mutex_*() calls, so one can include the header files
   mutex.h and SbMutex.h without #ifdef checks, and also declare
   e.g. SbMutex instances when thread-support is missing.

   This would clean up source code everywhere we're using mutex'es.

   20050516 mortene.
*/

/*!
  \struct cc_mutex common.h Inventor/C/threads/common.h
  \ingroup coin_threads
  \brief The structure for a mutex.
*/

/*!
  \typedef struct cc_mutex cc_mutex
  \ingroup coin_threads
  \brief The type definition for the mutex structure.
*/

#include "threads/threads.h"

#include <cstdlib>
#include <cassert>
#include <cstddef>
#include <cerrno>
#include <cfloat>
#include <chrono>

#include "errors/CoinInternalError.h"

#include "threads/mutexp.h"
#include "C/CoinTidbits.h"
#include "misc/SoEnvironment.h"

// C++17 includes for modern threading
#ifdef USE_CXX17_THREADS
#include <mutex>
#include <memory>
#endif

// C++17 threading is the only supported implementation
#include "mutex_cxx17.icc"

/**************************************************************************/

static double maxmutexlocktime = DBL_MAX;
static double reportmutexlocktiming = DBL_MAX;

/**************************************************************************/

/*
  \internal
*/

void
cc_mutex_struct_init(cc_mutex * mutex_struct)
{
  int ok;
  ok = internal_mutex_struct_init(mutex_struct);
  assert(ok);
  (void)ok; /* avoid unused variable warning in release builds */
}

/*
  \internal
*/

void
cc_mutex_struct_clean(cc_mutex * mutex_struct)
{
  int ok;
  assert(mutex_struct);
  ok = internal_mutex_struct_clean(mutex_struct);
  assert(ok == CC_OK);
  (void)ok; /* avoid unused variable warning in release builds */
}

/**************************************************************************/

/* debugging. for instance useful for checking that there's not
   excessive mutex construction. */

/* don't hide 'static' these to hide them in file-scope, as they are
   used from rwmutex.cpp and recmutex.cpp as well. */
unsigned int cc_debug_mtxcount = 0;
const char * COIN_DEBUG_MUTEX_COUNT = "COIN_DEBUG_MUTEX_COUNT";

/**************************************************************************/

/* Return value of COIN_DEBUG_MUTEX_COUNT environment variable. */
static int coin_debug_mutex_count(void)
{
  static int d = -1;
  if (d == -1) {
    const char* val = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_MUTEX_COUNT");
    d = val ? atoi(val) : 0;
  }
  return d;
}

/* Helper function to get current time as double (replacement for cc_time_gettimeofday) */
static double get_current_time_seconds(void) 
{
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration);
  return seconds.count();
}

/*! Constructs a mutex. */
cc_mutex *
cc_mutex_construct(void)
{
  cc_mutex * mutex;
  mutex = (cc_mutex *) malloc(sizeof(cc_mutex));
  assert(mutex != NULL);
  cc_mutex_struct_init(mutex);

  /* debugging */
  if (coin_debug_mutex_count() > 0) {
    cc_debug_mtxcount += 1;
    (void)fprintf(stderr, "DEBUG: live mutexes +1 => %u (mutex++)\n",
                  cc_debug_mtxcount);
  }

  return mutex;
}

/*! Destroys the \a mutex specified. */
void
cc_mutex_destruct(cc_mutex * mutex)
{
  /* debugging */
  if (coin_debug_mutex_count() > 0) {
    assert((cc_debug_mtxcount > 0) && "skewed mutex construct/destruct pairing");
    cc_debug_mtxcount -= 1;
    (void)fprintf(stderr, "DEBUG: live mutexes -1 => %u (mutex--)\n",
                  cc_debug_mtxcount);
  }

  assert(mutex != NULL);
  cc_mutex_struct_clean(mutex);
  free(mutex);
}

/**************************************************************************/

/*! Locks the \a mutex specified. */
void
cc_mutex_lock(cc_mutex * mutex)
{
  int ok;
  SbBool timeit;
  double start = 0.0;

  assert(mutex != NULL);

  timeit = (maxmutexlocktime != DBL_MAX) || (reportmutexlocktiming != DBL_MAX);
  if (timeit) { start = get_current_time_seconds(); }

  ok = internal_mutex_lock(mutex);

  assert(ok == CC_OK);
  (void)ok; /* avoid unused variable warning in release builds */

  /* This is here as an optional debugging aid, when having problems
     related to locks that are held too long. (Typically resulting in
     unresponsive user interaction / lags.)  */
  if (timeit) {
    const double spent = get_current_time_seconds() - start;

    if (spent >= reportmutexlocktiming) {
      /* Can't use cc_debugerror_postinfo() here, because we get a
         recursive call to this function, and a non-terminating lock /
         hang. */
      (void)fprintf(stdout, "DEBUG cc_mutex_lock(): mutex %p spent %f secs in lock\n",
                    mutex, spent);
    }

    assert(spent <= maxmutexlocktime);
  }
}

/*! Tests the specified \a mutex to see it is already locked. */

int
cc_mutex_try_lock(cc_mutex * mutex)
{
  int ok;
  assert(mutex != NULL);
  ok = internal_mutex_try_lock(mutex);
  assert(ok == CC_OK || ok == CC_BUSY);
  return ok;
}

/*! Unlocks the specified \a mutex.*/

void
cc_mutex_unlock(cc_mutex * mutex)
{
  int ok;
  assert(mutex != NULL);
  ok = internal_mutex_unlock(mutex);
  assert(ok == CC_OK);
  (void)ok; /* avoid unused variable warning in release builds */
}

static cc_mutex * cc_global_mutex = NULL;

static void
cc_mutex_cleanup(void)
{
  cc_mutex_destruct(cc_global_mutex);
  cc_global_mutex = NULL;
}

void
cc_mutex_init(void)
{
  const char * env = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_MUTEXLOCK_MAXTIME");

  if (cc_global_mutex == NULL) {
    cc_global_mutex = cc_mutex_construct();
    /* atexit priority makes this callback trigger after other cleanup
       functions. */
    /* FIXME: not sure if this really needs the "- 1", but I added it
       to keep the same order wrt the other thread-related cleanup
       functions, since before I changed hard-coded numbers for
       enumerated values for coin_atexit() invocations. 20060301 mortene. */
    coin_atexit((coin_atexit_f*) cc_mutex_cleanup, CC_ATEXIT_THREADING_SUBSYSTEM_LOWPRIORITY);
  }

  if (env) { maxmutexlocktime = atof(env); }

  env = CoinInternal::getEnvironmentVariableRaw("COIN_DEBUG_MUTEXLOCK_TIMING");
  if (env) { reportmutexlocktiming = atof(env); }
}

void 
cc_mutex_global_lock(void)
{
  /* Do this test in case a mutex is needed before cc_mutex_init() is
     called (called from SoDB::init()). This is safe, since the
     application should not be multithreaded before SoDB::init() is
     called */
  if (cc_global_mutex == NULL) cc_mutex_init();
  
  (void) cc_mutex_lock(cc_global_mutex);
}

void 
cc_mutex_global_unlock(void)
{
  (void) cc_mutex_unlock(cc_global_mutex);
}

