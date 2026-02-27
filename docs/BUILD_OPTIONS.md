# Build Options

This document describes CMake build options available in Obol.

## Core Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `ON` | Build shared library when ON, static library when OFF |
| `OBOL_BUILD_TESTS` | `ON` | Build unit tests |
| `OBOL_BUILD_VIEWER` | auto | Build the FLTK scene viewer (`obol_viewer`); auto-enabled if FLTK is found |

## OpenGL Backend Options

Obol supports three OpenGL backend configurations:

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_USE_OSMESA` | `OFF` | Build against OSMesa only (headless/offscreen rendering, no system GL) |
| `OBOL_USE_SYSTEM_ONLY` | `OFF` | Build against system OpenGL only (no OSMesa fallback) |
| `OBOL_BUILD_DUAL_GL` | `OFF` | Build with both system OpenGL and OSMesa backends in a single library |

When all three are `OFF` (the default), CMake auto-detects available backends: dual mode if both system OpenGL and OSMesa are present, otherwise whichever is available.

`OBOL_USE_OSMESA` and `OBOL_BUILD_DUAL_GL` are mutually exclusive.

## Feature Options

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_THREADSAFE` | `ON` | Enable thread-safe render traversals |
| `USE_EXCEPTIONS` | `ON` | Compile with C++ exceptions |

## Component Options

| Option | Default | Description |
|--------|---------|-------------|
| `HAVE_NODEKITS` | `ON` | Enable NodeKit support |
| `HAVE_DRAGGERS` | `ON` | Enable Dragger support |
| `HAVE_MANIPULATORS` | `ON` | Enable Manipulator support |
| `OBOL_PROFILING` | `OFF` | Enable profiling subsystem |

## Build Optimization Options

### Precompiled Headers

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_USE_PRECOMPILED_HEADERS` | `ON` | Use precompiled headers to speed up compilation (requires CMake 3.16+) |

The precompiled header (`src/misc/CoinPCH.h`) includes commonly used standard C++ library headers and is applied to the main `Obol` library target and all object library subdirectories. On CMake versions older than 3.16 this option is automatically disabled with a warning.

**Performance impact** (GCC 13.3.0, Ubuntu, `-j4`):
- Without PCH: ~3m30s
- With PCH: ~2m53s (~18% faster, ~37 seconds saved)

### Code Coverage

| Option | Default | Description |
|--------|---------|-------------|
| `OBOL_COVERAGE` | `OFF` | Enable code coverage instrumentation (GCC/Clang only); provides a `coverage` target that runs CTest and generates an lcov HTML report |

## Build Types

Set via `CMAKE_BUILD_TYPE`: `Release` (default), `Debug`, `MinSizeRel`, `RelWithDebInfo`.

## Example Build Commands

**Standard release build:**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Release
cmake --build build_dir -- -j4
```

**Development build with debug info:**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Debug
cmake --build build_dir -- -j4
```

**Minimal release build (no tests):**
```bash
cmake -S . -B build_dir -DCMAKE_BUILD_TYPE=Release -DOBOL_BUILD_TESTS=OFF
cmake --build build_dir -- -j4
```

**Static library build:**
```bash
cmake -S . -B build_dir -DBUILD_SHARED_LIBS=OFF
cmake --build build_dir -- -j4
```

**Headless/offscreen-only build:**
```bash
cmake -S . -B build_dir -DOBOL_USE_OSMESA=ON
cmake --build build_dir -- -j4
```