#ifndef COIN_THREADS_H
#define COIN_THREADS_H

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

#ifndef COIN_INTERNAL
#error this is a private header file
#endif /* ! COIN_INTERNAL */

#include <Inventor/C/basic.h>  /* COIN_DLL_API */

/* ********************************************************************** */

/* Implementation note: it is important that this header file can be
   included even when Coin was built with no threads support.

   (This simplifies client code, as we get away with far less #ifdef
   HAVE_THREADS wrapping.) */

/* ********************************************************************** */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ********************************************************************** */

/* Common threading types and enums */
typedef struct cc_sched cc_sched;
typedef struct cc_wpool cc_wpool;
typedef struct cc_worker cc_worker;
typedef struct cc_thread cc_thread;
typedef struct cc_mutex cc_mutex;
typedef struct cc_rwmutex cc_rwmutex;
typedef struct cc_condvar cc_condvar;
typedef struct cc_storage cc_storage;
typedef struct cc_fifo cc_fifo;
typedef struct cc_recmutex cc_recmutex;

/* used by rwmutex - read_precedence is default */
enum cc_precedence {
  CC_READ_PRECEDENCE,
  CC_WRITE_PRECEDENCE
};

enum cc_threads_implementation {
  CC_NO_THREADS = -1,
  CC_PTHREAD    = 0,
  CC_W32THREAD
};

enum cc_retval {
  CC_ERROR = 0,
  CC_OK = 1,
  CC_TIMEOUT,
  CC_BUSY
};

typedef enum cc_precedence cc_precedence;
typedef enum cc_threads_implementation cc_threads_implementation;
typedef enum cc_retval cc_retval;

/* ********************************************************************** */

COIN_DLL_API int cc_thread_implementation(void);

/* ********************************************************************** */

/* Thread storage API */
typedef void cc_storage_f(void * closure);
typedef void cc_storage_apply_func(void * dataptr, void * closure);

COIN_DLL_API cc_storage * cc_storage_construct(unsigned int size);
COIN_DLL_API cc_storage * cc_storage_construct_etc(unsigned int size, 
                                                   cc_storage_f * constructor,
                                                   cc_storage_f * destructor);
COIN_DLL_API void cc_storage_destruct(cc_storage * storage);

COIN_DLL_API void * cc_storage_get(cc_storage * storage);
COIN_DLL_API void cc_storage_apply_to_all(cc_storage * storage, 
                                          cc_storage_apply_func * func, 
                                          void * closure);

/* Thread API */
typedef void * cc_thread_f(void *);

COIN_DLL_API cc_thread * cc_thread_construct(cc_thread_f * func, void * closure);
COIN_DLL_API void cc_thread_destruct(cc_thread * thread);

COIN_DLL_API int cc_thread_join(cc_thread * thread, void ** retvalptr);

COIN_DLL_API unsigned long cc_thread_id(void);
COIN_DLL_API void cc_sleep(float seconds);

/* Mutex API */
COIN_DLL_API cc_mutex * cc_mutex_construct(void);
COIN_DLL_API void cc_mutex_destruct(cc_mutex * mutex);

COIN_DLL_API void cc_mutex_lock(cc_mutex * mutex);
COIN_DLL_API int cc_mutex_try_lock(cc_mutex * mutex);
COIN_DLL_API void cc_mutex_unlock(cc_mutex * mutex);

/* Condition variable API */
COIN_DLL_API cc_condvar * cc_condvar_construct(void);
COIN_DLL_API void cc_condvar_destruct(cc_condvar * condvar);

COIN_DLL_API int cc_condvar_wait(cc_condvar * condvar, cc_mutex * mutex);
COIN_DLL_API int cc_condvar_timed_wait(cc_condvar * condvar, cc_mutex * mutex,
                                       double period);

COIN_DLL_API void cc_condvar_wake_one(cc_condvar * condvar);
COIN_DLL_API void cc_condvar_wake_all(cc_condvar * condvar);

/* Recursive mutex API */
COIN_DLL_API cc_recmutex * cc_recmutex_construct(void);
COIN_DLL_API void cc_recmutex_destruct(cc_recmutex * recmutex);

COIN_DLL_API int cc_recmutex_lock(cc_recmutex * recmutex);
COIN_DLL_API int cc_recmutex_unlock(cc_recmutex * recmutex);
COIN_DLL_API int cc_recmutex_try_lock(cc_recmutex * recmutex);

/* ********************************************************************** */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* ! COIN_THREADS_H */