#ifndef CC_STORAGE_CXX17_H
#define CC_STORAGE_CXX17_H

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
 * \file storage_cxx17.h
 * \brief Enhanced thread-local storage with C++17 improvements
 * 
 * This header provides C++17-enhanced implementations for thread-local storage
 * while maintaining full compatibility with the existing cc_storage C API.
 * 
 * Key improvements over the original C implementation:
 * - Automatic thread cleanup using RAII and std::thread_local destructors
 * - Enhanced thread safety using C++17 threading primitives
 * - Better exception safety in constructor/destructor callbacks
 * - Global storage registry for comprehensive thread cleanup
 */

#ifndef COIN_INTERNAL
#error this is a private header file
#endif /* ! COIN_INTERNAL */

#include "threads/threads.h"
#include "threads/storagep.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <thread>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*!
 * \brief Enhanced thread cleanup implementation for cc_storage
 * 
 * This function provides a complete implementation of thread cleanup
 * functionality that was previously unimplemented (FIXME stub).
 * 
 * It safely removes and destructs all data for the specified thread
 * across all active storage objects, preventing memory leaks in
 * applications with frequently created/destroyed threads.
 * 
 * \param threadid The ID of the thread to clean up
 */
void cc_storage_thread_cleanup_enhanced(unsigned long threadid);

/*!
 * \brief Register a storage object for enhanced thread cleanup
 * 
 * This function registers a storage object with the global cleanup
 * system, enabling automatic cleanup when threads exit.
 * 
 * \param storage The storage object to register
 */
void cc_storage_register_for_cleanup(cc_storage * storage);

/*!
 * \brief Unregister a storage object from enhanced thread cleanup
 * 
 * This function removes a storage object from the global cleanup
 * system. Should be called before destroying the storage object.
 * 
 * \param storage The storage object to unregister
 */
void cc_storage_unregister_for_cleanup(cc_storage * storage);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#ifdef __cplusplus

namespace CoinInternal {

/*!
 * \brief C++17 Enhanced Storage Registry
 * 
 * This class provides a thread-safe registry of all active storage
 * objects, enabling comprehensive cleanup when threads exit.
 * 
 * Key features:
 * - Thread-safe registration/unregistration using shared_mutex
 * - Automatic cleanup detection using thread_local destructors
 * - Exception-safe operations throughout
 * - Global singleton pattern for application-wide cleanup
 */
class StorageRegistry {
public:
    /*!
     * \brief Get the global storage registry instance
     * \return Reference to the singleton registry
     */
    static StorageRegistry& getInstance();

    /*!
     * \brief Register a storage object for thread cleanup
     * \param storage Pointer to the storage object to register
     */
    void registerStorage(cc_storage* storage);

    /*!
     * \brief Unregister a storage object from thread cleanup
     * \param storage Pointer to the storage object to unregister
     */
    void unregisterStorage(cc_storage* storage);

    /*!
     * \brief Clean up all data for a specific thread across all storage objects
     * \param threadid The ID of the thread to clean up
     */
    void cleanupThread(unsigned long threadid);

    /*!
     * \brief Get the current thread ID in a platform-independent way
     * \return Thread ID compatible with cc_storage key format
     */
    static unsigned long getCurrentThreadId();

private:
    StorageRegistry() = default;
    ~StorageRegistry() = default;
    StorageRegistry(const StorageRegistry&) = delete;
    StorageRegistry& operator=(const StorageRegistry&) = delete;

    mutable std::shared_mutex registry_mutex;
    std::unordered_set<cc_storage*> registered_storages;
};

/*!
 * \brief Thread-local cleanup trigger
 * 
 * This class uses RAII to automatically trigger thread cleanup
 * when a thread exits. An instance is created as thread_local
 * in any thread that uses storage, and its destructor will
 * clean up all storage for that thread.
 */
class ThreadCleanupTrigger {
public:
    ThreadCleanupTrigger();
    ~ThreadCleanupTrigger();

    // Ensure cleanup trigger is created for current thread
    static void ensureCleanupTrigger();

private:
    unsigned long thread_id;
    static thread_local std::unique_ptr<ThreadCleanupTrigger> instance;
};

} // namespace CoinInternal

#endif /* __cplusplus */

#endif /* !CC_STORAGE_CXX17_H */