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
#include "config.h"

#include <cassert>
#include <mutex>
#include <vector>

#include "threads/threads.h"

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

    std::lock_guard<std::mutex> lock(registry_mutex);
    registered_storages[storage] = std::make_shared<StorageRecord>(storage);
}

void StorageRegistry::unregisterStorage(cc_storage* storage) {
    if (!storage) return;

    std::unique_lock<std::mutex> lock(registry_mutex);
    const auto it = registered_storages.find(storage);
    if (it == registered_storages.end()) return;

    // Retire the stable registry record before waiting. Cleanup snapshots own
    // records, not raw storage pointers, so a snapshot taken before this call
    // can observe the retirement without dereferencing freed storage.
    const std::shared_ptr<StorageRecord> record = it->second;
    record->registered = false;
    record->storage = nullptr;
    registered_storages.erase(it);
    registry_cv.wait(lock, [&record] {
        return record->active_cleanups == 0;
    });
}

void StorageRegistry::cleanupThread(unsigned long threadid) {
    // Snapshot stable records rather than raw storage pointers. A lifetime
    // lease is acquired for only one storage at a time and is released before
    // its user destructor is invoked. Consequently a destructor may safely
    // unregister either its own storage or another storage in this snapshot.
    std::vector<std::shared_ptr<StorageRecord>> snapshot;
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        snapshot.reserve(registered_storages.size());
        for (const auto & item : registered_storages) {
            snapshot.push_back(item.second);
        }
    }

    for (const std::shared_ptr<StorageRecord> & record : snapshot) {
        cc_storage * storage = nullptr;
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            if (!record->registered || !record->storage) continue;
            ++record->active_cleanups;
            storage = record->storage;
        }

        void * data = nullptr;
        void (*destructor)(void *) = nullptr;

        // Detach this thread's data while the storage lifetime is leased and
        // its dictionary is locked. No user code runs under either lock.
#ifdef HAVE_THREADS
        if (storage->mutex) cc_mutex_lock(storage->mutex);
#endif /* HAVE_THREADS */
        if (storage->dict && cc_dict_get(storage->dict, threadid, &data) && data) {
            destructor = storage->destructor;
            cc_dict_remove(storage->dict, threadid);
        }
#ifdef HAVE_THREADS
        if (storage->mutex) cc_mutex_unlock(storage->mutex);
#endif /* HAVE_THREADS */

        // The cleanup code will not touch storage after this point. Release
        // the lease before invoking the callback so reentrant destruction
        // cannot wait on a lease owned by this same thread.
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            assert(record->active_cleanups > 0);
            --record->active_cleanups;
            registry_cv.notify_all();
        }

        if (data) {
            if (destructor) {
                try {
                    destructor(data);
                }
                catch (...) {
                    // Preserve the C callback contract at thread exit.
                }
            }
            free(data);
        }
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
