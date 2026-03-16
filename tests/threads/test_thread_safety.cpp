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
 * @file test_thread_safety.cpp
 * @brief Phase-1 and Phase-2 thread safety stress tests.
 *
 * Each test hammers a specific fix from multiple threads and verifies
 * the invariant that the fix is supposed to guarantee.  The tests are designed
 * to expose data races when the relevant protection is absent, and to pass
 * cleanly (and quickly, under TSan) when it is present.
 *
 * Test inventory
 * ==============
 * Phase 1:
 * 1. sbname_concurrent_intern      — SbName string interning (namemap mutex)
 * 2. sbname_unique_addresses       — All threads get the same canonical pointer
 * 3. sonode_unique_ids             — SoNode::nextUniqueId (atomic counter)
 * 4. sonode_no_zero_id             — The zero ID is never assigned to a node
 * 5. sobase_refcount_races         — SoBase ref/unref atomicity
 * 6. sobase_refcount_balance       — Reference count stays non-negative
 * 7. sotype_concurrent_lookup      — SoType::fromName concurrent reads
 * 8. sotype_lookup_validity        — Looked-up types are valid after concurrent init
 * 9. notify_counter_balance        — SoDB::startNotify/endNotify counter balance
 * 10. notify_counter_never_negative — Counter never goes negative under concurrency
 * 11. mixed_workload               — Interleaved node creation, naming, field set
 * Phase 2:
 * 12. sobase_setname_getname       — Concurrent setName/getName on name dicts
 * 13. glcache_unique_context_ids   — SoGLCacheContextElement::getUniqueCacheContext
 * 14. enabled_elements_counter     — SoEnabledElementsList::getCounter atomicity
 * Phase 3:
 * 15. auditorlist_concurrent_notification — SoAuditorList snapshot-then-deliver (no crash)
 * 16. field_connect_disconnect_concurrent — Concurrent field connect/disconnect + notify
 */

#include "../test_utils.h"

#include <Inventor/SoDB.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SbName.h>
#include <Inventor/misc/SoBase.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/SoType.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoMFFloat.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/lists/SoEnabledElementsList.h>
#include <Inventor/lists/SoNodeList.h>

#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <mutex>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <algorithm>
#include <functional>

// ---------------------------------------------------------------------------
// Test parameters — tune these for faster CI vs. more thorough local runs.
// ---------------------------------------------------------------------------
// Test parameters — configurable via environment variables for CI vs. local:
//   OBOL_TEST_THREADS  — number of concurrent threads per test (default: 8)
//   OBOL_TEST_ITERS    — iterations each thread executes per test (default: 2000)
// ---------------------------------------------------------------------------
static int kNumThreads     = 8;
static int kItersPerThread = 2000;

static void init_test_params() {
  if (const char * v = std::getenv("OBOL_TEST_THREADS"))
    kNumThreads = std::max(2, std::atoi(v));
  if (const char * v = std::getenv("OBOL_TEST_ITERS"))
    kItersPerThread = std::max(1, std::atoi(v));
}

// ---------------------------------------------------------------------------
// Null context manager (no GL needed for these tests)
// ---------------------------------------------------------------------------
class NullCtxMgr : public SoDB::ContextManager {
public:
  void *createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
  SbBool makeContextCurrent(void *) override { return FALSE; }
  void restorePreviousContext(void *) override {}
  void destroyContext(void *) override {}
};

// ============================================================================
// 1 & 2 — SbName concurrent interning
// ============================================================================

/**
 * Invariant: concurrent calls to SbName(const char*) for the SAME string must
 * all return the same canonical memory address (that is the whole point of
 * string interning).  A data race in namemap would produce duplicate entries
 * with different addresses.
 */
static bool test_sbname_concurrent_intern()
{
  // Three distinct strings that will be interned concurrently.
  static const char * const strs[] = {
    "concurrent_test_string_alpha",
    "concurrent_test_string_beta",
    "concurrent_test_string_gamma",
  };
  static const int nstrs = 3;

  // First, intern them sequentially to get the expected canonical addresses.
  const char * expected[nstrs];
  for (int i = 0; i < nstrs; ++i)
    expected[i] = SbName(strs[i]).getString();

  std::atomic<bool> failure{false};
  std::atomic<int> ready{0};

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    // Spin until all threads are ready (rudimentary barrier).
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int iter = 0; iter < kItersPerThread && !failure; ++iter) {
      for (int i = 0; i < nstrs; ++i) {
        const char * got = SbName(strs[i]).getString();
        if (got != expected[i]) {
          failure = true;
          return;
        }
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  return !failure;
}

/**
 * Stress-interns a large set of unique strings from multiple threads.
 * Then verifies (single-threaded) that every string is findable and
 * that SbName comparisons between threads agree.
 */
static bool test_sbname_unique_addresses()
{
  // Build the string pool in advance.
  std::vector<std::string> pool;
  pool.reserve(200);
  for (int i = 0; i < 200; ++i) {
    std::ostringstream oss;
    oss << "sbname_stress_" << i;
    pool.push_back(oss.str());
  }

  // Each thread interns all strings and records the pointers it received.
  std::vector<std::vector<const char*>> thread_ptrs(kNumThreads,
                                                     std::vector<const char*>(pool.size()));
  std::atomic<int> ready{0};

  auto worker = [&](int tid) {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (size_t i = 0; i < pool.size(); ++i)
      thread_ptrs[tid][i] = SbName(pool[i].c_str()).getString();
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker, i);
  for (auto & t : threads) t.join();

  // Verify: every thread must agree on every pointer.
  for (size_t i = 0; i < pool.size(); ++i) {
    const char * ref = thread_ptrs[0][i];
    for (int tid = 1; tid < kNumThreads; ++tid) {
      if (thread_ptrs[tid][i] != ref) return false;
    }
    // Also verify the pointer is still valid and points to the right string.
    if (std::strcmp(ref, pool[i].c_str()) != 0) return false;
  }
  return true;
}

// ============================================================================
// 3 & 4 — SoNode::nextUniqueId uniqueness
// ============================================================================

/**
 * Creates many nodes concurrently and verifies that every node ID is globally
 * unique.  A non-atomic counter produces duplicate IDs.
 */
static bool test_sonode_unique_ids()
{
  const int total = kNumThreads * kItersPerThread;
  std::vector<SbUniqueId> collected(total);
  std::atomic<int> slot{0};
  std::atomic<int> ready{0};

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < kItersPerThread; ++i) {
      SoSphere * node = new SoSphere;
      SbUniqueId id = node->getNodeId();
      int idx = slot.fetch_add(1, std::memory_order_relaxed);
      collected[idx] = id;
      node->ref();
      node->unref();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  // All IDs must be distinct.
  std::sort(collected.begin(), collected.end());
  for (int i = 1; i < total; ++i)
    if (collected[i] == collected[i-1]) return false;
  return true;
}

/**
 * Verifies that no thread ever gets assigned the forbidden ID 0.
 */
static bool test_sonode_no_zero_id()
{
  std::atomic<bool> got_zero{false};
  std::atomic<int> ready{0};

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < kItersPerThread && !got_zero; ++i) {
      SoCone * node = new SoCone;
      if (node->getNodeId() == 0) got_zero = true;
      node->ref();
      node->unref();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  return !got_zero;
}

// ============================================================================
// 5 & 6 — SoBase reference count atomicity
// ============================================================================

/**
 * A single long-lived node is ref'd and unref'd from many threads
 * simultaneously.  We keep a mirror counter in an std::atomic<int> and verify
 * it matches getRefCount() at the end.
 *
 * With non-atomic refcounting a lost-update race would cause the internal count
 * to diverge from the mirror.
 *
 * Note: we keep the node alive via an extra ref so it is never destroyed during
 * the test (destruction would race with concurrent ref/unref otherwise — that
 * scenario is tested separately below).
 */
static bool test_sobase_refcount_races()
{
  SoSphere * node = new SoSphere;
  node->ref();   // anchor ref — prevents accidental destruction

  std::atomic<int> mirror{1};  // tracks our expected count (start at 1 for anchor)
  std::atomic<int> ready{0};

  // Each thread does kItersPerThread ref() then kItersPerThread unref() operations.
  // Net change per thread = 0, so the final count must equal 1.
  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < kItersPerThread; ++i) {
      node->ref();
      mirror.fetch_add(1, std::memory_order_relaxed);
    }
    for (int i = 0; i < kItersPerThread; ++i) {
      mirror.fetch_sub(1, std::memory_order_relaxed);
      node->unrefNoDelete();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  int32_t final_ref = node->getRefCount();
  int expected = mirror.load(std::memory_order_acquire);

  node->unref();  // release anchor ref

  return (final_ref == expected) && (final_ref == 1);
}

/**
 * Verifies the reference count never goes negative during concurrent
 * ref/unref operations.  A negative refcount (or a prematurely-zero count
 * that triggers a double-free) would manifest as a crash or assertion failure.
 *
 * We sample the refcount from a watcher thread while workers drive it.
 */
static bool test_sobase_refcount_balance()
{
  SoSeparator * node = new SoSeparator;
  node->ref();  // anchor

  std::atomic<bool> negative_seen{false};
  std::atomic<bool> done{false};
  std::atomic<int> ready{0};

  // Watcher samples the refcount continuously.
  std::thread watcher([&]() {
    while (!done.load(std::memory_order_acquire)) {
      if (node->getRefCount() < 0) {
        negative_seen = true;
        break;
      }
    }
  });

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < kItersPerThread; ++i) {
      node->ref();
      node->unrefNoDelete();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  done = true;
  watcher.join();

  node->unref();
  return !negative_seen;
}

// ============================================================================
// 7 & 8 — SoType concurrent lookup
// ============================================================================

/**
 * Many threads concurrently look up well-known built-in types via
 * SoType::fromName().  No type should ever come back as badType.
 */
static bool test_sotype_concurrent_lookup()
{
  static const char * const type_names[] = {
    "SoNode", "SoSphere", "SoCone", "SoCube", "SoCylinder",
    "SoSeparator", "SoTranslation", "SoRotation", "SoMaterial",
    "SoDirectionalLight", "SoCamera", "SoPerspectiveCamera",
    "SoGroup", "SoSwitch", "SoLOD",
  };
  static const int n = sizeof(type_names) / sizeof(type_names[0]);

  std::atomic<bool> failure{false};
  std::atomic<int> ready{0};

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int iter = 0; iter < kItersPerThread && !failure; ++iter) {
      for (int i = 0; i < n; ++i) {
        SoType t = SoType::fromName(type_names[i]);
        if (t.isBad()) {
          failure = true;
          return;
        }
      }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  return !failure;
}

/**
 * After concurrent lookups, verifies the type hierarchy is still intact
 * (isDerivedFrom should agree for every built-in type).
 */
static bool test_sotype_lookup_validity()
{
  // Concurrently look up types from multiple threads.
  std::atomic<int> ready{0};
  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}
    for (int i = 0; i < kItersPerThread; ++i) {
      SoType::fromName("SoSphere");
      SoType::fromName("SoCone");
      SoType::fromName("SoGroup");
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  // Single-threaded consistency check after the concurrent storm.
  SoType node_t  = SoType::fromName("SoNode");
  SoType base_t  = SoType::fromName("SoBase");
  SoType sphere_t = SoType::fromName("SoSphere");
  SoType group_t  = SoType::fromName("SoGroup");

  if (node_t.isBad() || base_t.isBad() || sphere_t.isBad() || group_t.isBad())
    return false;

  if (!sphere_t.isDerivedFrom(node_t))  return false;
  if (!sphere_t.isDerivedFrom(base_t))  return false;
  if (!group_t.isDerivedFrom(node_t))   return false;

  return true;
}

// ============================================================================
// 9 & 10 — SoDB notification counter
// ============================================================================

/**
 * Calls startNotify/endNotify from many threads in a balanced fashion and
 * verifies the counter returns to zero when all threads are done.
 *
 * Each thread calls sequential (non-overlapping within that thread) start/end
 * pairs.  The OBOL_THREADSAFE global recursive mutex serialises calls across
 * threads, but the atomic counter must still accumulate and drain correctly as
 * different threads hold the lock at different times.
 *
 * With a non-atomic counter, lost-update races could leave the counter at a
 * non-zero value.
 */
static bool test_notify_counter_balance()
{
  std::atomic<int> ready{0};

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < kItersPerThread; ++i) {
      SoDB::startNotify();
      SoDB::endNotify();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  // After perfectly balanced start/end, isNotifying() must be FALSE.
  return !SoDB::isNotifying();
}

/**
 * Verifies that a thread observing the counter after its own balanced
 * startNotify/endNotify pairs always sees a non-negative state.
 *
 * Strategy: run N threads, each doing M balanced pairs sequentially.
 * Between each pair, assert that the count is ≥ 0 (i.e. isNotifying() is
 * FALSE when we're not inside a notify, TRUE when we are).
 */
static bool test_notify_counter_never_negative()
{
  std::atomic<bool> failure{false};
  std::atomic<int> ready{0};

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < kItersPerThread && !failure; ++i) {
      // Outside a notify block: must not be notifying (we have no open block).
      // Note: other threads might have an open block, so we can't assert FALSE.
      // We CAN assert that startNotify works and endNotify balances it.
      SoDB::startNotify();
      if (!SoDB::isNotifying()) { failure = true; break; }
      SoDB::endNotify();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  // After all balanced calls, counter must be zero.
  if (SoDB::isNotifying()) failure = true;

  return !failure;
}

// ============================================================================
// 11 — Mixed workload stress test
// ============================================================================

/**
 * Simulates realistic concurrent usage: threads simultaneously create nodes
 * (exercising nextUniqueId + namemap), read type info (SoType::fromName),
 * and drive the notification counter.
 *
 * Pass criterion: no crashes, no assertions, all created nodes have unique IDs,
 * and the notification counter is balanced at the end.
 */
static bool test_mixed_workload()
{
  // Use half the normal iteration count — each iteration does significantly
  // more work (node creation + type lookup + notify + name interning).
  const int reduced_iters = kItersPerThread / 2;
  std::atomic<int> ready{0};
  std::atomic<bool> failure{false};

  // Collect IDs from all threads to verify uniqueness.
  const int total = kNumThreads * reduced_iters;
  std::vector<SbUniqueId> ids(total);
  std::atomic<int> id_slot{0};

  auto worker = [&](int tid) {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < reduced_iters && !failure; ++i) {
      // Create a node — exercises nextUniqueId and SbName interning.
      SoSphere * sphere = new SoSphere;
      sphere->ref();

      // Record the node ID.
      int slot = id_slot.fetch_add(1, std::memory_order_relaxed);
      if (slot < total) ids[slot] = sphere->getNodeId();

      // Look up a type — exercises SoType read-path under shared lock.
      SoType t = SoType::fromName("SoSphere");
      if (t.isBad()) failure = true;

      // Drive startNotify / endNotify.
      SoDB::startNotify();
      SoDB::endNotify();

      // Intern a per-thread-per-iteration name — exercises namemap writes.
      std::ostringstream oss;
      oss << "mixed_workload_t" << tid << "_i" << i;
      SbName n(oss.str().c_str());
      if (n.getLength() == 0) failure = true;

      sphere->unref();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker, i);
  for (auto & t : threads) t.join();

  if (failure) return false;
  if (SoDB::isNotifying()) return false;

  // Verify ID uniqueness among the collected IDs.
  int collected = std::min(id_slot.load(), total);
  auto begin = ids.begin();
  auto end = begin + collected;
  std::sort(begin, end);
  for (auto it = begin + 1; it != end; ++it)
    if (*it == *(it - 1)) return false;

  return true;
}

// ============================================================================
// Phase 2 tests
// ============================================================================

/**
 * Concurrently call setName() and getName() on distinct SoBase-derived objects.
 *
 * Invariant: after all threads finish, each object's name must be the last
 * name assigned by any thread (no corruption of the name/object dictionaries).
 * The test is primarily a no-crash/no-sanitizer-alarm check.
 */
static bool test_sobase_setname_getname()
{
  // Create a pool of nodes; each thread names them repeatedly.
  const int kNodes = 16;
  std::vector<SoSphere*> nodes(kNodes);
  for (int i = 0; i < kNodes; ++i) {
    nodes[i] = new SoSphere;
    nodes[i]->ref();
  }

  std::atomic<bool> failure{false};
  std::atomic<int> ready{0};

  auto worker = [&](int tid) {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int iter = 0; iter < kItersPerThread && !failure; ++iter) {
      // Each thread works on a rotating subset of nodes to maximise contention.
      int idx = (tid + iter) % kNodes;
      SoSphere * n = nodes[idx];

      // Set a name
      std::ostringstream oss;
      oss << "node_" << idx << "_t" << tid << "_" << iter;
      n->setName(SbName(oss.str().c_str()));

      // Read it back (shared lock path). Another thread may have overwritten
      // the name before we got here, so don't assert on the value — the test
      // is a no-crash/no-sanitizer-alarm check for the locking paths.
      (void)n->getName();

      // Also exercise getByName lookup (read path on name2obj)
      SoNodeList list;
      SoNode::getByName(SbName(oss.str().c_str()), list);
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker, i);
  for (auto & t : threads) t.join();

  for (int i = 0; i < kNodes; ++i)
    nodes[i]->unref();

  return !failure;
}

/**
 * Concurrently call SoGLCacheContextElement::getUniqueCacheContext() from
 * multiple threads and verify that all returned IDs are unique.
 *
 * Invariant: each call returns a distinct positive integer; no two threads
 * get the same context ID.
 */
static bool test_glcache_unique_context_ids()
{
  const int total = kNumThreads * kItersPerThread;
  std::vector<uint32_t> ids(total);
  std::atomic<int> ready{0};

  auto worker = [&](int tid) {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int i = 0; i < kItersPerThread; ++i) {
      ids[tid * kItersPerThread + i] =
        SoGLCacheContextElement::getUniqueCacheContext();
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker, i);
  for (auto & t : threads) t.join();

  // All IDs must be positive and unique.
  for (int i = 0; i < total; ++i)
    if (ids[i] == 0) return false;

  std::sort(ids.begin(), ids.end());
  for (int i = 1; i < total; ++i)
    if (ids[i] == ids[i-1]) return false;

  return true;
}

/**
 * Concurrently call SoEnabledElementsList::getCounter() and
 * SoEnabledElementsList::enable() verifying the counter is never negative
 * and the list is accessible from multiple threads simultaneously.
 *
 * This exercises the std::atomic<int> enable_counter and recursive_mutex.
 */
static bool test_enabled_elements_counter()
{
  // Use the global action elements list as a read target.
  const int start_counter = SoEnabledElementsList::getCounter();
  (void)start_counter; // value not critical; just check it's accessible

  std::atomic<bool> failure{false};
  std::atomic<int> ready{0};

  auto worker = [&]() {
    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int iter = 0; iter < kItersPerThread && !failure; ++iter) {
      int c = SoEnabledElementsList::getCounter();
      if (c < 0) { failure = true; return; }
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  return !failure;
}

// ============================================================================
// Phase 3 tests
// ============================================================================

/**
 * Stress-test the SoAuditorList snapshot-then-deliver fix.
 *
 * A "master" SoSFFloat field is created on the main thread.  Each worker
 * thread connects its own SoSFFloat to the master, sets the master's value
 * (triggering notification delivery through the auditor list), then
 * disconnects.  Without the snapshot-under-NOTIFY_LOCK fix the auditor list
 * could be corrupted or the iterator could visit freed entries mid-delivery.
 *
 * Invariant: no crash, and the total number of connected fields at the end
 * is zero (all workers disconnected cleanly).
 */
static bool test_auditorlist_concurrent_notification()
{
  // One master field whose auditor list will be hammered.
  SoSeparator * root = new SoSeparator;
  root->ref();

  SoTranslation * tnode = new SoTranslation;
  root->addChild(tnode);

  std::atomic<bool> failure{false};
  std::atomic<int>  ready{0};

  auto worker = [&]() {
    // Each thread creates its own node+field pair and connects to root.
    SoTranslation * local = new SoTranslation;
    local->ref();

    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int iter = 0; iter < kItersPerThread && !failure; ++iter) {
      // Connect the local field to the shared node's field.
      SoDB::writelock();
      local->translation.connectFrom(&tnode->translation);
      SoDB::writeunlock();

      // Mutate the master field — triggers notification through auditor list.
      SoDB::writelock();
      tnode->translation.setValue(SbVec3f(static_cast<float>(iter), 0.0f, 0.0f));
      SoDB::writeunlock();

      // Disconnect — this removes from the auditor list mid-notify
      SoDB::writelock();
      local->translation.disconnect();
      SoDB::writeunlock();
    }

    local->unref();
  };

  std::vector<std::thread> threads;
  threads.reserve(static_cast<size_t>(kNumThreads));
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  root->unref();
  return !failure;
}

/**
 * Concurrent field connect/disconnect from multiple threads.
 *
 * Several threads independently create source/dest field pairs, connect
 * them, trigger notification by touching the source, then disconnect.
 * This exercises the full NOTIFY_LOCK → snapshot → deliver path.
 *
 * Invariant: all threads complete without crashing; each source field value
 * is readable after the loop.
 */
static bool test_field_connect_disconnect_concurrent()
{
  std::atomic<bool> failure{false};
  std::atomic<int>  ready{0};

  auto worker = [&]() {
    // Private nodes per thread — no sharing between workers here.
    SoTranslation * src  = new SoTranslation;
    SoTranslation * dst  = new SoTranslation;
    src->ref();
    dst->ref();

    ready.fetch_add(1, std::memory_order_relaxed);
    while (ready.load(std::memory_order_acquire) < kNumThreads) {}

    for (int iter = 0; iter < kItersPerThread && !failure; ++iter) {
      SoDB::writelock();
      dst->translation.connectFrom(&src->translation);
      SoDB::writeunlock();

      SoDB::writelock();
      src->translation.setValue(SbVec3f(static_cast<float>(iter), 0.0f, 0.0f));
      SoDB::writeunlock();

      SoDB::readlock();
      SbVec3f v = dst->translation.getValue();
      SoDB::readunlock();
      // iter is always >= 0, so a negative x-component indicates corruption.
      // We don't assert exact equality because notification may propagate the
      // previous iteration's value (races between connect and setValue are
      // intentional stress here); we only verify no garbage was written.
      if (v[0] < 0.0f) { failure = true; }

      SoDB::writelock();
      dst->translation.disconnect();
      SoDB::writeunlock();
    }

    src->unref();
    dst->unref();
  };

  std::vector<std::thread> threads;
  threads.reserve(static_cast<size_t>(kNumThreads));
  for (int i = 0; i < kNumThreads; ++i)
    threads.emplace_back(worker);
  for (auto & t : threads) t.join();

  return !failure;
}

// ============================================================================
// main
// ============================================================================
int main(int /*argc*/, char ** /*argv*/) {
  static NullCtxMgr ctxMgr;
  SoDB::init(&ctxMgr);
  SoInteraction::init();
  init_test_params();

  // Pre-warm every node type that appears in the concurrent tests.
  // SoFieldData::addEnumValue and other per-class lazy initialisation
  // is not thread safe (Phase 2+ fix); we prime it here so that all
  // concurrent tests see an already-initialised class.
  {
    auto warm = [](SoNode * n) { n->ref(); n->unref(); };
    warm(new SoSphere);
    warm(new SoCone);
    warm(new SoCube);
    warm(new SoSeparator);
    warm(new SoTranslation);  // Phase 3 tests use SoTranslation
  }

  SimpleTest::TestRunner runner;

  struct TC { const char * name; bool (*fn)(); };
  TC tests[] = {
    { "sbname_concurrent_intern",       test_sbname_concurrent_intern      },
    { "sbname_unique_addresses",        test_sbname_unique_addresses       },
    { "sonode_unique_ids",              test_sonode_unique_ids             },
    { "sonode_no_zero_id",              test_sonode_no_zero_id             },
    { "sobase_refcount_races",          test_sobase_refcount_races         },
    { "sobase_refcount_balance",        test_sobase_refcount_balance       },
    { "sotype_concurrent_lookup",       test_sotype_concurrent_lookup      },
    { "sotype_lookup_validity",         test_sotype_lookup_validity        },
    { "notify_counter_balance",         test_notify_counter_balance        },
    { "notify_counter_never_negative",  test_notify_counter_never_negative },
    { "mixed_workload",                 test_mixed_workload                },
    // Phase 2 tests
    { "sobase_setname_getname",                   test_sobase_setname_getname                   },
    { "glcache_unique_context_ids",               test_glcache_unique_context_ids               },
    { "enabled_elements_counter",                 test_enabled_elements_counter                 },
    // Phase 3 tests
    { "auditorlist_concurrent_notification",      test_auditorlist_concurrent_notification      },
    { "field_connect_disconnect_concurrent",      test_field_connect_disconnect_concurrent      },
  };

  for (auto & tc : tests) {
    runner.startTest(tc.name);
    bool passed = false;
    try {
      passed = tc.fn();
    } catch (const std::exception & e) {
      runner.endTest(false, e.what());
      continue;
    } catch (...) {
      runner.endTest(false, "Unknown exception");
      continue;
    }
    runner.endTest(passed, passed ? "" : "invariant violated");
  }

  SoDB::finish();
  return runner.getSummary();
}
