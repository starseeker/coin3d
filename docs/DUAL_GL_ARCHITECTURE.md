# Dual-GL Architecture: System OpenGL + OSMesa in One Library

## Overview

Obol can be built so that **both system OpenGL and OSMesa are available in the
same shared library**, selectable per `SoOffscreenRenderer` instance at runtime.
This is the `OBOL_BUILD_DUAL_GL` build mode, which is auto-enabled whenever CMake
finds both a system GL implementation and the OSMesa submodule (`external/osmesa`)
without an explicit override.

This might seem surprising — having two conflicting OpenGL implementations in
the same binary is normally a recipe for linker errors or runtime chaos.  This
document explains the architecture that makes it safe and correct.

---

## Background: The Problem

OSMesa ("Off-Screen Mesa") provides a software-rasterized OpenGL implementation.
Its entry points have the same names as system OpenGL:

```c
// system GL (libGL.so / libOpenGL.so)
void glGenTextures(GLsizei n, GLuint * textures);

// OSMesa (libOSMesa.so) — same name, different implementation
void glGenTextures(GLsizei n, GLuint * textures);
```

Naïvely linking both into the same `.so` would produce duplicate-symbol link
errors, or worse, silently route all calls to one implementation.  Even if it
linked, a `glBegin()` issued against an OSMesa context would actually go to the
system-GL entry point, producing undefined behaviour.

---

## The Solution: Compile-Time Symbol Namespacing

Obol borrows a technique used by BRL-CAD for embedding a private `zlib`
alongside the system `zlib`: the secondary implementation is compiled with all
its symbols renamed.

### Name Mangling via `USE_MGL_NAMESPACE`

OSMesa ships a header `<OSMesa/gl_mangle.h>` that, when included, `#define`s
every `gl*` symbol to `mgl*`:

```c
// gl_mangle.h effect:
#define glGenTextures   mglGenTextures
#define glBegin         mglBegin
// ... ~600 such defines ...
```

When OSMesa is built with `USE_MGL_NAMESPACE`, its library exports `mgl*`
symbols instead of `gl*`.  The `external/osmesa` submodule provides exactly
this build of OSMesa.

### The `gl_osmesa.cpp` Trick

`src/glue/gl_osmesa.cpp` is a single-file compilation unit that recompiles the
main GL glue layer (`src/glue/gl.cpp`) with three preprocessor overrides:

```cpp
// gl_osmesa.cpp

// 1. Use OSMesa headers (<OSMesa/osmesa.h>, <OSMesa/gl.h>)
//    instead of system <GL/gl.h>
#define OBOL_GLHEADERS_OSMESA_OVERRIDE 1

// 2. Activate the mgl* name-mangling
//    Every gl* call in the included source becomes mgl*
#define USE_MGL_NAMESPACE 1

// 3. Prefix every exported SoGLContext_* function with osmesa_
#define SOGL_PREFIX_SET 1
#define SOGL_PREFIX_STR osmesa_

// 4. Include the main GL glue implementation
#include "gl.cpp"
```

The result is an object file that:
- Internally calls `mgl*` functions (OSMesa's name-mangled entry points)
- Exports `osmesa_SoGLContext_*` symbols (e.g. `osmesa_SoGLContext_glGenTextures`)

The main `gl.cpp` object (compiled without the overrides) exports plain
`SoGLContext_*` symbols that call system GL (`glGenTextures`, …).

**Both objects link into the same `.so` with no symbol collision.**

---

## Runtime Dispatch

A thin dispatch layer in `gl.cpp` (compiled with `-DOBOL_BUILD_DUAL_GL`) keeps
the stable `SoGLContext_*` API unchanged at runtime.  Every call that needs
per-context routing goes through a check like this:

```
Application calls SoGLContext_glGenTextures(contextid, ...)
     │
     └─► coingl_is_osmesa_context(contextid)?
              │  YES → osmesa_SoGLContext_glGenTextures(contextid, ...)
              │             calls mglGenTextures  →  OSMesa
              └  NO  → (inline) system glGenTextures(...)
                             →  system libGL
```

The per-context flag is set at context-creation time via the
`ContextManager::isOSMesaContext()` hook (see §23 of `docs/API_DIFFERENCES.md`).
`CoinOSMesaContextManager::isOSMesaContext()` returns `TRUE`;
`FLTKContextManager::isOSMesaContext()` returns `FALSE`.

The flag is stored in a thread-safe registry (`coingl_osmesa_context_ids`) and
looked up by the integer context ID that Obol assigns to each GL context.

---

## Why `USE_MGL_NAMESPACE` Is Not Set Globally

In a pure OSMesa build (`OBOL_USE_OSMESA=ON`), `USE_MGL_NAMESPACE` is set as a
`target_compile_definitions` on the Obol library target — not via
`add_definitions()`.  This scoping is deliberate: test executables and viewer
code that include `<GL/glx.h>` must **not** see `USE_MGL_NAMESPACE`, because
`<GL/glx.h>` conditionally includes `"glx_mangle.h"` when `USE_MGL_NAMESPACE`
is set, and `glx_mangle.h` does not exist on modern Ubuntu 24.04 (it was removed
from the `libgl-dev` package).

In dual-GL builds, `USE_MGL_NAMESPACE` is **not** set on any CMake target.  It
is activated only inside `gl_osmesa.cpp` through the `#define` at the top of
that file.  The system-GL compilation unit (`gl.cpp`) never sees it, ensuring
complete isolation between the two backends.

---

## Extension Function Pointers Are Backend-Local

OpenGL extension entry points (e.g. `glGenFramebuffersEXT`) are not exported by
the main GL library on all systems; they must be fetched via a per-context
resolver:

| Backend | Resolver |
|---------|---------|
| System GL / GLX | `glXGetProcAddress()` |
| System GL / WGL | `wglGetProcAddress()` |
| System GL / EGL | `eglGetProcAddress()` |
| OSMesa | `OSMesaGetProcAddress()` |

In a dual-GL build, using the wrong resolver for a context produces a function
pointer from the wrong backend — which would call system-GL code from an OSMesa
context, causing incorrect rendering or crashes.

The `ContextManager::getProcAddress()` virtual method allows each manager to
delegate to its own resolver.  `CoinOSMesaContextManager` overrides this to call
`OSMesaGetProcAddress()`, keeping all OSMesa extension lookups within OSMesa.

---

## Why Is It Safe? A Summary

| Concern | Mitigation |
|---------|-----------|
| Linker symbol collision | `osmesa_` prefix on exported symbols; `mgl*` prefix on internal calls |
| Wrong backend called at runtime | Per-context dispatch flag; `isOSMesaContext()` hook |
| Extension pointer cross-contamination | `getProcAddress()` routes per-backend; OSMesa uses `OSMesaGetProcAddress()` |
| `USE_MGL_NAMESPACE` leaking into test code | Activated only inside `gl_osmesa.cpp`, not via CMake global defines |
| Thread-local GL current-context leakage | OSMesa and system GL maintain independent current-context state |

---

## Build Configuration Summary

```bash
# Dual-GL (auto-detect: recommended for development machines)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Force dual-GL explicitly
cmake -S . -B build -DOBOL_BUILD_DUAL_GL=ON

# OSMesa only (headless CI, servers)
cmake -S . -B build -DOBOL_USE_OSMESA=ON

# System GL only (GPU-only deployment, no headless fallback)
cmake -S . -B build -DOBOL_USE_SYSTEM_ONLY=ON

# No OpenGL (custom context driver, raytracing only)
cmake -S . -B build -DOBOL_NO_OPENGL=ON
```

When `OBOL_BUILD_DUAL_GL` is active the library is named `libObol.so` and
`SoDB::createOSMesaContextManager()` returns a live OSMesa manager.  When only
system GL is available the function returns `nullptr`.

---

## See Also

- `docs/API_DIFFERENCES.md` §23 — Extended ContextManager API
- `docs/API_DIFFERENCES.md` §24 — Dual-GL architecture summary
- `docs/CONTEXT_MANAGEMENT_API.md` — Full ContextManager implementation guide
- `src/glue/gl_osmesa.cpp` — The compilation-unit trick
- `src/glue/gl.cpp` — The dispatch layer (`coingl_is_osmesa_context`)
- `tests/utils/osmesa_context_manager.h` — Reference OSMesa implementation
- `tests/utils/fltk_context_manager.h` — Reference system-GL implementation
