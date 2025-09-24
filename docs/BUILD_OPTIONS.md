# Build Options and Performance Features

This document describes CMake build options and performance features available in the Coin3D library.

## Precompiled Headers Support

### Overview

Coin3D supports precompiled headers (PCH) to significantly speed up compilation. This feature uses CMake's `target_precompile_headers()` functionality (available in CMake 3.16+) to precompile commonly used standard library headers.

### Usage

**Enable precompiled headers (default):**
```bash
cmake -S . -B build_dir -DCOIN_USE_PRECOMPILED_HEADERS=ON
```

**Disable precompiled headers:**
```bash
cmake -S . -B build_dir -DCOIN_USE_PRECOMPILED_HEADERS=OFF
```

### Performance Impact

Based on testing with GCC 13.3.0 on Ubuntu with 4 parallel build jobs (`-j4`):

- **Without PCH**: ~3m30s build time
- **With PCH**: ~2m53s build time  
- **Performance improvement**: ~18% faster build (37+ seconds saved)
- **User CPU time savings**: ~20% reduction (2m33s saved)

### Technical Details

The precompiled header (`src/CoinPCH.h`) includes:
- Commonly used standard C++ library headers (`<cassert>`, `<cstring>`, `<cstdlib>`, `<cstdio>`)
- Additional standard library headers (`<cmath>`, `<iostream>`, `<algorithm>`, `<memory>`, `<vector>`, `<string>`)

The PCH is applied to:
- Main library target (`Coin`)
- All object library subdirectories (actions, base, elements, etc.)

### Compatibility

- **Requires**: CMake 3.16 or later
- **Tested with**: GCC 13.3.0, modern compilers
- **Platform support**: Linux, Windows (MSVC), macOS (Clang)
- **Backward compatible**: Can be disabled without affecting functionality

### Notes

- PCH only includes safe standard library headers to avoid include order dependencies
- Inventor-specific headers are not precompiled to prevent type conflicts
- The feature is automatically disabled on CMake versions < 3.16 with a warning
- No source code changes are required - PCH is applied automatically during build

## Other Build Options

### Core Options
- `BUILD_SHARED_LIBS`: Build shared library (ON) or static library (OFF) - default: ON
- `COIN_BUILD_TESTS`: Build unit tests - default: ON  
- `COIN_BUILD_EXAMPLES`: Build example applications - default: OFF

### Feature Options
- `COIN3D_USE_OSMESA`: Use OSMesa for offscreen/headless rendering - default: ON
- `COIN_THREADSAFE`: Enable thread-safe render traversals - default: ON
- `USE_EXCEPTIONS`: Compile with C++ exceptions - default: ON

### Component Options
- `HAVE_NODEKITS`: Enable NodeKit support - default: ON
- `HAVE_DRAGGERS`: Enable Dragger support - default: ON  
- `HAVE_MANIPULATORS`: Enable Manipulator support - default: ON

### Build Types
- `CMAKE_BUILD_TYPE`: Release, Debug, MinSizeRel, RelWithDebInfo - default: Release

### Example Build Commands

**Fast development build with PCH:**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Debug -DCOIN_USE_PRECOMPILED_HEADERS=ON
cmake --build build_dir -- -j4
```

**Minimal release build:**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Release \
  -DCOIN_BUILD_TESTS=OFF -DCOIN_BUILD_EXAMPLES=OFF \
  -DCOIN_USE_PRECOMPILED_HEADERS=ON
cmake --build build_dir -- -j4
```

**Static library build:**
```bash
cmake -S . -B build_dir -DBUILD_SHARED_LIBS=OFF \
  -DCOIN_USE_PRECOMPILED_HEADERS=ON  
cmake --build build_dir -- -j4
```