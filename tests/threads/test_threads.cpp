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

/**
 * @file test_threads.cpp
 * @brief Threading API tests for Coin3D
 *
 * Tests for Coin threading primitives: SbMutex, SbThreadMutex,
 * SbCondVar, SbRWMutex, SbThread, SbBarrier, SbFifo, SbStorage,
 * SbTypedStorage, SbThreadAutoLock.
 *
 * Migrated from testsuite/threadsTest.cpp.
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/threads/SbMutex.h>
#include <Inventor/threads/SbThreadMutex.h>
#include <Inventor/threads/SbCondVar.h>
#include <Inventor/threads/SbRWMutex.h>
#include <Inventor/threads/SbThread.h>
#include <Inventor/threads/SbThreadAutoLock.h>
#include <Inventor/threads/SbBarrier.h>
#include <Inventor/threads/SbFifo.h>
#include <Inventor/threads/SbStorage.h>
#include <Inventor/threads/SbTypedStorage.h>
#include <Inventor/SbTime.h>

#include <vector>
#include <atomic>
#include <sstream>

// ---------------------------------------------------------------------------
// Shared test state
// ---------------------------------------------------------------------------
static std::atomic<int> g_counter{0};
static int g_shared_data = 0;
static SbMutex *g_mutex = nullptr;
static SbThreadMutex *g_rec_mutex = nullptr;
static SbCondVar *g_condvar = nullptr;
static SbRWMutex *g_rwmutex = nullptr;
static SbBarrier *g_barrier = nullptr;
static SbFifo *g_fifo = nullptr;

// ---------------------------------------------------------------------------
// Thread functions
// ---------------------------------------------------------------------------
static void *mutex_thread_func(void *data) {
    for (int i = 0; i < 100; ++i) {
        SbThreadAutoLock lock(g_mutex);
        g_shared_data++;
    }
    g_counter++;
    return nullptr;
}

static void *recursive_mutex_thread_func(void *data) {
    g_rec_mutex->lock();
    g_rec_mutex->lock();
    g_rec_mutex->lock();
    g_shared_data++;
    g_rec_mutex->unlock();
    g_rec_mutex->unlock();
    g_rec_mutex->unlock();
    g_counter++;
    return nullptr;
}

static void *condvar_producer_func(void *data) {
    for (int i = 0; i < 5; ++i) {
        g_mutex->lock();
        g_shared_data++;
        g_condvar->wakeOne();
        g_mutex->unlock();
        SbTime::sleep(10); // 10 ms
    }
    g_counter++;
    return nullptr;
}

static void *condvar_consumer_func(void *data) {
    int consumed = 0;
    while (consumed < 5) {
        g_mutex->lock();
        while (g_shared_data == consumed) {
            SbTime timeout(1.0);
            if (!g_condvar->timedWait(*g_mutex, timeout)) {
                g_mutex->unlock();
                return nullptr; // timeout
            }
        }
        consumed = g_shared_data;
        g_mutex->unlock();
    }
    g_counter++;
    return nullptr;
}

static void *rwmutex_reader_func(void *data) {
    for (int i = 0; i < 50; ++i) {
        g_rwmutex->readLock();
        volatile int value = g_shared_data;
        (void)value;
        g_rwmutex->readUnlock();
    }
    g_counter++;
    return nullptr;
}

static void *rwmutex_writer_func(void *data) {
    for (int i = 0; i < 10; ++i) {
        g_rwmutex->writeLock();
        g_shared_data++;
        g_rwmutex->writeUnlock();
    }
    g_counter++;
    return nullptr;
}

static void *barrier_thread_func(void *data) {
    g_counter++;
    g_barrier->enter();
    g_shared_data++;
    return nullptr;
}

static void *fifo_producer_func(void *data) {
    int *id = static_cast<int *>(data);
    for (int i = 0; i < 10; ++i) {
        int *value = new int(*id * 100 + i);
        g_fifo->assign(value, *id);
    }
    g_counter++;
    return nullptr;
}

static void *fifo_consumer_func(void *data) {
    int consumed = 0;
    while (consumed < 20) {
        void *item;
        uint32_t type;
        if (g_fifo->tryRetrieve(item, type)) {
            delete static_cast<int *>(item);
            consumed++;
        } else {
            SbTime::sleep(1);
        }
    }
    g_counter++;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Test implementations
// ---------------------------------------------------------------------------
static bool test_basic_mutex() {
    g_mutex = new SbMutex();
    g_counter = 0;
    g_shared_data = 0;

    const int num_threads = 4;
    std::vector<SbThread *> threads;
    std::vector<int> ids(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        ids[i] = i;
        threads.push_back(SbThread::create(mutex_thread_func, &ids[i]));
    }
    for (SbThread *t : threads) {
        t->join();
        SbThread::destroy(t);
    }

    bool ok = (g_counter.load() == num_threads) &&
              (g_shared_data == num_threads * 100);
    delete g_mutex;
    g_mutex = nullptr;
    return ok;
}

static bool test_recursive_mutex() {
    g_rec_mutex = new SbThreadMutex();
    g_counter = 0;
    g_shared_data = 0;

    const int num_threads = 3;
    std::vector<SbThread *> threads;
    std::vector<int> ids(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        ids[i] = i;
        threads.push_back(SbThread::create(recursive_mutex_thread_func, &ids[i]));
    }
    for (SbThread *t : threads) {
        t->join();
        SbThread::destroy(t);
    }

    bool ok = (g_counter.load() == num_threads) &&
              (g_shared_data == num_threads);
    delete g_rec_mutex;
    g_rec_mutex = nullptr;
    return ok;
}

static bool test_condition_variable() {
    g_mutex = new SbMutex();
    g_condvar = new SbCondVar();
    g_counter = 0;
    g_shared_data = 0;

    SbThread *producer = SbThread::create(condvar_producer_func, nullptr);
    SbThread *consumer = SbThread::create(condvar_consumer_func, nullptr);
    producer->join();
    consumer->join();
    SbThread::destroy(producer);
    SbThread::destroy(consumer);

    bool ok = (g_counter.load() == 2) && (g_shared_data == 5);
    delete g_condvar;
    delete g_mutex;
    g_condvar = nullptr;
    g_mutex = nullptr;
    return ok;
}

static bool test_rw_mutex() {
    g_rwmutex = new SbRWMutex(SbRWMutex::READ_PRECEDENCE);
    g_counter = 0;
    g_shared_data = 0;

    const int num_readers = 3;
    const int num_writers = 2;
    std::vector<SbThread *> threads;
    std::vector<int> ids(num_readers + num_writers);

    for (int i = 0; i < num_readers; ++i) {
        ids[i] = i;
        threads.push_back(SbThread::create(rwmutex_reader_func, &ids[i]));
    }
    for (int i = 0; i < num_writers; ++i) {
        ids[num_readers + i] = num_readers + i;
        threads.push_back(SbThread::create(rwmutex_writer_func, &ids[num_readers + i]));
    }
    for (SbThread *t : threads) {
        t->join();
        SbThread::destroy(t);
    }

    bool ok = (g_counter.load() == num_readers + num_writers) &&
              (g_shared_data == num_writers * 10);
    delete g_rwmutex;
    g_rwmutex = nullptr;
    return ok;
}

static bool test_barrier() {
    const int num_threads = 4;
    g_barrier = new SbBarrier(num_threads);
    g_counter = 0;
    g_shared_data = 0;

    std::vector<SbThread *> threads;
    std::vector<int> ids(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        ids[i] = i;
        threads.push_back(SbThread::create(barrier_thread_func, &ids[i]));
    }
    for (SbThread *t : threads) {
        t->join();
        SbThread::destroy(t);
    }

    bool ok = (g_counter.load() == num_threads) &&
              (g_shared_data == num_threads);
    delete g_barrier;
    g_barrier = nullptr;
    return ok;
}

static bool test_thread_safe_fifo() {
    g_fifo = new SbFifo();
    g_counter = 0;

    int id1 = 1, id2 = 2, id3 = 3;
    SbThread *p1 = SbThread::create(fifo_producer_func, &id1);
    SbThread *p2 = SbThread::create(fifo_producer_func, &id2);
    SbThread *c  = SbThread::create(fifo_consumer_func, &id3);
    p1->join(); p2->join(); c->join();
    SbThread::destroy(p1); SbThread::destroy(p2); SbThread::destroy(c);

    bool ok = (g_counter.load() == 3) && (g_fifo->size() == 0);
    delete g_fifo;
    g_fifo = nullptr;
    return ok;
}

static bool test_thread_local_storage() {
    SbStorage storage(sizeof(int));
    int *value = static_cast<int *>(storage.get());
    *value = 42;
    int *value2 = static_cast<int *>(storage.get());
    return (*value2 == 42);
}

static bool test_typed_thread_local_storage() {
    SbTypedStorage<int *> typed_storage(sizeof(int *));
    int test_value = 123;
    int **ptr = reinterpret_cast<int **>(typed_storage.get());
    *ptr = &test_value;
    int **ptr2 = reinterpret_cast<int **>(typed_storage.get());
    return (**ptr2 == 123);
}

static bool test_auto_lock() {
    SbMutex mutex;
    {
        SbThreadAutoLock lock(&mutex);
        // Mutex locked; tryLock should fail
        if (mutex.tryLock()) {
            mutex.unlock();
            return false;
        }
    }
    // Mutex released; tryLock should succeed
    if (!mutex.tryLock()) return false;
    mutex.unlock();
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int /*argc*/, char ** /*argv*/) {
    // Initialise Coin with a null context manager (no rendering needed)
    class NullCtxMgr : public SoDB::ContextManager {
    public:
        void *createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
        SbBool makeContextCurrent(void *) override { return FALSE; }
        void restorePreviousContext(void *) override {}
        void destroyContext(void *) override {}
    };
    static NullCtxMgr ctxMgr;
    SoDB::init(&ctxMgr);
    SoInteraction::init();

    SimpleTest::TestRunner runner;

    struct TestCase { const char *name; bool (*func)(); };
    TestCase tests[] = {
        { "basicMutex",            test_basic_mutex           },
        { "recursiveMutex",        test_recursive_mutex       },
        { "conditionVariable",     test_condition_variable    },
        { "readerWriterMutex",     test_rw_mutex              },
        { "barrierSynchronization",test_barrier               },
        { "threadSafeFifo",        test_thread_safe_fifo      },
        { "threadLocalStorage",    test_thread_local_storage  },
        { "typedThreadLocalStorage", test_typed_thread_local_storage },
        { "automaticLocking",      test_auto_lock             },
    };

    for (auto &tc : tests) {
        runner.startTest(tc.name);
        bool passed = false;
        try {
            passed = tc.func();
        } catch (const std::exception &e) {
            runner.endTest(false, e.what());
            continue;
        } catch (...) {
            runner.endTest(false, "Unknown exception");
            continue;
        }
        runner.endTest(passed, passed ? "" : "unexpected result");
    }

    SoDB::finish();
    return runner.getSummary();
}
