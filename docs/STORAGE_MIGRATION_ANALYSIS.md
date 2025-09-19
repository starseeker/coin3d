# SbStorage/SbTypedStorage C++ Migration Analysis

## Overview

This document provides a detailed analysis of the complex threading semantics in the current SbStorage/SbTypedStorage implementation and the challenges preventing straightforward migration to C++17 standard library features.

## Current Implementation Architecture

### Core Components

1. **cc_storage structure** (storagep.h):
   ```c
   struct cc_storage {
     unsigned int size;           // Size of per-thread data block
     void (*constructor)(void *); // Optional constructor callback
     void (*destructor)(void *);  // Optional destructor callback
     cc_dict * dict;             // Hash table keyed by thread ID
     cc_mutex * mutex;           // Protects dictionary operations
   };
   ```

2. **Thread ID Management**:
   - **POSIX**: Uses `pthread_self()` directly as thread ID
   - **Windows**: Uses TLS to assign sequential integer IDs starting from 1
   - Thread IDs serve as dictionary keys for thread-local data lookup

3. **Dictionary-Based Storage**:
   - Each storage object maintains a `cc_dict` hash table
   - Keys: Thread IDs (unsigned long)
   - Values: Allocated memory blocks for thread-local data
   - Mutex protection around all dictionary operations

### Key Operations

#### cc_storage_get() - Thread-Local Data Access
```c
void * cc_storage_get(cc_storage * storage)
{
  void * val;
  unsigned long threadid = cc_thread_id();
  
  cc_mutex_lock(storage->mutex);
  if (!cc_dict_get(storage->dict, threadid, &val)) {
    val = malloc(storage->size);
    if (storage->constructor) {
      storage->constructor(val);
    }
    cc_dict_put(storage->dict, threadid, val);
  }
  cc_mutex_unlock(storage->mutex);
  
  return val;
}
```

**Semantics:**
- Thread-safe lazy allocation of per-thread data
- Automatic construction using provided callback
- Data persists for the lifetime of the thread
- Dictionary lookup with fallback to allocation

#### cc_storage_apply_to_all() - Cross-Thread Enumeration
```c
void cc_storage_apply_to_all(cc_storage * storage, 
                            cc_storage_apply_func * func, 
                            void * closure)
{
  cc_mutex_lock(storage->mutex);
  cc_dict_apply(storage->dict, storage_hash_apply, &mydata);
  cc_mutex_unlock(storage->mutex);
}
```

**Semantics:**
- Applies a function to ALL thread-local data across ALL threads
- Requires global lock to iterate safely over dictionary
- Critical for operations like cache invalidation and resource cleanup

## Critical Usage Patterns

### 1. SoShape Static Data Management

**Location**: `src/shapenodes/SoShape.cpp:356-359`
```cpp
soshape_staticstorage = new SbStorage(sizeof(soshape_staticdata),
                                     soshape_construct_staticdata,
                                     soshape_destruct_staticdata);
```

**Semantics:**
- Complex static data structure per thread
- Constructor/destructor callbacks for proper initialization
- Data includes texture contexts, primitive data, triangle sorting
- Used for expensive OpenGL state management

### 2. Cache Element Invalidation

**Location**: `src/elements/SoCacheElement.cpp:98-99`
```cpp
invalidated_storage = new SbTypedStorage<SbBool*>(sizeof(SbBool));
*(invalidated_storage->get()) = FALSE;
```

**Semantics:**
- Simple boolean flag per thread
- Used for tracking cache invalidation state
- Critical for rendering correctness

### 3. Cross-Thread Cache Invalidation

**Location**: `src/nodes/SoSeparator.cpp:342`
```cpp
glcachestorage->applyToAll(invalidate_gl_cache, NULL);
```

**Semantics:**
- Invalidates GL caches across ALL threads
- Essential for maintaining OpenGL state consistency
- Demonstrates need for cross-thread enumeration

### 4. GL Resource Management

**Location**: `src/rendering/SoGLBigImage.cpp:574`
```cpp
cc_storage_apply_to_all(PRIVATE(this)->storage, soglbigimage_unrefolddl_cb, &data);
```

**Semantics:**
- Manages OpenGL display lists across threads
- Age-based cleanup of graphics resources
- Complex resource lifecycle management

## Migration Challenges

### 1. C++17 thread_local Limitations

**Problem**: `thread_local` variables are not enumerable
- No standard way to iterate over all threads' instances
- No notification when threads exit
- No central registry of thread_local variables

**Impact**: Cannot implement `applyToAll()` functionality

### 2. Constructor/Destructor Callback Integration

**Problem**: Thread-local storage with custom initialization
- `thread_local` initialization is thread-specific
- No way to pass construction parameters at thread creation time
- Destructor timing is controlled by thread exit

**Current Approach**: Manual callback invocation during lazy allocation

### 3. Cross-Thread Access Requirements

**Problem**: Some operations need to affect all threads simultaneously
- Cache invalidation must be global
- Resource cleanup must be comprehensive
- Thread-local data is inherently thread-specific

**Current Approach**: Global mutex + dictionary iteration

### 4. Thread Cleanup Infrastructure

**Problem**: Unimplemented cleanup when threads exit
- `cc_storage_thread_cleanup()` is currently a stub
- No mechanism to remove dead thread entries from dictionaries
- Potential memory leaks for long-running applications with many threads

### 5. Thread ID Consistency

**Problem**: Different thread ID semantics across platforms
- POSIX: pthread_self() returns varying types
- Windows: Custom sequential ID assignment
- Dictionary keys must be stable and unique

## Design Constraints for C++17 Migration

### Requirements to Preserve

1. **API Compatibility**: Existing SbStorage/SbTypedStorage interfaces
2. **Thread Safety**: All operations must remain thread-safe
3. **Lazy Allocation**: Memory allocated only when first accessed per thread
4. **Constructor/Destructor Callbacks**: Custom initialization/cleanup
5. **Cross-Thread Enumeration**: `applyToAll()` functionality
6. **Performance**: No significant performance regression

### Technical Constraints

1. **C++17 Only**: Cannot use C++20 features like `std::jthread`
2. **Standard Library Preference**: Minimize custom threading primitives
3. **Platform Independence**: Must work on POSIX and Windows
4. **Memory Management**: Proper cleanup of thread-local data
5. **Exception Safety**: Robust error handling

## Potential Migration Strategies

### Strategy 1: Hybrid Thread-Local + Registry

**Approach**: Combine `thread_local` storage with global registry
```cpp
template<typename T>
class ThreadLocalRegistry {
  thread_local static std::unique_ptr<T> instance;
  static std::mutex registry_mutex;
  static std::set<T*> registry;
  
public:
  static T* get() {
    if (!instance) {
      instance = std::make_unique<T>();
      std::lock_guard<std::mutex> lock(registry_mutex);
      registry.insert(instance.get());
    }
    return instance.get();
  }
  
  static void applyToAll(std::function<void(T*)> func) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    for (T* ptr : registry) {
      func(ptr);
    }
  }
};
```

**Pros:**
- Uses standard thread_local for thread-specific storage
- Maintains registry for cross-thread operations
- Automatic cleanup when threads exit

**Cons:**
- Potential for dangling pointers in registry
- Complex lifecycle management
- Thread exit detection required

### Strategy 2: Enhanced Dictionary Approach

**Approach**: Modernize current approach with C++17 features
```cpp
class StorageCxx17 {
  std::unordered_map<std::thread::id, std::unique_ptr<void, void(*)(void*)>> data;
  std::shared_mutex mutex;
  std::function<void*(size_t)> constructor;
  std::function<void(void*)> destructor;
  
public:
  void* get() {
    auto tid = std::this_thread::get_id();
    std::shared_lock<std::shared_mutex> lock(mutex);
    auto it = data.find(tid);
    if (it != data.end()) {
      return it->second.get();
    }
    
    lock.unlock();
    std::unique_lock<std::shared_mutex> write_lock(mutex);
    // Double-check pattern...
  }
};
```

**Pros:**
- Similar to current implementation
- Full control over lifecycle
- Can implement all current features

**Cons:**
- Still requires custom thread ID management
- Complex thread cleanup detection
- More complex than current C implementation

### Strategy 3: Event-Driven Thread Management

**Approach**: Use thread creation/destruction notifications
- Hook into thread creation to register storage
- Use RAII or atexit() for cleanup
- Maintain explicit thread registry

## Recommended Approach

Given the complexity and critical nature of the storage functionality, I recommend a **conservative, incremental migration approach**:

### Phase 1: Documentation and Testing
1. Create comprehensive test suite for all storage semantics
2. Document exact behavior of all usage patterns
3. Establish performance benchmarks

### Phase 2: Enhanced C Implementation
1. Implement missing `cc_storage_thread_cleanup()` functionality
2. Add thread exit detection and cleanup
3. Optimize current implementation with C++17 features where possible

### Phase 3: Gradual C++17 Migration
1. Start with simpler storage use cases (single-value, no callbacks)
2. Migrate to hybrid approach preserving complex semantics
3. Performance and behavior validation at each step

### Phase 4: Complete Migration
1. Full C++17 implementation with preserved semantics
2. Remove C implementation once all tests pass
3. Documentation update and cleanup

## Conclusion

The SbStorage/SbTypedStorage migration is complex due to fundamental limitations in C++17's thread_local capabilities, specifically the lack of enumeration and cross-thread access features. The current dictionary-based approach, while more complex, provides essential functionality that cannot be directly replaced with standard C++17 features.

The migration requires careful design to preserve critical semantics while modernizing the implementation. A conservative, well-tested approach is essential given the widespread use of storage throughout the graphics pipeline.