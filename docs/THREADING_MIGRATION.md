# Obol Threading: C++17 Migration

## What Changed

Obol's threading primitives were migrated from custom C wrappers (built on pthreads/Win32 API) to C++17 standard library equivalents:

| Old Primitive | New Implementation |
|---------------|-------------------|
| `SbMutex` | `std::mutex` |
| `SbThreadMutex` | `std::recursive_mutex` |
| `SbCondVar` | `std::condition_variable` (with `SbTime`→`std::chrono` conversion) |
| `SbThread` | `std::thread` with a lambda wrapper preserving `void*(*)(void*)` signatures |
| `SbThreadAutoLock` | `std::lock_guard` (RAII) |
| `SbRWMutex` | Custom wrapper using `std::shared_mutex` + `std::condition_variable`, preserving READ/WRITE precedence policies |
| `SbBarrier` | Custom C++17 implementation (std::barrier is C++20) |
| `SbFifo` | Custom thread-safe queue using `std::deque` + `std::mutex` + `std::condition_variable` |
| `SbStorage`/`SbTypedStorage` | Unchanged — kept original C implementation due to complex `thread_local` registry requirements |
| `cc_recmutex_internal_field/notify_lock/unlock` | Global `SbThreadMutex` instances with `thread_local` nesting counters |

## Why It Was Changed

- **Reduce dependencies**: Eliminated the custom C threading layer (pthreads/Win32 wrappers).
- **Standard compliance**: C++17 threading primitives are well-tested and maintained by compiler vendors.
- **Maintainability**: Fewer custom primitives to audit and maintain.
- **Portability**: Standard library implementations are portable across platforms without conditional compilation.

## Implications

- **Full API compatibility**: All 218+ call sites continue to compile and run without modification.
- **Behavioral compatibility**: Semantics are preserved, including RW mutex READ/WRITE precedence policies and recursive mutex nesting level return values.
- **Requires C++17**: The build now requires a C++17-capable compiler. This was already the project's minimum standard.
- `SbStorage`/`SbTypedStorage` still use the original C implementation; standard `thread_local` does not support runtime enumeration of all live instances, which the storage API requires.

## Potential Future Improvements (C++20+)

| Primitive | C++20 Opportunity |
|-----------|-------------------|
| `SbBarrier` | Replace with `std::barrier` |
| `SbThread` | Replace with `std::jthread` for automatic joining and stop-token support |
| `SbFifo` | Evaluate `std::concurrent_queue` if standardized |
| `SbStorage` | Revisit once `thread_local` enumeration or a suitable standard facility is available |

`std::stop_token` and `std::latch` are also worth considering for cooperative cancellation and one-shot synchronization scenarios respectively.