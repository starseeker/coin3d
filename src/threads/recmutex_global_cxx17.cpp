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
  \file recmutex_global_cxx17.cpp
  \brief C++17 replacement for global recursive mutex functionality.
  
  This file provides modern C++17 implementations of the cc_recmutex_internal_*
  functions using SbThreadMutex (which uses std::recursive_mutex internally).
  This eliminates the need for custom C-based recursive mutex implementation
  for global locking scenarios.
*/

#include "threads/recmutex_global_cxx17.h"

#include <Inventor/threads/SbThreadMutex.h>
#include "threads/threads.h"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <cassert>
#include <atomic>

#include "CoinTidbits.h"

/* ********************************************************************** */

// Global C++17 recursive mutexes replacing the C cc_recmutex implementation
static std::atomic<SbThreadMutex *> field_mutex_cxx17{nullptr};
static std::atomic<SbThreadMutex *> notify_mutex_cxx17{nullptr};
static std::mutex recmutex_lifecycle_mutex;

// Thread-local nesting levels to track recursive lock counts
// This maintains API compatibility with the original cc_recmutex_internal_* functions
static thread_local int field_lock_level = 0;
static thread_local int notify_lock_level = 0;

static void
recmutex_cxx17_cleanup(void)
{
  std::lock_guard<std::mutex> guard(recmutex_lifecycle_mutex);
  delete field_mutex_cxx17.exchange(nullptr, std::memory_order_acq_rel);
  delete notify_mutex_cxx17.exchange(nullptr, std::memory_order_acq_rel);
}

void 
cc_recmutex_cxx17_init(void)
{
  if (field_mutex_cxx17.load(std::memory_order_acquire) == nullptr) {
    std::lock_guard<std::mutex> guard(recmutex_lifecycle_mutex);
    if (field_mutex_cxx17.load(std::memory_order_relaxed) == nullptr) {
      std::unique_ptr<SbThreadMutex> field(new SbThreadMutex);
      std::unique_ptr<SbThreadMutex> notify(new SbThreadMutex);
      /* field_mutex_cxx17 is the initialization guard observed by the fast
         path.  Publish the dependent notify mutex first so that an acquire
         load of a non-null field mutex also makes the notify mutex visible. */
      notify_mutex_cxx17.store(notify.release(), std::memory_order_release);
      field_mutex_cxx17.store(field.release(), std::memory_order_release);

      /* Register once per initialized generation. */
      coin_atexit((coin_atexit_f*) recmutex_cxx17_cleanup,
                  CC_ATEXIT_THREADING_SUBSYSTEM);
    }
  }
}

int 
cc_recmutex_cxx17_field_lock(void)
{
  cc_recmutex_cxx17_init();
  SbThreadMutex * mutex = field_mutex_cxx17.load(std::memory_order_acquire);
  assert(mutex != nullptr);
  
  // SbThreadMutex::lock() returns 0 on success
  mutex->lock();
  
  // Increment thread-local nesting level
  field_lock_level++;
  
  return field_lock_level;
}

int 
cc_recmutex_cxx17_field_unlock(void)
{
  cc_recmutex_cxx17_init();
  SbThreadMutex * mutex = field_mutex_cxx17.load(std::memory_order_acquire);
  assert(mutex != nullptr);
  assert(field_lock_level > 0);
  
  // Decrement thread-local nesting level
  field_lock_level--;
  
  // SbThreadMutex::unlock() returns 0 on success
  mutex->unlock();
  
  return field_lock_level;
}

int 
cc_recmutex_cxx17_notify_lock(void)
{
  cc_recmutex_cxx17_init();
  SbThreadMutex * mutex = notify_mutex_cxx17.load(std::memory_order_acquire);
  assert(mutex != nullptr);
  
  // SbThreadMutex::lock() returns 0 on success
  mutex->lock();
  
  // Increment thread-local nesting level
  notify_lock_level++;
  
  return notify_lock_level;
}

int 
cc_recmutex_cxx17_notify_unlock(void)
{
  cc_recmutex_cxx17_init();
  SbThreadMutex * mutex = notify_mutex_cxx17.load(std::memory_order_acquire);
  assert(mutex != nullptr);
  assert(notify_lock_level > 0);
  
  // Decrement thread-local nesting level
  notify_lock_level--;
  
  // SbThreadMutex::unlock() returns 0 on success
  mutex->unlock();
  
  return notify_lock_level;
}
