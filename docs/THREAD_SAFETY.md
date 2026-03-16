# Thread Safety Analysis for Obol

## Overview

Obol inherits its scene-graph architecture from Coin3D, which was historically
documented as not thread safe for general use.  This document provides a deep
analysis of *why* Obol is not thread safe today, categorises every blocker,
assesses whether thread safety is theoretically achievable, and outlines the
work required to reach that goal.

The short answer is: **complete thread safety is achievable, but it requires
significant and carefully sequenced work across several independent subsystems.**
A number of subsystems already carry locking scaffolding (guarded by
`OBOL_THREADSAFE`) — the remaining gaps are well-defined and fixable.

---

## Definitions

The analysis uses the following threat model:

| Scenario | Description |
|---|---|
| **Init-then-read** | `SoDB::init()` is called once on thread A; afterwards multiple threads only *read* the scene graph (no mutations). |
| **Concurrent render** | Multiple threads each own an independent GL context and traverse independent scene graphs simultaneously. |
| **Concurrent mutation** | Multiple threads create, modify, or connect scene-graph nodes and fields simultaneously. |
| **Mixed read/write** | One or more threads traverse while others mutate. |

The strongest guarantee Obol should eventually offer is *Mixed read/write* with
`SoDB::readlock()` / `SoDB::writelock()` protecting mutations, and *Concurrent
render* without any application-level locking when each thread owns its own GL
context.

---

## Current State: What Obol Already Does

Several subsystems already have correct or near-correct locking, always compiled
in when `OBOL_THREADSAFE` is defined (the default, per `CMakeLists.txt`).

### 1. Scene-graph traversal — `SoDB` read/write mutex

`src/misc/SoDBP.h` declares a global `SbRWMutex * globalmutex` that is
allocated in `SoDB::init()` when `OBOL_THREADSAFE` is set.  `SoAction::apply()`
always calls `SoDB::readlock()` before traversing and `SoDB::readunlock()`
afterward.  Write-locking is left to the application when mutating the graph.

*Status:* Correct for the single-scene-graph case; does **not** protect other
global state described below.

### 2. Field notification — global recursive mutex

`src/threads/recmutex_global_cxx17.cpp` provides two process-wide recursive
mutexes, one for field operations and one for notification chains:

```
cc_recmutex_cxx17_field_lock()   / cc_recmutex_cxx17_field_unlock()
cc_recmutex_cxx17_notify_lock()  / cc_recmutex_cxx17_notify_unlock()
```

`SoField` uses `SOFIELD_RECLOCK` / `SOFIELD_RECUNLOCK` (which map to the field
mutex) around value operations.  `SoDB::startNotify()` / `SoDB::endNotify()`
acquire the notify mutex around notification traversals.

*Status:* Protects the hot path of field evaluation and notification.  See the
gaps listed in §5 below.

### 3. Sensor queues — per-queue `SbMutex`

`SoSensorManager` allocates four separate `SbMutex` objects (timer, delay,
immediate, reschedule) and uses `LOCK_*_QUEUE` / `UNLOCK_*_QUEUE` macros around
every enqueue and dequeue operation.

*Status:* Correct.

### 4. GL texture cache — shared `SbMutex`

`SoGLImageP::mutex` is a single shared mutex that serialises all
`getGLDisplayList()` calls across all `SoGLImage` instances.  `SoGLBigImage`
uses a per-instance `SbMutex`.

*Status:* Correct within each class; does not protect the unrelated GL context
list described below.

### 5. Per-`SoSeparator` render-cache mutex

Each `SoSeparator` private implementation carries an `SbMutex` that serialises
`SoGLCacheList` creation and invalidation.  GL cache storage is per-thread via
`SbStorage`, so caches themselves are thread-local.

*Status:* Correct.

### 6. Normal-cache mutex — `SoVertexShape`

`SoVertexShapeP::normalcachemutex` is a global `SbRWMutex` protecting shared
generation of vertex normals.

*Status:* Correct.

### 7. Thread-local state — `SbStorage` / `thread_local`

`SbStorage` (backed by `std::unordered_map` and a `std::shared_mutex` registry)
provides per-thread data.  Its registry is correctly protected.  Several
subsystems use it for per-thread GL cache pointers, big-image tiles, color
packers, texture-coordinate data, and copy-map dictionaries.  `SoGL.cpp`
declares a `thread_local` GL-context pointer for the current render context.

*Status:* Correct.

### 8. `SoProtoInstance` and `CoinTidbits` atexit list

Both use a `std::mutex` (unconditionally) around their respective global
structures.

*Status:* Correct.

### 9. Debug thread check — `OBOL_CHECK_THREAD`

When the CMake option `OBOL_DEBUG_CHECK_THREAD` is enabled, `OBOL_CHECK_THREAD()`
compares `std::this_thread::get_id()` against the thread that called
`SoDB::init()` and logs a `SoDebugError` if they differ.  This is a diagnostic
tool, not a safety mechanism; it emits a warning but does not prevent the race.

*Status:* Useful for development; not a fix.

---

## Current State: What Is Still Broken

The following subsystems have either no locking or insufficient locking.

### Blocker 1 — `SbName` / `namemap`: unprotected string interning *(CRITICAL)*

**File:** `src/base/namemap.cpp`

`SbName` is used pervasively — every type name, field name, and node name goes
through it.  Internally, `namemap_find_or_add_string()` operates on two global,
unprotected data structures:

```c
static NamemapBucketEntry ** nametable;  // hash table of name → entry
static NamemapMemChunk     * headchunk;  // slab allocator for strings
```

Two threads calling `SbName::SbName(const char*)` simultaneously can:

- Corrupt the linked-list buckets (`entry->next = nametable[i]; nametable[i] = entry;` is not atomic).
- Corrupt the slab allocator (both threads decrement `bytesleft` and advance `curbyte` without synchronization, causing overlapping writes).
- Create duplicate entries (both threads observe the entry as absent and insert it twice).

Because `SbName` is used to look up types, field names, and node names, any
corruption here can silently produce wrong type matches or crashes anywhere in
the library.

**Fix required:** Add a `std::mutex` (or a `std::shared_mutex` for read-heavy
loads) protecting `namemap_find_or_add_string()`.  Because `cc_namemap_peek_string()`
is read-only, it qualifies for a shared (reader) lock.

---

### Blocker 2 — `SoType` registration: unprotected type dictionary *(CRITICAL)*

**File:** `src/misc/SoType.cpp`

The type system manages three global data structures, none protected by any mutex:

```cpp
static Name2IdMap     * type_dict    = NULL;  // name → type index
static Name2HandleMap * module_dict  = NULL;  // for dynamic loading
SbList<SoTypeData *>  * typedatalist = NULL;  // indexed type data (class member)
```

`SoType::createType()` performs a read followed by a conditional append:

```cpp
if (type_dict->get(name, discard)) { /* already registered */ return; }
newType.index = SoType::typedatalist->getLength();   // read length
SoType::typedatalist->append(typeData);              // append
type_dict->put(name, newType.getKey());              // insert
```

Two threads registering different types simultaneously can corrupt both
`typedatalist` (wrong indices) and `type_dict` (duplicate or missing entries).
Dynamic loading in `internal_fromName()` compounds this: it touches `module_dict`
and falls through into `createType()` with no lock held.

**Fix required:** A single `std::mutex` protecting all of `createType()`,
`deregisterType()`, `internal_fromName()`, and any `typedatalist` access.
Type lookups (`fromName()`, `getAllDerivedFrom()`) need at least a shared lock.

---

### Blocker 3 — `SoBase` reference count: non-atomic bit-field *(CRITICAL)*

**File:** `include/Inventor/misc/SoBase.h`

The reference count is stored in a bit-packed struct:

```cpp
struct {
  mutable signed int  referencecount : 28;
  mutable unsigned int alive         :  4;
} objdata;
```

`ref()` and `unref()` perform non-atomic read-increment-write sequences:

```cpp
// SoBase.cpp, SoBase::ref():
this->objdata.referencecount++;
```

```cpp
// SoBase.cpp, SoBase::unref():
this->objdata.referencecount--;
int refcount = this->objdata.referencecount;
if (refcount == 0) base->destroy();
```

The classic lost-update race applies: two threads calling `ref()` on the same
object simultaneously can both read the same value, both increment it, and write
back the same incremented value — dropping a reference count.  For `unref()`,
both threads can read a count of 1, both decrement to 0, and both call
`destroy()`, causing a double-free.

The C++ standard does not guarantee atomic access to bit-fields, even on
architectures with native word-size operations.

**Fix required:** The bit-packing cannot be preserved — C++ provides no atomic
operations on bit-fields.  The replacement trades 4 bytes per `SoBase` instance
for correctness: replace `referencecount:28` with a standalone
`std::atomic<int32_t>` (4 bytes), and promote `alive:4` to a separate
`std::atomic<uint8_t>` or a plain `uint8_t` protected by a compare-exchange on
the reference count.  `SoBase` instances are heap-allocated and already carry a
virtual-function table pointer (8 bytes on 64-bit), so the 4-byte growth of the
`objdata` struct is not a practical concern.  Use `std::memory_order_acq_rel` for
increment/decrement and `std::memory_order_acquire` for the zero-check.

---

### Blocker 4 — `SoNode::nextUniqueId`: non-atomic global counter *(CRITICAL)*

**File:** `src/nodes/SoNode.cpp`

Every node creation, modification, and copy stamps a unique ID via:

```cpp
SbUniqueId SoNode::nextUniqueId = 1;  // global, unprotected

#define SET_UNIQUE_NODE_ID(obj)                   \
  (obj)->uniqueId = SoNode::nextUniqueId++;       \
  if (obj->uniqueId == 0) {                       \
    obj->uniqueId = SoNode::nextUniqueId++;       \
  }
```

`SbUniqueId` is a plain integer type (`uint32_t` or `uint64_t`).
`nextUniqueId++` is **not** atomic in C++.  Two threads creating nodes
simultaneously can receive the same unique ID, which breaks the change-detection
caches (VBO validation, bounding-box caches, etc.) used throughout rendering.

`SbUniqueId` is platform-configurable: `uint64_t` by default, or `uint32_t`
when `OBOL_UNIQUE_ID_UINT32` is set (see `include/Inventor/basic.h.cmake.in`).
Both widths are natively lock-free atomics on all platforms Obol targets.

**Fix required:** Change `nextUniqueId` to `std::atomic<SbUniqueId>` and use
`fetch_add(1, std::memory_order_relaxed)` (ordering is not required for ID
uniqueness — only for the zero-skip check, which can use a local compare).
The fix is identical for both the 32-bit and 64-bit variants because
`std::atomic<uint32_t>` and `std::atomic<uint64_t>` are both always lock-free
on x86-64 and ARM64.

---

### Blocker 5 — `SoBase::PImpl` global name/auditor dictionaries ✅ **(Fixed)**

**File:** `src/misc/SoBaseP.cpp`, `src/misc/SoBaseP.h`

Four global data structures were accessed without synchronization.  All four
are now protected by `SoBase::PImpl::base_dict_mutex` (`std::shared_mutex`)
declared in `SoBaseP.h` and defined in `SoBaseP.cpp`.  Shared (read) lock for
`getName()`, `getNamedBase()`, `getNamedBases()`; exclusive (write) lock for
`setName()`, `getAuditors()`, and the constructor/destructor tracking.

---

### Blocker 6 — `SoGLCacheContextElement` static state ✅ **(Fixed)**

**File:** `src/elements/GL/SoGLCacheContextElement.cpp`

`biggest_cache_context_id` is now `std::atomic<int>` with CAS-loop updates in
`set()` and `fetch_add(1)` in `getUniqueCacheContext()`.  A `std::mutex
glcache_list_mutex` protects all three `SbList` containers.

---

### Blocker 7 — `SoDBP::notificationcounter`: non-atomic plain `int` ✅ **(Fixed in Phase 1)**

**File:** `src/misc/SoDBP.h` and `src/misc/SoDB.cpp`

Changed to `std::atomic<int>` in Phase 1.

---

### Blocker 8 — `SoActionMethodList` and `SoEnabledElementsList`: conditional locking ✅ **(Fixed)**

**Files:** `src/lists/SoActionMethodList.cpp`, `src/lists/SoEnabledElementsList.cpp`

`SoActionMethodList` now uses `std::shared_mutex` (removing `OBOL_THREADSAFE`
guard).  A new `getMethod(int)` function provides a shared-lock read path;
`SoAction::traverse()` uses it instead of `operator[]` for the traversal lookup.
`SoEnabledElementsList` uses `std::recursive_mutex` for per-instance locking
and `std::atomic<int>` for `enable_counter`; `enable()` and `merge()` now take
the per-instance lock.

---

### Blocker 9 — `SoAuditorList` iteration during concurrent modification ✅ **(Fixed)**

**File:** `src/lists/SoAuditorList.cpp`

`SoAuditorList` is the linked notification mechanism between fields, sensors,
and nodes.  The previous implementation iterated the live list inside
`notify()` — if a callback added or removed an auditor mid-delivery, the
iterator would walk freed or newly-inserted entries.  An `assert` at the end
of the loop would crash debug builds any time an auditor was removed during
notification.

**Fix applied:**

1. **True snapshot-then-deliver** — `notify()` now takes the global notify
   recursive mutex (`NOTIFY_LOCK`), copies all `(auditor, type)` pairs into a
   `std::vector`, releases the lock, and then delivers notifications from the
   snapshot.  Callbacks can freely add or remove auditors; changes affect
   the live list but not the in-progress snapshot.
2. **Unconditional locking** — The `NOTIFY_LOCK` / `NOTIFY_UNLOCK` macros in
   `append()`, `set()`, and `remove()` are now always active; the
   `#ifdef OBOL_THREADSAFE` guard has been removed.
3. **Dead assert removed** — The `assert(num == this->getLength())` that
   would crash debug builds when auditors were removed during notification
   has been removed; it is no longer needed because we iterate the snapshot,
   not the live list.

---

### Blocker 10 — `SbHash<>` and `SbList<>` are not thread-safe containers *(PERVASIVE)*

**Files:** `src/misc/SbHash.h`, `include/Inventor/lists/SbList.h`

`SbHash` is an open-addressing hash map backed by a plain C array.  `SbList` is
a growable array backed by a `new[]`-allocated block.  Neither provides any
internal synchronization.  They are used as containers for the global state in
blockers 1–6 above.

Fixing the individual blockers (by wrapping access with appropriate mutexes) is
sufficient; replacing these containers with lock-free or concurrent variants is
not required but would improve scalability.

---

## Design-level Issues: Not Easily Fixed by Locking Alone

The following problems require design changes, not just added mutexes.

### D1 — OpenGL's per-context single-thread model ✅ **(Detection implemented)**

OpenGL contexts are implicitly associated with exactly one thread at a time
(`glXMakeCurrent`, `wglMakeCurrent`).

**Fix applied:** `SoGLCacheContextElement::set()` now records the
`std::thread::id` of the first thread that uses each cache-context ID in a
per-process `std::unordered_map<int, std::thread::id>` guarded by
`glcache_list_mutex`.  Any subsequent `set()` call for the same context from
a *different* thread emits a `SoDebugError::postWarning()` message.
`cleanupContext()` erases the per-context record when the GL context is
destroyed.

This is a detection mechanism, not a hard lock: the warning fires early enough
for debugging but does not block cross-thread access that is intentional (e.g.
explicit `glXMakeCurrent` migration).  The correct architecture remains:
**each rendering thread owns exactly one GL context, and GL objects are never
migrated between contexts.**  Obol's existing per-thread GL cache storage
(`SbStorage` in `SoSeparator`) is already aligned with this model.

### D2 — Re-entrant notification during traversal ✅ **(Fixed — see Blocker 9)**

`SoDB::startNotify()` acquires the notify recursive mutex, then walks the
auditor list, which can trigger sensor callbacks, which in turn may modify
fields, which calls `startNotify()` again (allowed by the recursive mutex).

**Fix applied:** The snapshot-then-deliver strategy implemented for Blocker 9
(see above) resolves this class of problem.  `SoAuditorList::notify()` takes
the notify lock to copy all auditors, releases it, then delivers to the
snapshot.  Re-entrant `startNotify()` calls from within a callback still
acquire the (recursive) notify lock safely; they can modify the live list
without corrupting the outer delivery pass.

### D3 — `SoState` per-action, not per-thread

`SoState` is created and owned by a single `SoAction` instance and carries all
element stacks.  It is inherently single-threaded: no part of the traversal
machinery assumes concurrent access to the same `SoState`.

For *Concurrent render* (multiple threads each traversing their own independent
scene graph), this is already safe because each thread creates its own action
and thus its own `SoState`.  This design constraint simply means that one
`SoAction` cannot be shared between threads, which is documented behaviour.

### D4 — Static `SbStorage` across lifetime boundaries ✅ **(Fixed)**

`SbStorage` registers all per-thread storage objects in a global registry so
that thread-exit cleanup can walk all live storages and release each thread's
slot.  The registry itself is protected by a `std::shared_mutex` (see
`src/threads/storage_cxx17.h`).

**Fix applied:** `StorageRegistry::cleanupThread()` now takes a *snapshot* of
`registered_storages` into a local `std::vector` under the shared lock, then
releases the lock before iterating.  This avoids a potential deadlock where a
storage destructor calls `unregisterStorage()` (which takes the exclusive lock)
while `cleanupThread()` is still iterating with the shared lock held.

---

## Summary Table

| # | Subsystem | File(s) | Severity | Existing Lock | Gap |
|---|---|---|---|---|---|
| 1 | `SbName` interning | `base/namemap.cpp` | **CRITICAL** | ✅ `std::mutex` | Fixed (Phase 1) |
| 2 | `SoType` registration | `misc/SoType.cpp` | **CRITICAL** | ✅ `std::shared_mutex` | Fixed (Phase 1) |
| 3 | `SoBase` refcount | `misc/SoBase.h/.cpp` | **CRITICAL** | ✅ `std::atomic<int32_t>` | Fixed (Phase 1) |
| 4 | `SoNode::nextUniqueId` | `nodes/SoNode.cpp` | **CRITICAL** | ✅ `std::atomic<SbUniqueId>` | Fixed (Phase 1) |
| 5 | `SoBase::PImpl` dicts | `misc/SoBaseP.cpp/.h` | **CRITICAL** | ✅ `std::shared_mutex` | Fixed (Phase 2) |
| 6 | GL context element statics | `elements/GL/SoGLCacheContextElement.cpp` | **HIGH** | ✅ `std::atomic` + `std::mutex` | Fixed (Phase 2) |
| 7 | Notification counter | `misc/SoDBP.h/.cpp` | **MEDIUM** | ✅ `std::atomic<int>` | Fixed (Phase 1) |
| 8 | Action/element method lists | `lists/SoAction*.cpp`, `lists/SoEnabled*.cpp` | **MEDIUM** | ✅ `std::shared_mutex` | Fixed (Phase 2) |
| 9 | `SoAuditorList` iteration | `lists/SoAuditorList.cpp` | **MEDIUM** | ✅ snapshot-then-deliver | Fixed (Phase 3) |
| 10 | `SbHash` / `SbList` | `misc/SbHash.h`, `lists/SbList.h` | **PERVASIVE** | Protected by caller locks | No additional lock needed |
| D1 | GL context / thread binding | `elements/GL/SoGLCacheContextElement.cpp` | Design | ✅ `SoDebugError` warning | Detection implemented (Phase 3) |
| D2 | Re-entrant notification | `misc/SoDB.cpp`, `fields/SoField.cpp` | Design | ✅ Recursive mutex + snapshot | Fixed (Phase 3, see Blocker 9) |
| D3 | `SoState` per-action | Traversal machinery | Design | N/A | Documented: do not share actions between threads |
| D4 | `StorageRegistry` iteration | `threads/storage_cxx17.*` | Design | ✅ snapshot under lock | Fixed (Phase 3) |

---

## Recommended Migration Strategy

The blockers fall into three natural phases, ordered by dependency and risk.

### Phase 1 — Eliminate data races in global infrastructure ✅ **(Complete)**

1. **`SoNode::nextUniqueId` → `std::atomic<SbUniqueId>`** — Zero application
   impact.  Pure mechanical change.

2. **`SbName`/`namemap` mutex** — `static std::mutex namemap_mutex` inside
   `namemap.cpp` protects `namemap_find_or_add_string()`.

3. **`SoBase` refcount → `std::atomic<int32_t>`** — Split `objdata` into a
   separate `uint8_t alive` marker.  `ref()`, `unref()`, and `unrefNoDelete()`
   use `fetch_add`/`fetch_sub` with appropriate memory ordering.

4. **`SoType` registration mutex** — `static std::shared_mutex type_mutex`
   in `SoType.cpp`.  Writer-lock in `createType()` and `deregisterType()`;
   reader-lock in `fromName()`, `getAllDerivedFrom()`, etc.

5. **`SoDBP::notificationcounter` → `std::atomic<int>`** — Mechanical.
   The existing notify recursive mutex serialises the notification pass;
   the atomic counter uses relaxed ordering for increment/decrement and
   acquire/release for the zero-check.

### Phase 2 — Protect global object registries ✅ **(Complete)**

6. **`SoBase::PImpl` dicts** — added `static std::shared_mutex base_dict_mutex`
   in `SoBaseP.h`/`SoBaseP.cpp`.  Shared (read) lock for `getName()`,
   `getNamedBase()`, `getNamedBases()`; exclusive (write) lock for `setName()`
   (reads old name directly from `obj2name` to avoid recursive lock),
   `getAuditors()` (lazy-initialises `auditordict` entries), and `allbaseobj`
   / `auditordict` tracking in the constructor and destructor.

7. **`SoGLCacheContextElement` statics** — `std::atomic<int>` for
   `biggest_cache_context_id`; `std::mutex glcache_list_mutex` around all
   three `SbList` operations (`extsupportlist`, `scheduledeletelist`,
   `scheduledeletecblist`).  `set()` uses a CAS loop; `getUniqueCacheContext()`
   uses `fetch_add(1)`.

8. **`SoActionMethodList` / `SoEnabledElementsList`** — `SoActionMethodList`
   upgraded from `SbMutex` to `std::shared_mutex` (removing `OBOL_THREADSAFE`
   guard); added `getMethod(int)` with shared lock for the traversal read path;
   `SoAction::traverse()` updated to use `getMethod()`.
   `SoEnabledElementsList` uses `std::recursive_mutex` (allowing re-entrant
   calls from `getElements()` → `merge()` → `enable()`); `enable_counter`
   changed to `static std::atomic<int>`; explicit locks added to `enable()`
   and `merge()`.

### Phase 3 — Design-level work ✅ **(Complete)**

9. **`SoAuditorList` snapshot iteration** — `notify()` now takes the global
   notify recursive mutex (`NOTIFY_LOCK`), copies all `(auditor, type)` pairs
   into a `std::vector`, releases the lock, then delivers to the snapshot.
   The `#ifdef OBOL_THREADSAFE` guard around `NOTIFY_LOCK`/`NOTIFY_UNLOCK` in
   `append()`, `set()`, and `remove()` has been removed (locking is now
   unconditional).  The `assert(num == this->getLength())` that would crash
   debug builds when auditors were removed during notification has been
   removed.

10. **`StorageRegistry::cleanupThread` — snapshot under lock** — Takes a copy
    of `registered_storages` into a local `std::vector` under the shared
    registry lock, releases the lock, then iterates the snapshot.  This
    eliminates a potential deadlock where a storage destructor calls
    `unregisterStorage()` (exclusive lock) while `cleanupThread()` iterates
    with the shared lock held.

11. **GL context/thread binding detection** — `SoGLCacheContextElement::set()`
    now records the `std::thread::id` of the first thread that uses each
    cache-context ID in a `context_thread_map` guarded by `glcache_list_mutex`.
    A subsequent `set()` call from a different thread emits a
    `SoDebugError::postWarning()`.  The map entry is cleared in
    `cleanupContext()`.

12. **`OBOL_THREADSAFE` conditionals removed** — All `#ifdef OBOL_THREADSAFE`
    / `#else` / `#endif` conditional blocks have been eliminated from 32
    source files using `unifdef -DOBOL_THREADSAFE`.  The threadsafe code paths
    (mutex locks, atomic operations, guarded includes) are now unconditional.
    `SoDB::isMultiThread()` now returns `TRUE` unconditionally.  The
    `OBOL_THREADSAFE` CMake option is kept only for backward compatibility;
    it has no effect.  `config.h` always defines `OBOL_THREADSAFE 1`.

### Phase 4 — Validation ✅ **(Complete)**

- **ThreadSanitizer (TSan) support** — A new `OBOL_TSAN` CMake option adds
  `-fsanitize=thread -fno-omit-frame-pointer` and links with `-fsanitize=thread`
  across the entire build.  Usage:
  ```bash
  cmake -DOBOL_TSAN=ON -DOBOL_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
  cmake --build build && ctest --output-on-failure
  ```
  `OBOL_TSAN` is mutually exclusive with `OBOL_COVERAGE`.

- **Test suite** — The test suite in `tests/threads/` covers all three phases
  (16 tests):
  - Phase 1 (tests 1–11): concurrent `SbName`, `SoNode` ID, `SoBase` refcount,
    `SoType` lookup, notification counter, mixed workload.
  - Phase 2 (tests 12–14): concurrent `setName`/`getName`, GL cache context
    IDs, enabled-elements counter.
  - Phase 3 (tests 15–16): concurrent auditor-list notification via field
    connect/disconnect; concurrent field connect/disconnect + notification.

- **Public API documentation** — The `SoDB` class documentation now contains
  a *Thread Safety* section stating the supported scenarios (*Concurrent render*,
  *Init-then-read*, *Mixed read/write*) and the single caveat (no sharing of
  `SoAction`/`SoState` between threads).  Doxygen comments for
  `SoDB::isMultiThread()`, `readlock()`, `readunlock()`, `writelock()`, and
  `writeunlock()` have been rewritten to remove stale Coin references and
  accurately describe current behaviour.

---

## Conclusion

As of Phase 4, **all known blocking data races have been fixed and the thread-safety
work is complete**.  The remaining caveats are:

- `SoState` must not be shared between threads (each `SoAction` owns its own
  `SoState`; this is documented behaviour and stated in the `SoDB` class doxygen).
- GL contexts must not migrate between threads without explicit platform API
  calls (`glXMakeCurrent` etc.); the `SoGLCacheContextElement` thread
  affinity check will emit a `SoDebugError` warning if this is detected.

The infrastructure for thread safety (mutex wrappers, thread-local storage,
recursive mutexes for notification, atomic counters) is uniformly active
across the library.  All `#ifdef OBOL_THREADSAFE` conditional guards have been
removed; locking is unconditional.  The `SoDB` public API documentation
states the supported concurrency scenarios and the locking protocol.

**Ongoing maintenance:**

1. Run the thread-safety tests under `OBOL_TSAN=ON` in CI to catch regressions.
2. Profile contention on the field recursive mutex in rendering-heavy workloads
   and consider lock-free alternatives if needed.
