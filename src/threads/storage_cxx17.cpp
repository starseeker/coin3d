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
 * \file storage_cxx17.cpp
 * \brief Implementation of C++17 enhanced thread-local storage
 * 
 * This file implements enhanced thread-local storage functionality using
 * C++17 features while maintaining full compatibility with the existing
 * cc_storage C API. The key enhancement is automatic thread cleanup
 * when threads exit, addressing the long-standing FIXME in the original
 * implementation.
 */

#include "threads/storage_cxx17.h"
#include "coindefs.h"

#include <cassert>
#include <exception>
#include <iostream>
#include <mutex>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "C/threads/thread.h"
#include "C/threads/mutex.h"
#include "base/dict.h"

namespace CoinInternal {

// Thread-local cleanup trigger instance
thread_local std::unique_ptr<ThreadCleanupTrigger> ThreadCleanupTrigger::instance;

// Storage registry implementation
StorageRegistry& StorageRegistry::getInstance() {
    static StorageRegistry instance;
    return instance;
}

void StorageRegistry::registerStorage(cc_storage* storage) {
    if (!storage) return;
    
    std::unique_lock<std::shared_mutex> lock(registry_mutex);
    registered_storages.insert(storage);
}

void StorageRegistry::unregisterStorage(cc_storage* storage) {
    if (!storage) return;
    
    std::unique_lock<std::shared_mutex> lock(registry_mutex);
    registered_storages.erase(storage);
}

void StorageRegistry::cleanupThread(unsigned long threadid) {
    std::shared_lock<std::shared_mutex> lock(registry_mutex);
    
    // Iterate through all registered storage objects and clean up
    // the specified thread's data from each one
    for (cc_storage* storage : registered_storages) {
        if (!storage || !storage->dict) continue;
        
        void* data = nullptr;
        
        // Lock the storage mutex to safely access the dictionary
#ifdef HAVE_THREADS
        if (storage->mutex) {
            cc_mutex_lock(storage->mutex);
        }
#endif /* HAVE_THREADS */
        
        try {
            // Check if this thread has data in this storage
            if (cc_dict_get(storage->dict, threadid, &data) && data) {
                // Call destructor if provided
                if (storage->destructor) {
                    try {
                        storage->destructor(data);
                    } catch (...) {
                        // Swallow exceptions in destructors to prevent terminate()
                        // This preserves the behavior expected in C code
                    }
                }
                
                // Free the memory
                free(data);
                
                // Remove entry from dictionary
                cc_dict_remove(storage->dict, threadid);
            }
        } catch (...) {
            // Ensure we always unlock the mutex even if an exception occurs
#ifdef HAVE_THREADS
            if (storage->mutex) {
                cc_mutex_unlock(storage->mutex);
            }
#endif /* HAVE_THREADS */
            throw;
        }
        
#ifdef HAVE_THREADS
        if (storage->mutex) {
            cc_mutex_unlock(storage->mutex);
        }
#endif /* HAVE_THREADS */
    }
}

unsigned long StorageRegistry::getCurrentThreadId() {
    // Use the same thread ID mechanism as the existing cc_storage implementation
    return cc_thread_id();
}

// Thread cleanup trigger implementation
ThreadCleanupTrigger::ThreadCleanupTrigger() 
    : thread_id(StorageRegistry::getCurrentThreadId()) {
    // Constructor intentionally minimal - just store the thread ID
}

ThreadCleanupTrigger::~ThreadCleanupTrigger() {
    try {
        // When this destructor runs, the thread is exiting
        // Trigger cleanup for all storage objects
        StorageRegistry::getInstance().cleanupThread(thread_id);
    } catch (...) {
        // Swallow all exceptions in destructor to prevent terminate()
        // This is critical for thread exit scenarios
    }
}

void ThreadCleanupTrigger::ensureCleanupTrigger() {
    if (!instance) {
        instance = std::make_unique<ThreadCleanupTrigger>();
    }
}

} // namespace CoinInternal

// C API implementation
extern "C" {

void cc_storage_thread_cleanup_enhanced(unsigned long threadid) {
    try {
        CoinInternal::StorageRegistry::getInstance().cleanupThread(threadid);
    } catch (...) {
        // In C API, we cannot throw exceptions
        // Log error if possible, otherwise swallow
    }
}

void cc_storage_register_for_cleanup(cc_storage * storage) {
    if (storage) {
        CoinInternal::StorageRegistry::getInstance().registerStorage(storage);
    }
}

void cc_storage_unregister_for_cleanup(cc_storage * storage) {
    if (storage) {
        CoinInternal::StorageRegistry::getInstance().unregisterStorage(storage);
    }
}

} // extern "C"