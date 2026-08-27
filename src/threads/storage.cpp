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

/*
  This ADT manages thread-local memory.  When different threads access
  the memory an cc_storage object manages, they will receive different
  memory blocks back.

  For additional API documentation, see doc of the SbStorage C++
  wrapper around the cc_storage_*() functions at the bottom of this
  file.
*/

/* ********************************************************************** */

/*!
  \struct cc_storage threads.h src/threads/threads.h
  \ingroup coin_threads
  \brief The structure for the thread local memory storage.
*/

/*!
  \typedef struct cc_storage cc_storage
  \ingroup coin_threads
  \brief The type definition for the thread local memory storage structure.
*/

#include "threads/threads.h"
#include "config.h"

#include <cstdlib>
#include <cassert>

#include "threads/storagep.h"
#include "threads/storage_cxx17.h"

/* ********************************************************************** */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* private functions */

static cc_storage *
cc_storage_init(unsigned int size, void (*constructor)(void *), 
                void (*destructor)(void *)) 
{
  cc_storage * storage = new cc_storage;
  storage->size = size;
  storage->constructor = constructor;
  storage->destructor = destructor;
  storage->dict = new cc_storage_map;
#ifdef HAVE_THREADS
  storage->mutex = cc_mutex_construct();
#endif /* HAVE_THREADS */

  // Register with enhanced cleanup system
  cc_storage_register_for_cleanup(storage);

  return storage;
}

/* public api */

cc_storage *
cc_storage_construct(unsigned int size)
{
  return cc_storage_init(size, NULL, NULL);
}

cc_storage *
cc_storage_construct_etc(unsigned int size,
                         void (*constructor)(void *),
                         void (*destructor)(void *))
{
  return cc_storage_init(size, constructor, destructor);
}

/*
*/
void
cc_storage_destruct(cc_storage * storage)
{
  assert(storage != NULL);

  // Unregister from enhanced cleanup system first
  cc_storage_unregister_for_cleanup(storage);

  for (const auto & entry : *storage->dict) {
    if (storage->destructor) storage->destructor(entry.second);
    free(entry.second);
  }
  delete storage->dict;

#ifdef HAVE_THREADS
  cc_mutex_destruct(storage->mutex);
#endif /* HAVE_THREADS */

  delete storage;
}

/* ********************************************************************** */

/*
*/

void *
cc_storage_get(cc_storage * storage)
{
  void * val;
  cc_thread_id_t threadid = 0;

#ifdef HAVE_THREADS
  threadid = cc_thread_id();

  // Ensure cleanup trigger is set up for this thread
  // This will automatically clean up all storage when the thread exits
#ifdef __cplusplus
  CoinInternal::ThreadCleanupTrigger::ensureCleanupTrigger();
#endif

  cc_mutex_lock(storage->mutex);
#endif /* HAVE_THREADS */

  const auto existing = storage->dict->find(threadid);
  if (existing == storage->dict->end()) {
    val = malloc(storage->size);
    if (storage->constructor) {
      storage->constructor(val);
    }
    storage->dict->emplace(threadid, val);
  }
  else val = existing->second;

#ifdef HAVE_THREADS
  cc_mutex_unlock(storage->mutex);
#endif /* HAVE_THREADS */

  return val;
}

/* struct needed for cc_dict wrapper callback */
typedef struct {
  cc_storage_apply_func * func;
  void * closure;
} cc_storage_hash_apply_data; 

/* callback from cc_dict_apply. will simply call the function specified
   in cc_storage_apply_to_appl */
void 
cc_storage_apply_to_all(cc_storage * storage, 
                        cc_storage_apply_func * func, 
                        void * closure)
{
  /* need to set up a struct to use cc_dict_apply */
  cc_storage_hash_apply_data mydata;
  
  /* store func and closure in struct */
  mydata.func = func;
  mydata.closure = closure;

#ifdef HAVE_THREADS
  cc_mutex_lock(storage->mutex);
  for (const auto & entry : *storage->dict) {
    mydata.func(entry.second, mydata.closure);
  }
  cc_mutex_unlock(storage->mutex);
#else /* ! HAVE_THREADS */
  for (const auto & entry : *storage->dict) {
    mydata.func(entry.second, mydata.closure);
  }
#endif /* ! HAVE_THREADS */

}


/* ********************************************************************** */

void 
cc_storage_thread_cleanup(cc_thread_id_t threadid)
{
  // Use the enhanced cleanup implementation
  cc_storage_thread_cleanup_enhanced(threadid);
}

/* ********************************************************************** */

// All the documentation below is obsolete - however it may be useful for re-writing
// So, for now, simply revert to a normal c++ comment.  walroy 20140613
/*
  \class SbStorage Inventor/threads/SbStorage.h
  \brief The SbStorage class manages thread-local memory.

  \ingroup coin_threads

  This class manages thread-local memory.  When different threads
  access the memory an SbStorage object manages, they will receive
  different memory blocks back.

  This provides a mechanism for sharing read/write static data.

  One important implementation detail: if the Coin library was
  explicitly configured to be built without multi-platform thread
  abstractions, or neither pthreads nor native Win32 thread functions
  are available, it will be assumed that the client code will all run
  in the same thread. This means that the same memory block will be
  returned for any request without considering the current thread id.
*/

/*
  \fn SbStorage::SbStorage(unsigned int size)

  Constructor.  \a size specifies the number of bytes each thread should
  have in this thread-local memory management object.
*/

/*
  \fn SbStorage::SbStorage(unsigned int size, void (*constr)(void *), void (*destr)(void *))

  Constructor.  \a size specifies the number of bytes each thread should
  have in this thread-local memory management object.  A constructor and
  a destructor functions can be given that will be called when the actual
  memory blocks are allocated and freed.
*/

/*
  \fn SbStorage::~SbStorage(void)

  The destructor.
*/

/*
  \fn void * SbStorage::get(void)

  This method returns the calling thread's thread-local memory block.
*/

/*
  \fn void SbStorage::applyToAll(SbStorageApplyFunc * func, void * closure)
  
  This method will call \a func for all thread local storage data.
  \a closure will be supplied as the second parameter to the callback.
*/

/* ********************************************************************** */

/*
  \class SbTypedStorage Inventor/threads/SbTypedStorage.h
  \brief The SbTypedStorage class manages generic thread-local memory.

  \ingroup coin_threads

  This class manages thread-local memory.  When different threads
  access the memory an SbTypedStorage object manages, they will receive
  different memory blocks back.

  This provides a mechanism for sharing read/write static data.
*/

/*
  \fn SbTypedStorage<Type>::SbTypedStorage(unsigned int size)

  Constructor.  \a size specifies the number of bytes each thread should
  have in this thread-local memory management object.
*/

/*
  \fn SbTypedStorage<Type>::SbTypedStorage(unsigned int size, void (*constr)(void *), void (*destr)(void *))

  Constructor.  \a size specifies the number of bytes each thread
  should have in this thread-local memory management object.
  Constructor and a destructor functions can be specified that will be
  called when the actual memory blocks are allocated and freed.
*/

/*
  \fn SbTypedStorage<Type>::~SbTypedStorage(void)

  The destructor.
*/

/*
  \fn Type SbTypedStorage<Type>::get(void)

  This method returns the calling thread's thread-local memory block.
*/

/* ********************************************************************** */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
