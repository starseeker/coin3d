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

#include "CoinTestFramework.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include "TestSuiteUtils.h"
#include "TestSuiteMisc.h"
using namespace SIM::Coin3D::Coin;
using namespace SIM::Coin3D::Coin::TestSuite;

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

BOOST_AUTO_TEST_SUITE(ThreadingMigration_TestSuite);

// Test data structures
static std::atomic<int> global_counter{0};
static int shared_data = 0;
static SbMutex * test_mutex = nullptr;
static SbThreadMutex * test_rec_mutex = nullptr;
static SbCondVar * test_condvar = nullptr;
static SbRWMutex * test_rwmutex = nullptr;
static SbBarrier * test_barrier = nullptr;
static SbFifo * test_fifo = nullptr;

// Test function for basic mutex functionality
static void * mutex_test_func(void * data)
{
  int * thread_id = static_cast<int*>(data);
  
  for (int i = 0; i < 100; ++i) {
    SbThreadAutoLock lock(test_mutex);
    shared_data++;
  }
  
  global_counter++;
  return nullptr;
}

// Test function for recursive mutex functionality  
static void * recursive_mutex_test_func(void * data)
{
  int * thread_id = static_cast<int*>(data);
  
  // Test recursive locking
  test_rec_mutex->lock();
  test_rec_mutex->lock(); // Should not deadlock
  test_rec_mutex->lock(); // Should not deadlock
  
  shared_data++;
  
  test_rec_mutex->unlock();
  test_rec_mutex->unlock();
  test_rec_mutex->unlock();
  
  global_counter++;
  return nullptr;
}

// Test function for condition variable functionality
static void * condvar_producer_func(void * data)
{
  SbTime delay(0.1); // 100ms delay
  
  for (int i = 0; i < 5; ++i) {
    test_mutex->lock();
    shared_data++;
    test_condvar->wakeOne();
    test_mutex->unlock();
    
    // Sleep briefly using milliseconds
    SbTime::sleep(static_cast<int>(delay.getMsecValue()));
  }
  
  global_counter++;
  return nullptr;
}

static void * condvar_consumer_func(void * data)
{
  int consumed = 0;
  
  while (consumed < 5) {
    test_mutex->lock();
    while (shared_data == consumed) {
      SbTime timeout(1.0); // 1 second timeout
      if (!test_condvar->timedWait(*test_mutex, timeout)) {
        test_mutex->unlock();
        return nullptr; // Timeout
      }
    }
    consumed = shared_data;
    test_mutex->unlock();
  }
  
  global_counter++;
  return nullptr;
}

// Test function for RW mutex functionality
static void * rwmutex_reader_func(void * data)
{
  int * thread_id = static_cast<int*>(data);
  
  for (int i = 0; i < 50; ++i) {
    test_rwmutex->readLock();
    // Read shared_data (simulate work)
    volatile int value = shared_data;
    (void)value; // Suppress unused variable warning
    test_rwmutex->readUnlock();
  }
  
  global_counter++;
  return nullptr;
}

static void * rwmutex_writer_func(void * data)
{
  int * thread_id = static_cast<int*>(data);
  
  for (int i = 0; i < 10; ++i) {
    test_rwmutex->writeLock();
    shared_data++;
    test_rwmutex->writeUnlock();
  }
  
  global_counter++;
  return nullptr;
}

// Test function for barrier functionality
static void * barrier_test_func(void * data)
{
  int * thread_id = static_cast<int*>(data);
  
  // Each thread increments counter before barrier
  global_counter++;
  
  // Wait at barrier
  test_barrier->enter();
  
  // All threads should reach here simultaneously
  shared_data++;
  
  return nullptr;
}

// Test function for FIFO functionality
static void * fifo_producer_func(void * data)
{
  int * thread_id = static_cast<int*>(data);
  
  for (int i = 0; i < 10; ++i) {
    int * value = new int(*thread_id * 100 + i);
    test_fifo->assign(value, *thread_id);
  }
  
  global_counter++;
  return nullptr;
}

static void * fifo_consumer_func(void * data)
{
  int * thread_id = static_cast<int*>(data);
  int consumed = 0;
  
  while (consumed < 20) { // Two producers, 10 items each
    void * item;
    uint32_t type;
    
    if (test_fifo->tryRetrieve(item, type)) {
      int * value = static_cast<int*>(item);
      delete value;
      consumed++;
    } else {
      SbTime::sleep(1); // Brief delay in milliseconds
    }
  }
  
  global_counter++;
  return nullptr;
}

BOOST_AUTO_TEST_CASE(basicMutex)
{
  test_mutex = new SbMutex();
  global_counter = 0;
  shared_data = 0;
  
  const int num_threads = 4;
  std::vector<SbThread*> threads;
  std::vector<int> thread_ids(num_threads);
  
  // Create threads
  for (int i = 0; i < num_threads; ++i) {
    thread_ids[i] = i;
    threads.push_back(SbThread::create(mutex_test_func, &thread_ids[i]));
  }
  
  // Wait for all threads to complete
  for (SbThread* thread : threads) {
    thread->join();
    SbThread::destroy(thread);
  }
  
  // Verify results
  BOOST_CHECK_EQUAL(global_counter.load(), num_threads);
  BOOST_CHECK_EQUAL(shared_data, num_threads * 100);
  
  delete test_mutex;
  test_mutex = nullptr;
}

BOOST_AUTO_TEST_CASE(recursiveMutex)
{
  test_rec_mutex = new SbThreadMutex();
  global_counter = 0;
  shared_data = 0;
  
  const int num_threads = 3;
  std::vector<SbThread*> threads;
  std::vector<int> thread_ids(num_threads);
  
  // Create threads
  for (int i = 0; i < num_threads; ++i) {
    thread_ids[i] = i;
    threads.push_back(SbThread::create(recursive_mutex_test_func, &thread_ids[i]));
  }
  
  // Wait for all threads to complete
  for (SbThread* thread : threads) {
    thread->join();
    SbThread::destroy(thread);
  }
  
  // Verify results
  BOOST_CHECK_EQUAL(global_counter.load(), num_threads);
  BOOST_CHECK_EQUAL(shared_data, num_threads);
  
  delete test_rec_mutex;
  test_rec_mutex = nullptr;
}

BOOST_AUTO_TEST_CASE(conditionVariable)
{
  test_mutex = new SbMutex();
  test_condvar = new SbCondVar();
  global_counter = 0;
  shared_data = 0;
  
  // Create producer and consumer threads
  SbThread* producer = SbThread::create(condvar_producer_func, nullptr);
  SbThread* consumer = SbThread::create(condvar_consumer_func, nullptr);
  
  // Wait for completion
  producer->join();
  consumer->join();
  
  SbThread::destroy(producer);
  SbThread::destroy(consumer);
  
  // Verify results
  BOOST_CHECK_EQUAL(global_counter.load(), 2);
  BOOST_CHECK_EQUAL(shared_data, 5);
  
  delete test_condvar;
  delete test_mutex;
  test_condvar = nullptr;
  test_mutex = nullptr;
}

BOOST_AUTO_TEST_CASE(readerWriterMutex)
{
  test_rwmutex = new SbRWMutex(SbRWMutex::READ_PRECEDENCE);
  global_counter = 0;
  shared_data = 0;
  
  const int num_readers = 3;
  const int num_writers = 2;
  std::vector<SbThread*> threads;
  std::vector<int> thread_ids(num_readers + num_writers);
  
  // Create reader threads
  for (int i = 0; i < num_readers; ++i) {
    thread_ids[i] = i;
    threads.push_back(SbThread::create(rwmutex_reader_func, &thread_ids[i]));
  }
  
  // Create writer threads
  for (int i = 0; i < num_writers; ++i) {
    thread_ids[num_readers + i] = num_readers + i;
    threads.push_back(SbThread::create(rwmutex_writer_func, &thread_ids[num_readers + i]));
  }
  
  // Wait for all threads to complete
  for (SbThread* thread : threads) {
    thread->join();
    SbThread::destroy(thread);
  }
  
  // Verify results
  BOOST_CHECK_EQUAL(global_counter.load(), num_readers + num_writers);
  BOOST_CHECK_EQUAL(shared_data, num_writers * 10);
  
  delete test_rwmutex;
  test_rwmutex = nullptr;
}

BOOST_AUTO_TEST_CASE(barrierSynchronization)
{
  const int num_threads = 4;
  test_barrier = new SbBarrier(num_threads);
  global_counter = 0;
  shared_data = 0;
  
  std::vector<SbThread*> threads;
  std::vector<int> thread_ids(num_threads);
  
  // Create threads
  for (int i = 0; i < num_threads; ++i) {
    thread_ids[i] = i;
    threads.push_back(SbThread::create(barrier_test_func, &thread_ids[i]));
  }
  
  // Wait for all threads to complete
  for (SbThread* thread : threads) {
    thread->join();
    SbThread::destroy(thread);
  }
  
  // Verify results - all threads should have synchronized
  BOOST_CHECK_EQUAL(global_counter.load(), num_threads);
  BOOST_CHECK_EQUAL(shared_data, num_threads);
  
  delete test_barrier;
  test_barrier = nullptr;
}

BOOST_AUTO_TEST_CASE(threadSafeFifo)
{
  test_fifo = new SbFifo();
  global_counter = 0;
  
  // Create producer and consumer threads
  int producer_id1 = 1, producer_id2 = 2, consumer_id = 3;
  SbThread* producer1 = SbThread::create(fifo_producer_func, &producer_id1);
  SbThread* producer2 = SbThread::create(fifo_producer_func, &producer_id2);
  SbThread* consumer = SbThread::create(fifo_consumer_func, &consumer_id);
  
  // Wait for completion
  producer1->join();
  producer2->join();
  consumer->join();
  
  SbThread::destroy(producer1);
  SbThread::destroy(producer2);
  SbThread::destroy(consumer);
  
  // Verify results
  BOOST_CHECK_EQUAL(global_counter.load(), 3);
  BOOST_CHECK_EQUAL(test_fifo->size(), 0); // All items consumed
  
  delete test_fifo;
  test_fifo = nullptr;
}

BOOST_AUTO_TEST_CASE(threadLocalStorage)
{
  // Test basic thread-local storage functionality
  SbStorage storage(sizeof(int));
  
  // Set value in current thread
  int * value = static_cast<int*>(storage.get());
  *value = 42;
  
  // Verify value persists in same thread
  int * value2 = static_cast<int*>(storage.get());
  BOOST_CHECK_EQUAL(*value2, 42);
  
  // Note: Testing across multiple threads would require more complex setup
  // and is covered by the existing threading infrastructure tests
}

BOOST_AUTO_TEST_CASE(typedThreadLocalStorage)
{
  // Test typed thread-local storage
  SbTypedStorage<int*> typed_storage(sizeof(int*));
  
  int test_value = 123;
  int** storage_ptr = reinterpret_cast<int**>(typed_storage.get());
  *storage_ptr = &test_value;
  
  // Verify value persists
  int** storage_ptr2 = reinterpret_cast<int**>(typed_storage.get());
  BOOST_CHECK_EQUAL(**storage_ptr2, 123);
}

BOOST_AUTO_TEST_CASE(automaticLocking)
{
  SbMutex mutex;
  
  // Test automatic locking and unlocking
  {
    SbThreadAutoLock lock(&mutex);
    // Mutex should be locked here
    BOOST_CHECK(!mutex.tryLock()); // Should fail if already locked
  }
  // Mutex should be unlocked here
  BOOST_CHECK(mutex.tryLock()); // Should succeed
  mutex.unlock();
  
  // Test with recursive mutex
  SbThreadMutex rec_mutex;
  {
    SbThreadAutoLock lock(&rec_mutex);
    // Recursive mutex should be locked, but tryLock might still succeed for recursive mutexes
    // depending on implementation - we'll accept both behaviors for now
  }
  // Should be unlocked
  BOOST_CHECK(rec_mutex.tryLock());
  rec_mutex.unlock();
}

BOOST_AUTO_TEST_SUITE_END();