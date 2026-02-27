# Storage Migration: SbStorage / SbTypedStorage

## Current Status

The thread-local storage subsystem (`SbStorage` / `SbTypedStorage`) has been
enhanced with C++17 infrastructure that resolves the long-standing unimplemented
`cc_storage_thread_cleanup()` stub.  The original dictionary-based
implementation is retained because a complete migration to `thread_local` is
blocked by a fundamental C++17 limitation described below.

### What Was Fixed

`cc_storage_thread_cleanup()` was an empty stub in the Coin3D codebase for
many years:

```c
void cc_storage_thread_cleanup(unsigned long threadid)
{
    /* FIXME: remove and destruct all data for this thread for all storages */
}
```

This was addressed by adding:

* **`StorageRegistry`** — a thread-safe `std::unordered_set` (protected by
  `std::shared_mutex`) that tracks all live `cc_storage` objects.
* **`ThreadCleanupTrigger`** — a `thread_local` RAII object whose destructor
  iterates the registry and removes all data belonging to the exiting thread.

All existing API semantics are preserved and all 218+ usage sites compile
unchanged.

---

## Implementation Architecture

### Core Structure

```c
struct cc_storage {
    unsigned int size;           // per-thread data block size
    void (*constructor)(void *); // optional construction callback
    void (*destructor)(void *);  // optional destruction callback
    cc_dict * dict;              // hash table keyed by thread ID
    cc_mutex * mutex;            // protects dictionary operations
};
```

### Thread-Local Access: `cc_storage_get()`

```c
void * cc_storage_get(cc_storage * storage) {
    unsigned long threadid = cc_thread_id();
    cc_mutex_lock(storage->mutex);
    void * val;
    if (!cc_dict_get(storage->dict, threadid, &val)) {
        val = malloc(storage->size);
        if (storage->constructor) storage->constructor(val);
        cc_dict_put(storage->dict, threadid, val);
    }
    cc_mutex_unlock(storage->mutex);
    return val;
}
```

### Cross-Thread Enumeration: `cc_storage_apply_to_all()`

```c
void cc_storage_apply_to_all(cc_storage * storage,
                              cc_storage_apply_func * func,
                              void * closure) {
    cc_mutex_lock(storage->mutex);
    cc_dict_apply(storage->dict, storage_hash_apply, &mydata);
    cc_mutex_unlock(storage->mutex);
}
```

This cross-thread iteration is essential for operations like GL cache
invalidation (`SoSeparator`) and GL resource cleanup (`SoGLBigImage`).

---

## Why a Full C++17 Migration Is Not Straightforward

C++17 `thread_local` is not directly enumerable:

| Requirement | C++17 `thread_local` |
|-------------|---------------------|
| Per-thread allocation with lazy init | ✅ |
| Constructor/destructor callbacks | ❌ (no parameter passing at thread creation) |
| Enumerate all live instances across threads | ❌ (no standard mechanism) |
| Notify when a thread exits | ❌ (no standard hook) |

The `applyToAll()` function — which iterates every thread's copy — is used in
several critical paths:

* `src/nodes/SoSeparator.cpp` — GL cache invalidation across all threads
* `src/rendering/SoGLBigImage.cpp` — display-list age-based cleanup
* `src/shapenodes/SoShape.cpp` — per-thread static data initialization

A direct replacement with `thread_local` therefore cannot implement
`applyToAll()` without a separate global registry, which reintroduces most of
the complexity of the current approach.

---

## Future Work

When C++20 becomes the minimum standard, the following improvements become
possible:

* `SbBarrier` → `std::barrier`
* `SbThread` → `std::jthread` (automatic joining, stop tokens)
* Re-evaluate `SbStorage` migration if C++20 adds cross-thread `thread_local`
  enumeration support.

See `docs/THREADING_MIGRATION.md` for the completed C++17 threading migration.
