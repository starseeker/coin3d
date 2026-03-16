# Obol vs. Coin: API Differences Reference

This document catalogs the differences between Obol and the upstream
[Coin3D](https://github.com/coin3d/coin) library from which it derives.  For
each change the rationale is given and the user-facing implications are
described.  Code that worked against Coin may require changes before it will
compile and link against Obol; this document is intended to be the primary
migration guide.

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Initialization: SoDB::init()](#2-initialization-sodbinit)
3. [VRML / VRML97 Support Removed](#3-vrml--vrml97-support-removed)
4. [ScXML / Navigation State-Machine Removed](#4-scxml--navigation-state-machine-removed)
5. [Audio Support Removed](#5-audio-support-removed)
6. [Geospatial Nodes Removed](#6-geospatial-nodes-removed)
7. [NURBS Nodes Removed](#7-nurbs-nodes-removed)
8. [Collision Detection Action Removed](#8-collision-detection-action-removed)
9. [Platform-Specific Context Code Removed](#9-platform-specific-context-code-removed)
10. [PostScript / Hardcopy Output Always Included](#10-postscript--hardcopy-output-always-included)
11. [Profiling Subsystem Off by Default](#11-profiling-subsystem-off-by-default)
12. [Font Handling Simplified](#12-font-handling-simplified)
13. [Threading Primitives Modernized](#13-threading-primitives-modernized)
14. [Dead Code and Obsolete Classes Removed](#14-dead-code-and-obsolete-classes-removed)
15. [New: SoProceduralShape Node](#15-new-soproceduralshape-node)
16. [New: SbFont Public API](#16-new-sbfont-public-api)
17. [New: SuUtils.h Atexit Helper](#17-new-suutilsh-atexit-helper)
18. [C Public API Header Tier Removed](#18-c-public-api-header-tier-removed)
19. [Windows-Specific Headers Removed](#19-windows-specific-headers-removed)
20. [Fields: Long Integer Types Removed](#20-fields-long-integer-types-removed)
21. [Build System Differences](#21-build-system-differences)
22. [Summary Table](#22-summary-table)
23. [New: Extended ContextManager API](#23-new-extended-contextmanager-api)
24. [New: Dual-GL Architecture (System GL + OSMesa in One Library)](#24-new-dual-gl-architecture-system-gl--osmesa-in-one-library)

---

## 1. Design Philosophy

Coin is a full-featured Open Inventor 2.1 implementation that targets GUI
toolkits (Qt, Xt/Motif, Win32), VRML/VRML97 file formats, audio, geospatial
data, and an array of other features that make it a broad-purpose scene graph.
Obol deliberately strips the library back to a self-contained core suitable for
a CAD scene-management back end:

* **No GUI toolkit dependencies** — the application owns the context and
  feeds events in through Obol's abstract interfaces.
* **No VRML/XML/audio** — file-format I/O beyond Open Inventor `.iv` and in-memory
  scene construction is out of scope.
* **C++17 throughout** — platform shims written for C89 compilers are replaced
  with standard-library equivalents.
* **headless first** — full test coverage in a completely headless
  environment is a first-class requirement, not an afterthought.

The result is a smaller, faster-to-build, lower-dependency library that is a
strict *subset* of Coin.  Applications that used only the core scene-graph
features will port with minimal effort; applications that relied on VRML, audio,
navigation, or GUI-toolkit integration require the most work.

---

## 2. Initialization: SoDB::init()

### What changed

`SoDB::init()` now **requires** a `ContextManager*` argument.  The upstream
Coin signature takes no arguments:

```cpp
// Coin (upstream)
SoDB::init();

// Obol — a ContextManager MUST be provided
SoDB::setContextManager(&myManager);  // deprecated path
SoDB::init(&myManager);               // preferred path
```

`SoDB::ContextManager` is an abstract base class declared inside `SoDB`.

**Pure-virtual methods (required):**

```cpp
class SoDB::ContextManager {
public:
    virtual ~ContextManager() {}
    virtual void * createOffscreenContext(unsigned int width,
                                          unsigned int height) = 0;
    virtual SbBool makeContextCurrent(void * context) = 0;
    virtual void restorePreviousContext(void * context) = 0;
    virtual void destroyContext(void * context) = 0;
};
```

**Optional virtual methods (added during hardening — see §23):**

| Method | Purpose | Default |
|--------|---------|---------|
| `isOSMesaContext(context)` | Identify OSMesa-backed contexts for GL dispatch | `FALSE` |
| `maxOffscreenDimensions(w,h)` | Report backend size ceiling | `{0,0}` (probe via GL) |
| `getActualSurfaceSize(ctx,w,h)` | Report exact surface size for readback safety | `{0,0}` (unknown) |
| `getProcAddress(name)` | Backend-specific extension function resolver | `nullptr` |
| `renderScene(scene,w,h,px,n,bg)` | Optional CPU/GPU render path bypassing GL | `FALSE` (use GL) |

`SoDB::getContextManager()` returns the registered global instance.
`SoDB::setContextManager()` replaces it at runtime (no re-init needed).
`SoDB::createOSMesaContextManager()` returns a fully-configured built-in
OSMesa manager (see §23).
`SoDB::readAllVRML()` has been removed (see §3).

### Why

Coin internally creates OpenGL contexts using platform-specific APIs (WGL on
Windows, GLX on Linux, CGL/AGL on macOS).  This tightly couples the library to
each windowing system.  Moving context creation into an application-supplied
callback object eliminates all platform-specific context code from the library
and allows OSMesa, EGL, or any other backend to be substituted without patching
Obol itself.  The change also makes initialization ordering explicit: the
context manager is always in place before `SoDB::init()` attempts any
OpenGL-related setup.

### User implications

Every application that calls `SoDB::init()` must now supply a `ContextManager`.
For headless or testing scenarios the `NullContextManager` (no-op) shipped with
the test suite is sufficient.  For real rendering provide an implementation
backed by your context API of choice:

```cpp
// Minimal NullContextManager (no offscreen rendering)
class NullContextManager : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int, unsigned int) override
        { return nullptr; }
    SbBool makeContextCurrent(void *) override { return FALSE; }
    void restorePreviousContext(void *) override {}
    void destroyContext(void *) override {}
};

int main() {
    NullContextManager cm;
    SoDB::init(&cm);
    // ...
}
```

For offscreen rendering with OSMesa, implement all four methods using the
OSMesa API.  See `docs/CONTEXT_MANAGEMENT_API.md` and the test suite for
complete worked examples.

---

## 3. VRML / VRML97 Support Removed

### What changed

The entire VRML and VRML97 subsystem has been removed:

* **70 headers** under `include/Inventor/VRMLnodes/` are gone.
* `SoDB::readAllVRML()` has been removed.
* Actions `SoToVRMLAction` and `SoToVRML2Action` have been removed.
* `SoVRMLGroup` and all other `SoVRML*` node classes are gone.
* VRML-specific elements and fields are removed.

### Why

VRML/VRML97 file-format support requires XML parsing, network I/O, audio
nodes, and an ECMAScript engine — all of which conflict with Obol's goal of
being a minimal, dependency-free core.  The target use case (CAD scene
management from application-owned in-memory data) has no need for VRML file
reading or writing.

### User implications

Code that reads `.wrl` or `.vrml` files, calls `SoDB::readAllVRML()`, uses
`SoToVRMLAction`, or instantiates `SoVRML*` nodes will not compile.  There is
no migration path within Obol: VRML functionality must be handled by the
application or a separate library.

---

## 4. ScXML / Navigation State-Machine Removed

### What changed

The complete ScXML state-machine framework and the associated navigation layer
have been removed:

* **42 headers** under `include/Inventor/scxml/` (e.g., `ScXML.h`,
  `ScXMLStateMachine.h`, `SoScXMLStateMachine.h`).
* **11 headers** under `include/Inventor/navigation/` (e.g.,
  `SoScXMLNavigation.h`, `SoScXMLRotateTarget.h`, `SoScXMLPanTarget.h`, …).

### Why

The ScXML navigation system was introduced in Coin to drive interactive camera
and object navigation from state-machine descriptions encoded in XML.  It
depends on XML parsing, ECMAScript evaluation, and tight coupling to GUI event
systems — all of which Obol has removed.  Applications that own their own event
loop are expected to implement navigation logic directly using Obol's dragger
and event APIs.

### User implications

Applications that use `SoScXMLStateMachine`, `SoScXMLNavigation`, or any of
the `SoScXML*Target` classes must replace that logic with direct event
handling.  Obol provides `SoHandleEventAction`, draggers, and manipulators to
support interactive transformations without a state machine intermediary.

---

## 5. Audio Support Removed

### What changed

All OpenAL / VRML97 audio functionality has been removed:

* `include/Inventor/misc/SoAudioDevice.h` is gone.
* Audio-related elements (`SoListenerDopplerElement`,
  `SoListenerGainElement`, `SoListenerOrientationElement`,
  `SoListenerPositionElement`, `SoSoundElement`) are gone.
* `SoAudioRenderAction` has been removed.
* All `SoVRMLAudioClip`, `SoVRMLSound`, and related VRML audio nodes are gone
  (part of the VRML removal, §3).

### Why

Audio playback tied to VRML scene elements is not a feature of the intended CAD
scene-graph use case and introduces an OpenAL dependency.

### User implications

There is no audio API in Obol.  Applications requiring spatialised audio must
manage it independently.

---

## 6. Geospatial Nodes Removed

### What changed

The Geo (geospatial coordinate system) nodes have been removed:

* Node headers: `SoGeoCoordinate.h`, `SoGeoLocation.h`, `SoGeoOrigin.h`,
  `SoGeoSeparator.h`.
* Element: `SoGeoElement.h`.
* Misc: `SoGeo.h`.

### Why

Geospatial coordinate support (ECEF, UTM, geographic lat/long) was added to
Coin to support VRML97 geo-referenced scenes.  It is not relevant to the
desktop CAD target and pulls in significant extra complexity.

### User implications

Applications using `SoGeoOrigin`, `SoGeoLocation`, `SoGeoCoordinate`, or
`SoGeoSeparator` must either remove those nodes or perform their own coordinate
transformation in application code before inserting geometry into the Obol
scene graph.

---

## 7. NURBS Nodes Removed

### What changed

NURBS surface and curve nodes have been removed:

* `SoNurbsCurve.h`, `SoNurbsSurface.h`, `SoNurbsProfile.h`
* `SoIndexedNurbsCurve.h`, `SoIndexedNurbsSurface.h`

### Why

NURBS evaluation requires a separate computational library (historically
OpenGL's built-in GLU NURBS tessellator, which is deprecated) and is not needed
for the polygonal geometry the CAD target works with.

### User implications

Applications that render NURBS surfaces or curves through these nodes must
pre-tessellate the geometry into polygonal `SoIndexedFaceSet` /
`SoIndexedLineSet` representations before passing it to the scene graph.

---

## 8. Collision Detection Action Removed

### What changed

`SoIntersectionDetectionAction` and its header
`include/Inventor/collision/SoIntersectionDetectionAction.h` have been removed.

### Why

The collision/intersection detection action was a Coin extension beyond the
core Open Inventor 2.1 specification.  The target applications perform their
own geometry intersection tests in application space before updating the scene
graph.

### User implications

Applications that relied on `SoIntersectionDetectionAction` for scene-level
intersection queries must implement equivalent logic externally.

---

## 9. Platform-Specific Context Code Removed

### What changed

All platform-specific OpenGL context and extension code has been removed from
the library:

**Files removed (source level, each entry is a .cpp + .h pair unless noted):**

| File pair | What it provided |
|-----------|-----------------|
| `src/glue/gl_wgl.{cpp,h}` | Windows WGL context creation |
| `src/glue/gl_glx.{cpp,h}` | Linux GLX context creation |
| `src/glue/gl_agl.{cpp,h}` | macOS AGL context creation |
| `src/glue/gl_cgl.{cpp,h}` | macOS CGL context creation |
| `src/glue/gl_egl.{cpp,h}` | EGL context creation (embedded) |
| `src/rendering/SoOffscreenWGLData.{cpp,h}` | Windows screen resolution |
| `src/rendering/SoOffscreenGLXData.{cpp,h}` | Linux screen resolution |
| `src/rendering/SoOffscreenCGData.{cpp,h}` | macOS screen resolution |
| `src/fonts/win32.{cpp,h}` | Windows GDI font enumeration |
| `include/Inventor/oivwin32.h` | Windows SDK compatibility header (header only) |
| `src/glue/win32api.h` | Internal Windows API wrapper (header only) |

**Context management functions** (`cc_glglue_context_*`) now delegate entirely
to the application-supplied `SoDB::ContextManager` callbacks; they return
`NULL`/`FALSE` if no manager is registered.

**PBuffer functions** (`cc_glglue_context_pbuffer_is_bound`, etc.) are stubs
that always return `FALSE`/no-op.

**Screen resolution detection** uses a default of 72 DPI instead of
querying the display server.  Unlike Coin, applications can now change this
value at runtime using the new static API on `SoOffscreenRenderer`:

```cpp
// Coin (upstream) — queried display server
#ifdef HAVE_GLX
  pixmmres = SoOffscreenGLXData::getResolution();
#elif defined(HAVE_WGL)
  pixmmres = SoOffscreenWGLData::getResolution();
#endif

// Obol — portable default, overridable at runtime
SbVec2f pixmmres(72.0f / 25.4f, 72.0f / 25.4f);   // internal default

// Override to match display or output target:
SoOffscreenRenderer::setScreenPixelsPerInch(96.0f);   // e.g. typical 96 DPI display
float dpi = SoOffscreenRenderer::getScreenPixelsPerInch();  // read current value
```

Both methods are static; the setting is process-wide.

**OpenGL function loading** uses only the portable `cc_dl_sym()` path; the
`wglGetProcAddress` and `glXGetProcAddressARB` paths are gone.
`coin_gl_current_context()` always returns `NULL`.

**Font rendering** now uses the embedded `struetype` rasterizer on all
platforms; the Win32 GDI font path (`cc_flww32_*`) and the system
`libfreetype` dependency have both been removed (see §12).

### Why

This is the single largest structural change in Obol.  Removing ~8,000 lines of
platform-specific context code eliminates the conditional forest of
`#ifdef HAVE_WGL / HAVE_GLX / COIN_MACOS_10` directives and leaves a single,
portable code path for all platforms.  Portability is now the application's
responsibility through the `ContextManager` interface, which is far easier to
reason about and test than hidden `#ifdef` branches.

### User implications

* **Positive:** Obol itself has zero dependency on WGL, GLX, AGL, CGL, or EGL.
  It compiles and links identically on all platforms.
* **Required action:** Applications must implement `SoDB::ContextManager` for
  any platform where they want offscreen rendering.  On Windows this typically
  means a WGL-based manager; on Linux a GLX- or EGL-based manager; etc.  OSMesa-based managers work on any platform.
* **Configurable DPI:** The default 72 DPI is appropriate for most headless
  use cases.  Applications that relied on Coin's automatic screen DPI detection
  can set the desired value explicitly:
  ```cpp
  SoOffscreenRenderer::setScreenPixelsPerInch(96.0f);
  ```

---

## 10. PostScript / Hardcopy Output

### What changed

No change from upstream Coin: the `src/hardcopy/` subsystem (PostScript vector
output via `SoVectorizePSAction`, `PSVectorOutput`, `VectorOutput`,
`VectorizeAction`, `VectorizeActionP`) is always compiled and `SoHardCopy::init()`
is called unconditionally during `SoDB::init()`.

### Why

The PostScript hardcopy API works in any environment, including fully headless
builds backed by OSMesa, without requiring a running viewer or display server.

### User implications

`SoVectorizePSAction` and related classes are always available.  No CMake
option is needed and no source changes are required relative to upstream Coin.

---

## 11. Profiling Subsystem Off by Default

### What changed

In upstream Coin the profiling subsystem is always compiled in and activated by
setting the `COIN_PROFILER` environment variable at runtime.

In Obol, the profiling code is compiled in but **`SoProfiler::isEnabled()`
always returns `FALSE` unless the build is configured with
`-DOBOL_PROFILING=ON`**.  With that option off (the default) the entire
profiling code path is unreachable at runtime regardless of the `COIN_PROFILER`
environment variable.

### Why

Profiling instrumentation adds dead-code weight to every hot traversal path
(`SoSeparator`, `SoGroup`, `SoMaterial`).  Until the overhead has been
benchmarked against realistic scenes the subsystem is kept disabled by default
so that performance regressions cannot be silently introduced.

### User implications

`SoProfilingReportGenerator`, `SbProfilingData`, and related classes are
always compiled but are not reachable at runtime unless `-DOBOL_PROFILING=ON`
is specified at CMake configure time:

```bash
cmake -S . -B build_dir -DOBOL_PROFILING=ON
```

When enabled, profiling behaves identically to upstream Coin: set the
`COIN_PROFILER` environment variable to activate it (see the upstream Coin
documentation for the full syntax).

---

## 12. Font Handling Simplified

### What changed

* The Win32 GDI font backend (`cc_flww32_*`) has been removed.
* All platforms now use the embedded **`struetype`** TrueType rasterizer
  directly; the system `libfreetype` library is **not** used and is **not**
  required as a build or runtime dependency.
* An embedded fallback bitmap font (ProFont) is used when no TrueType font
  file is available.
* A new public `SbFont` class (§16) provides a clean C++ API for font
  management.
* The old `SoGlyph` class (declared obsolete in Coin itself) has been removed
  from both the source tree and the public headers.

### Why

Multi-platform font enumeration and two separate font backends added
significant complexity while providing little benefit in a headless or
single-platform context.  Replacing the libfreetype dependency with the
self-contained `struetype` rasterizer eliminates an external dependency
entirely: fonts are handled by the embedded rasterizer on all platforms with a
single, auditable code path.

### User implications

* The `SoGlyph` class no longer exists.  Code using `SoGlyph` must be
  updated to use `SbFont` / `SbGlyph2D` / `SbGlyph3D` instead.
* Windows applications that relied on system font enumeration through
  `cc_flww32_*` must supply font files explicitly via `SbFont::loadFont()`.
* **`libfreetype` is not required** and does not need to be installed.  The
  `struetype` rasterizer is bundled with the source tree.

---

## 13. Threading Primitives Modernized

### What changed

Coin's threading layer was built on custom C wrappers around POSIX pthreads and
Win32 thread APIs (defined in `include/Inventor/C/threads/`).  Obol replaces
the *implementations* with C++17 standard-library equivalents while preserving
the public `Sb*` class interfaces:

| Obol class | C++17 backing |
|------------|---------------|
| `SbMutex` | `std::mutex` |
| `SbThreadMutex` | `std::recursive_mutex` |
| `SbCondVar` | `std::condition_variable` |
| `SbThread` | `std::thread` |
| `SbThreadAutoLock` | `std::lock_guard` |
| `SbRWMutex` | custom using `std::shared_mutex` |
| `SbBarrier` | custom C++17 (std::barrier is C++20) |
| `SbFifo` | `std::deque` + `std::mutex` + `std::condition_variable` |
| `SbStorage` / `SbTypedStorage` | dictionary + C++17 RAII cleanup |

The C-level `cc_mutex_*`, `cc_condvar_*`, `cc_thread_*` functions that were
part of `include/Inventor/C/threads/` are **internal** and no longer in the
public header tree.

`SbBarrierImpl.h` (new in Obol) provides the C++17 barrier implementation
details.

### Why

The original C wrappers required maintaining platform-specific code paths for
POSIX and Win32.  C++17 `<mutex>`, `<thread>`, and `<condition_variable>` are
portable across all supported platforms, well-tested, and reduce the Obol
maintenance surface substantially.  The `cc_recmutex_internal_*` global locking
functions used for field and notification operations are now backed by
`std::recursive_mutex`.

A long-standing FIXME — `cc_storage_thread_cleanup()` was an empty stub for
years — has been addressed by implementing automatic per-thread cleanup using a
C++17 RAII `ThreadCleanupTrigger`.

### User implications

* The **public** `Sb*` threading classes (`SbMutex`, `SbCondVar`, etc.) have
  the same interface as in Coin and no source changes are needed.
* Code that included `include/Inventor/C/threads/mutex.h` (or other
  `Inventor/C/threads/` headers) must be updated; those headers are no longer
  part of the public API.  Use the `Sb*` C++ classes instead.
* C++17 (`-std=c++17`) is required to build Obol.

---

## 14. Dead Code and Obsolete Classes Removed

Several items with no callers in the Obol codebase have been removed to reduce
build time and binary size.

### SoGLImage viewer-loop methods

Nine public `SoGLImage` methods that existed solely to support a running viewer
render loop have been removed:

| Removed method | Reason |
|----------------|--------|
| `beginFrame(SoState*)` | Viewer-loop texture management; no viewer in Obol |
| `endFrame(SoState*)` | Same |
| `freeAllImages(SoState*)` | Same |
| `setDisplayListMaxAge(uint32_t)` | Same |
| `setEndFrameCallback(cb, closure)` | VRML97 caller removed |
| `getNumFramesSinceUsed()` | VRML97 caller removed |
| `setResizeCallback(cb, closure)` | No callers |
| `useAlphaTest()` | No callers; alpha-test handled by `SoLazyElement` |
| `getGLImageId()` | No callers |

The `SoGLImageResizeCB` typedef and associated private fields have also been
removed.

### cc_string C ADT

`src/base/string.cpp` (264 lines, `cc_string_*` functions) has been excluded
from the build.  All string management in Obol uses `SbString` or `std::string`.

### SoGlyph obsolete glyph cache

`SoGlyph` was explicitly marked obsolete in Coin's own documentation.  It has
been removed from the build and its header deleted.

### utf8/ini.cpp INI file library

The `utf8::IniFile` class bundled as part of the `neacsum/utf8` library is
never instantiated.  `utf8/ini.cpp` has been excluded from the build.

### User implications

Code that calls the removed `SoGLImage` methods, includes `SoGlyph.h`, or uses
`cc_string_*` functions will not compile.  Replacements:

* `SoGLImage` render-loop methods — no replacement needed; manage texture
  lifecycle explicitly via `ref()`/`unref()`.
* `SoGlyph` — use `SbFont` (see §16).
* `cc_string_*` — use `SbString` or `std::string`.

---

## 15. New: SoProceduralShape Node

Obol adds `SoProceduralShape` (`include/Inventor/nodes/SoProceduralShape.h`),
a new scene-graph node with no Coin equivalent.

`SoProceduralShape` allows applications to introduce custom 3D shape types
into the scene graph **without writing C++ subclasses of `SoShape`**.  Instead,
the application registers a *shape type* by name, supplying:

* A JSON schema describing editable parameters (type, bounds, defaults).
* A bounding-box callback (`SoProceduralBBoxCB`).
* A geometry callback (`SoProceduralGeomCB`) that produces triangle meshes
  and/or 3-D wireframe data.

```cpp
SoProceduralShape::registerShapeType(
    "TruncatedCone",
    jsonSchema,
    myBBoxCallback,
    myGeomCallback
);

SoProceduralShape * shape = new SoProceduralShape;
shape->shapeType.setValue("TruncatedCone");
shape->params.setValues(...);  // parameter values per the schema
root->addChild(shape);
```

The node participates fully in the standard action system:
`SoGetBoundingBoxAction`, `SoGLRenderAction`, `SoRayPickAction`, and
`SoCallbackAction` all work through the registered callbacks.

---

## 16. New: SbFont Public API

`include/Inventor/SbFont.h` exposes a clean C++ font management API.  In
upstream Coin font handling was entirely internal, accessible only through the
opaque `cc_flw_*` C functions.  `SbFont` provides:

```cpp
SbFont font("path/to/MyFont.ttf");
font.setSize(14.0f);

SbVec2f advance = font.getGlyphAdvance('A');
SbVec2f kern    = font.getGlyphKerning('A', 'V');
```

The class uses the embedded `struetype` TrueType rasterizer with the bundled
ProFont as a built-in fallback when no font file is supplied.

---

## 17. New: SuUtils.h Atexit Helper

`include/Inventor/SuUtils.h` (no Coin equivalent) exposes
`SbAtexitStaticInternal()`, a function used by the `SO_NODE_INIT_CLASS` and
related macros to register static-data cleanup functions.  In Coin this was an
internal-only symbol; Obol promotes it to the public header so that custom node
implementations outside the Obol source tree can use the macro correctly.

---

## 18. C Public API Header Tier Removed

Upstream Coin exposes a C-language public header tier under
`include/Inventor/C/`:

```
Inventor/C/XML/       — XML parser headers (8 headers)
Inventor/C/base/      — C string, heap, list, etc.
Inventor/C/threads/   — C threading primitives
Inventor/C/glue/dl.h, gl.h, spidermonkey.h
Inventor/C/tidbits.h
Inventor/C/errors/
```

**None of these are present in Obol's public include tree.**

### Why

The C-tier existed to allow the library internals and GUI toolkit adapters to
share a stable ABI across C/C++ boundaries.  With all GUI toolkit adapters,
VRML, ScXML, audio, and the legacy C threading layer removed, there are no
callers for this tier outside the library itself.  The equivalent functionality
is provided by the C++ `Sb*` classes and standard-library types.

### User implications

Code that includes `<Inventor/C/threads/mutex.h>`, `<Inventor/C/tidbits.h>`,
or any other `Inventor/C/` header will fail to compile.  Migration:

| Old header | Replacement |
|------------|-------------|
| `Inventor/C/threads/mutex.h` | `Inventor/threads/SbMutex.h` |
| `Inventor/C/threads/thread.h` | `Inventor/threads/SbThread.h` |
| `Inventor/C/threads/condvar.h` | `Inventor/threads/SbCondVar.h` |
| `Inventor/C/tidbits.h` | `Inventor/SuUtils.h` (partial) |
| `Inventor/C/XML/…` | Not available in Obol |
| `Inventor/C/glue/gl.h` | Internal; do not include directly |

---

## 19. Windows-Specific Headers Removed

`include/Inventor/oivwin32.h` (a thin wrapper around `<windows.h>`) and the
internal `src/glue/win32api.h` have been removed.  On Windows, applications
should include `<windows.h>` directly where needed.

---

## 20. Fields: Long Integer Types Removed

Four field types that wrapped the platform-dependent `long` integer type have
been removed:

* `SoSFLong`, `SoMFLong` — signed `long` scalar and multi-valued fields.
* `SoSFULong`, `SoMFULong` — unsigned `long` scalar and multi-valued fields.

### Why

`long` has different sizes on 32-bit vs. 64-bit Windows versus POSIX platforms,
making cross-platform serialization unreliable.  The corresponding
`SoSFInt32` / `SoMFInt32` and `SoSFUInt32` / `SoMFUInt32` types with explicit
32-bit widths are the correct portable replacements.

The audio-related elements (`SoListenerDopplerElement`, etc.) that depended on
`SoLongElement` are also removed (see §5).

### User implications

Replace all uses of `SoSFLong`, `SoMFLong`, `SoSFULong`, `SoMFULong` with
`SoSFInt32`, `SoMFInt32`, `SoSFUInt32`, `SoMFUInt32` respectively.

---

## 21. Build System Differences

### GL backend options

The two primary GL backend switches replace the old `OBOL_USE_OSMESA` /
`OBOL_USE_SYSTEM_ONLY` / `OBOL_BUILD_DUAL_GL` triplet:

| CMake option | Default | Description |
|---|---|---|
| `OBOL_USE_SYSTEM_GL` | auto¹ | Enable the system OpenGL backend |
| `OBOL_USE_SWRAST` | auto² | Enable the software-rasterizer (OSMesa) backend |

¹ Default `ON` when CMake's `find_package(OpenGL)` succeeds; `OFF` otherwise.  
² Default `ON` when `external/osmesa/CMakeLists.txt` is present; `OFF` otherwise.

The combination of these two flags determines the build mode:

| `OBOL_USE_SWRAST` | `OBOL_USE_SYSTEM_GL` | Mode | Library name |
|---|---|---|---|
| `OFF` | `OFF` | No-OpenGL (custom context managers only) | `libObol-nogl` |
| `ON` | `OFF` | Software-rasterizer only (headless/offscreen) | `libObol-swrast` |
| `OFF` | `ON` | System OpenGL only | `libObol` |
| `ON` | `ON` | **Dual-GL** (system primary + OSMesa per-renderer) | `libObol` |

### Core options

| CMake option | Default | Description |
|---|---|---|
| `BUILD_SHARED_LIBS` | `ON` | Shared vs. static library |
| `OBOL_BUILD_TESTS` | `ON` | Build the Catch2-based test suite |
| `OBOL_BUILD_DOCS` | `OFF` | Build Doxygen API documentation (requires Doxygen) |
| `OBOL_THREADSAFE` | `ON` | Thread-safe render traversals |
| `HAVE_NODEKITS` | `ON` | NodeKit support |
| `HAVE_DRAGGERS` | `ON` | Dragger support |
| `HAVE_MANIPULATORS` | `ON` | Manipulator support |
| `OBOL_PROFILING` | `OFF` | Enable profiling subsystem (`SoProfiler::isEnabled()` always `FALSE` when `OFF`) |
| `USE_EXCEPTIONS` | `ON` | Compile with C++ exceptions (GCC/Clang) |

### Build quality options

| CMake option | Default | Description |
|---|---|---|
| `OBOL_USE_UNITY_BUILD` | `ON` | Unity (jumbo) build for faster compilation (CMake ≥ 3.16) |
| `OBOL_WARNINGS` | `ON` | Enable extra warning flags (`-Wall -Wextra -Wundef -Wshadow` etc.) on the Obol target |
| `OBOL_COVERAGE` | `OFF` | Code-coverage instrumentation (GCC/Clang; adds `coverage` CTest target) |

### Viewer options

| CMake option | Default | Description |
|---|---|---|
| `OBOL_BUILD_VIEWER` | auto³ | Build `obol_viewer` FLTK scene viewer |
| `OBOL_BUILD_MINIMALIST_VIEWER` | same as `OBOL_BUILD_VIEWER` | Build `obol_minimalist_viewer` stress-tester |
| `OBOL_VIEWER_USE_FLTK_GL` | `ON` | Use FLTK GL context for the system-GL panel in `obol_viewer` |
| `OBOL_VIEWER_USE_EMBREE` | auto⁴ | Enable Intel Embree 4 CPU raytracing panel in `obol_viewer` |
| `OBOL_VIEWER_USE_VULKAN` | auto⁵ | Enable Vulkan hardware-rasterization panel in `obol_viewer` |
| `OBOL_BUILD_QT_VIEWER` | auto⁶ | Build `obol_qt_viewer` Qt6 scene viewer |
| `OBOL_BUILD_QT_EXAMPLE` | `OFF` | Build `qt_obol_example` standalone Qt/Obol widget demo |

³ `ON` when `FLTK_FOUND` and `OBOL_BUILD_TESTS`; `OFF` otherwise.  
⁴ `ON` when Embree 4 is found; `OFF` otherwise.  
⁵ `ON` when Vulkan is found; `OFF` otherwise.  
⁶ `ON` when Qt6 Widgets/Gui is found and `OBOL_BUILD_TESTS`; `OFF` otherwise.

### MSVC-specific options (Windows only)

| CMake option | Default | Description |
|---|---|---|
| `OBOL_BUILD_MSVC_STATIC_RUNTIME` | `OFF` | Link against the static MSVC C runtime (`/MT`) |
| `OBOL_BUILD_MSVC_MP` | `ON` | Enable parallel compilation (`/MP`) in Visual Studio |

**GL backend selection:** When neither `OBOL_USE_SWRAST` nor `OBOL_USE_SYSTEM_GL`
is explicitly set, CMake auto-detects available GL libraries and sets the defaults
as described above.  Setting both `ON` gives the dual-GL build in which both
system GL and OSMesa are compiled into the same shared library, selectable per
`SoOffscreenRenderer` instance at runtime (see §24).

**Software rasterizer submodule:** The `external/osmesa` submodule provides the
name-mangled OSMesa build required for `OBOL_USE_SWRAST=ON`.

**FLTK detection:** CMake first looks for a system FLTK installation
(`libfltk1.3-dev`); if none is found it automatically initialises the
`external/fltk` submodule and builds FLTK from source.

**Minimum compiler standard:** C++17.

**Out-of-source builds are enforced.**  Attempting to configure in the source
directory will fail immediately.

Upstream Coin's Autoconf/Automake build system is not present in Obol; only CMake is supported.

> **Note:** Obol's CMake options use the `OBOL_` prefix throughout.  The old
> Coin-era `COIN_USE_PRECOMPILED_HEADERS` option no longer exists; unity builds
> are controlled by `OBOL_USE_UNITY_BUILD` instead.

---

## 22. Summary Table

| Category | Change | Migration action |
|----------|--------|-----------------|
| **Initialization** | `SoDB::init(ContextManager*)` — argument required | Implement and pass a `ContextManager`; NullContextManager for non-rendering use |
| **VRML / VRML97** | All 70+ `SoVRML*` nodes and headers removed | Handle VRML externally; use `.iv` or in-memory scene construction |
| **VRML actions** | `SoToVRMLAction`, `SoToVRML2Action` removed | No Obol replacement |
| **ScXML / Navigation** | 42 ScXML + 11 navigation headers removed | Implement navigation directly using events and draggers |
| **Audio** | `SoAudioDevice`, audio elements, `SoAudioRenderAction` removed | Handle audio externally |
| **Geospatial** | `SoGeo*` nodes and `SoGeoElement` removed | Apply coordinate transforms in application code |
| **NURBS** | `SoNurbsCurve`, `SoNurbsSurface`, etc. removed | Pre-tessellate to `SoIndexedFaceSet` / `SoIndexedLineSet` |
| **Collision** | `SoIntersectionDetectionAction` removed | Perform intersection tests externally |
| **Platform GL context** | WGL/GLX/AGL/CGL/EGL code removed | Implement `SoDB::ContextManager` for each platform |
| **Configurable DPI** | Offscreen renderer defaults to 72 DPI; adjustable at runtime | Call `SoOffscreenRenderer::setScreenPixelsPerInch(dpi)` to override |
| **PostScript/hardcopy** | Always compiled; API always available | No action needed; `SoVectorizePSAction` usable in all builds |
| **Profiling** | Always compiled; off by default (`-DOBOL_PROFILING=ON` to enable) | Add CMake option; `COIN_PROFILER` env var activates it when enabled |
| **Font: SoGlyph** | Removed (was deprecated in Coin) | Use `SbFont` / `SbGlyph2D` / `SbGlyph3D` |
| **Font: Win32 GDI / libfreetype** | Both removed; embedded `struetype` rasterizer used | No external font library needed; supply `.ttf` files via `SbFont::loadFont()` |
| **Threading C API** | `Inventor/C/threads/` headers not public | Use `Sb*` C++ threading classes |
| **C header tier** | `Inventor/C/` subtree not in public headers | Use equivalent C++ APIs (see §18) |
| **XML headers** | `Inventor/C/XML/` removed | Not available; handle XML externally |
| **Windows headers** | `oivwin32.h` removed | Include `<windows.h>` directly |
| **Long fields** | `SoSFLong`, `SoMFLong`, `SoSFULong`, `SoMFULong` removed | Use `SoSFInt32` / `SoMFInt32` / `SoSFUInt32` / `SoMFUInt32` |
| **SoGLImage methods** | 9 viewer-loop methods removed | Manage texture lifetime via ref/unref |
| **cc_string ADT** | Removed from build | Use `SbString` or `std::string` |
| **NEW: SoProceduralShape** | New node for callback-driven geometry | No migration needed; new capability |
| **NEW: SbFont** | New public font API | Replaces internal `cc_flw_*` for user code |
| **NEW: SuUtils.h** | `SbAtexitStaticInternal` now public | Use when writing custom node classes |
| **NEW: Extended ContextManager** | `getActualSurfaceSize`, `maxOffscreenDimensions`, `isOSMesaContext`, `getProcAddress`, `renderScene` | Override as needed; defaults are safe no-ops |
| **NEW: Per-renderer managers** | `SoOffscreenRenderer` accepts per-instance `ContextManager*` | Pass manager to constructor; use `SDB::createOSMesaContextManager()` for built-in OSMesa |
| **NEW: Dual-GL mode** | System GL + OSMesa in one library (`OBOL_USE_SWRAST=ON` + `OBOL_USE_SYSTEM_GL=ON`) | Auto-enabled when both are detected; see §24 |
| **Build system** | CMake only; C++17 required; `OBOL_` option prefix throughout | Update build scripts; replace old `OBOL_USE_OSMESA`→`OBOL_USE_SWRAST`, `OBOL_USE_SYSTEM_ONLY`→`OBOL_USE_SYSTEM_GL`, `COIN_USE_PRECOMPILED_HEADERS`→`OBOL_USE_UNITY_BUILD`, `COIN_BUILD_TESTS`→`OBOL_BUILD_TESTS` |

---

## 23. New: Extended ContextManager API

### What changed

The `SoDB::ContextManager` interface has been extended with additional optional
virtual methods to support safety hardening, per-renderer backend selection, and
alternative (non-GL) rendering paths.  All new methods have safe default
implementations so existing managers continue to compile and work unchanged.

### New virtual methods

#### `getActualSurfaceSize()` — framebuffer safety

```cpp
virtual void getActualSurfaceSize(void * context,
                                  unsigned int & width,
                                  unsigned int & height) const;
// Default: width = height = 0  ("unknown")
```

**This is the most important new method to implement.**

Before calling `glReadPixels()`, Obol queries the actual pixel dimensions of the
backing surface.  If the surface is smaller than the requested render target and
FBO support is unavailable, Obol skips the readback and emits a diagnostic
warning rather than writing past the end of the allocation.

The default value of `{0,0}` (unknown) is safe: Obol falls back to relying on
FBO creation success/failure as its guard.  Override this method in any manager
whose backing surface can be smaller than the largest texture an application might
request — for example, a manager that creates a 1×1 Pbuffer as a GL context
anchor while using FBOs for actual rendering.

#### `maxOffscreenDimensions()` — backend size ceiling

```cpp
virtual void maxOffscreenDimensions(unsigned int & width,
                                    unsigned int & height) const;
// Default: width = height = 0  ("probe via global GL pipeline")
```

Reports the maximum offscreen render dimensions this backend supports.  Used by
`CoinOffscreenGLCanvas` to avoid requesting surfaces larger than the backend can
provide.  An OSMesa manager should return a large value such as 16384×16384 since
OSMesa is only RAM-limited.  The default `{0,0}` causes the canvas to probe via
the global GL pipeline (backward-compatible behaviour).

#### `isOSMesaContext()` — dual-GL dispatch

```cpp
virtual SbBool isOSMesaContext(void * context);
// Default: FALSE
```

In dual-GL builds (`OBOL_USE_SWRAST=ON` + `OBOL_USE_SYSTEM_GL=ON`), both system GL and OSMesa are compiled into the
same library.  Return `TRUE` for contexts created via OSMesa so that the GL-glue
dispatch layer routes calls to the `osmesa_SoGLContext_*` implementation rather
than the system-GL implementation.  Only meaningful in dual-GL builds; the default
`FALSE` is correct for all single-backend managers.

#### `getProcAddress()` — extension function resolution

```cpp
virtual void * getProcAddress(const char * funcName);
// Default: nullptr  (falls back to dlsym)
```

Called by the GL glue layer when `dlsym()` cannot locate an extension entry
point.  Override to delegate to the platform-specific resolver:
- X11/GLX: `glXGetProcAddress()`
- Windows: `wglGetProcAddress()`
- EGL: `eglGetProcAddress()`
- OSMesa: `OSMesaGetProcAddress()`

In dual-GL builds this is essential: routing OSMesa extension lookups through
`OSMesaGetProcAddress()` prevents the system-GL loader from returning a system-GL
function pointer for an OSMesa extension, which would cause incorrect behaviour
or crashes.

#### `renderScene()` — alternative render path

```cpp
virtual SbBool renderScene(SoNode * scene,
                           unsigned int width, unsigned int height,
                           unsigned char * pixels,
                           unsigned int nrcomponents,
                           const float background_rgb[3]);
// Default: FALSE  (use GL path)
```

When this returns `TRUE`, `SoOffscreenRenderer` uses `pixels` directly and
bypasses the entire OpenGL pipeline.  `pixels` is a pre-allocated buffer of
`width*height*nrcomponents` bytes in top-to-bottom row order, matching
`SoOffscreenRenderer::getBuffer()`.

Implement this to provide a CPU raytracing backend (NanoRT, Embree, OptiX) or a
Vulkan rasterization backend.  Obol ships three reference implementations:

| Class | Backend | Header |
|-------|---------|--------|
| `SoNanoRTContextManager` | NanoRT CPU raytracing (bundled) | `tests/utils/nanort_context_manager.h` |
| `SoEmbreeContextManager` | Intel Embree 4 (system library) | `tests/utils/embree_context_manager.h` |
| `SoVulkanContextManager` | Vulkan rasterization | `tests/utils/vulkan_context_manager.h` |

All three delegate scene collection to `SoSceneCollector`; see
`docs/CONTEXT_MANAGEMENT_API.md` for the full integration guide.

### Per-renderer context managers

`SoOffscreenRenderer` now accepts a `ContextManager*` in its constructors,
enabling independent backend selection per renderer:

```cpp
// Coin (upstream) — one global context manager, no per-renderer choice
SoOffscreenRenderer renderer(viewport);

// Obol — per-instance backend selection
CoinOSMesaContextManager osmesa_cm;
SoOffscreenRenderer renderer(&osmesa_cm, viewport);

// Switch backends at runtime:
renderer.setContextManager(&other_cm);  // non-NULL: use this manager
renderer.setContextManager(nullptr);    // NULL: revert to global singleton
```

### Built-in OSMesa factory

`SoDB::createOSMesaContextManager()` returns a fully-configured OSMesa manager
without requiring `<OSMesa/osmesa.h>` in application code.  Returns `nullptr`
when the library was built without OSMesa support:

```cpp
auto mgr = std::unique_ptr<SoDB::ContextManager>(
               SoDB::createOSMesaContextManager());
if (mgr) {
    SoOffscreenRenderer renderer(mgr.get(), viewport);
    renderer.render(root);
}
```

### Why

These changes were driven by two needs:
1. **Safety hardening** — `getActualSurfaceSize()` prevents `glReadPixels()`
   from writing past the end of a small backing surface when FBO support is
   unavailable.
2. **Dual-GL support** — `isOSMesaContext()` and `getProcAddress()` give the
   GL dispatch layer the information it needs to route calls to the correct
   backend without any global state change.

---

## 24. New: Dual-GL Architecture (System GL + OSMesa in One Library)

### Overview

Obol can be built so that **both system OpenGL and OSMesa are available in the
same shared library**, selectable per `SoOffscreenRenderer` instance at runtime.
This mode is activated by setting both `OBOL_USE_SWRAST=ON` and
`OBOL_USE_SYSTEM_GL=ON` (auto-enabled when both system GL and the
`external/osmesa` submodule are detected and neither option is overridden).

This might seem surprising — having two conflicting GL implementations in the
same binary is normally a recipe for chaos.  This document explains why it is
safe and how it works.

### The Problem

OSMesa defines the same OpenGL entry-point names as system GL
(`glBegin`, `glGenTextures`, …).  Naïvely linking both into the same `.so`
would produce duplicate symbol errors at link time, and even if it somehow
linked, every GL call in the process would go to one implementation regardless
of which context is "current".

### The Solution: Symbol Namespacing

Obol borrows the technique used by BRL-CAD for embedding a private `zlib`
alongside the system `zlib`: the secondary implementation is compiled with
every symbol renamed.

The key file is `src/glue/gl_osmesa.cpp`:

```cpp
// Step 1: use OSMesa headers instead of system <GL/gl.h>
#define OBOL_GLHEADERS_OSMESA_OVERRIDE 1

// Step 2: activate MGL name mangling
// After <OSMesa/gl_mangle.h> is included, every gl* call becomes mgl*
// (OSMesaGetProcAddress("glBegin") → OSMesaGetProcAddress("mglBegin"))
#define USE_MGL_NAMESPACE 1

// Step 3: give every SoGLContext_* function an osmesa_ prefix
#define SOGL_PREFIX_SET 1
#define SOGL_PREFIX_STR osmesa_

// Step 4: compile the main GL glue layer with the above defines active
#include "gl.cpp"
```

This single-TU trick produces an object file that:
- Calls `mgl*` (OSMesa-namespaced) GL functions internally
- Exports `osmesa_SoGLContext_*` symbols (e.g. `osmesa_SoGLContext_glGenTextures`)

The main `gl.cpp` object (compiled normally) exports `SoGLContext_*` symbols
calling system GL (`glGenTextures`, …).

Both objects link into the same `.so` with no symbol collision because:
- The system-GL object uses `glGenTextures` (resolved from `libGL.so`)
- The OSMesa object uses `mglGenTextures` (resolved from the OSMesa library,
  which exports `mgl*` via its own `gl_mangle.h`)

### Runtime Dispatch

A thin dispatch layer in `gl.cpp` (compiled in dual-GL mode) keeps the
stable `SoGLContext_*` API working at runtime:

```
Application calls SoGLContext_glGenTextures(ctx, ...)
     │
     └─► dispatch layer checks: coingl_is_osmesa_context(ctx)?
              │YES: osmesa_SoGLContext_glGenTextures(ctx, ...)  ← calls mglGenTextures
              └NO:  (inline) system glGenTextures(...)
```

The dispatch decision is based on a per-context flag set at context-creation
time via `ContextManager::isOSMesaContext()`.  The `CoinOSMesaContextManager`
returns `TRUE` from this method; the `FLTKContextManager` returns `FALSE`.

### Why `USE_MGL_NAMESPACE` Is Not Set Globally

In the software-rasterizer-only build (`OBOL_USE_SWRAST=ON`, `OBOL_USE_SYSTEM_GL=OFF`),
`USE_MGL_NAMESPACE` is set as a `target_compile_definition` on the Obol library
target only — not via `add_definitions()`.  This prevents the mangle header from
being activated in test executables or viewer code that includes `<GL/glx.h>`
(which conditionally includes `glx_mangle.h`, a file that does not exist on
modern Ubuntu 24.04).

In dual-GL builds, `USE_MGL_NAMESPACE` is **not** set globally at all; it is
activated only inside `gl_osmesa.cpp` via the `#define` at the top of that file,
ensuring complete isolation.

### Why Is It Safe?

1. **No linker collisions** — the `osmesa_` prefix on exported symbols and the
   `mgl*` prefix on internal calls ensure the two GL code paths never share a
   symbol name.
2. **No runtime contamination** — every GL call goes through the dispatch layer,
   which uses the per-context backend flag.  An OSMesa render never calls
   system-GL entry points; a system-GL render never calls `mgl*` entry points.
3. **Extension pointers are backend-local** — `ContextManager::getProcAddress()`
   routes OSMesa extension lookups through `OSMesaGetProcAddress()`, so the
   function pointer table for an OSMesa context always points into OSMesa, not
   system GL.
4. **No thread-local GL state leakage** — OSMesa maintains its own current-context
   state separate from the system GL current-context (which is typically
   maintained by GLX/WGL/CGL).  Making an OSMesa context current does not
   disturb any system-GL context that another thread or panel may be using.

### When to Use Which Mode

| Scenario | Recommended option |
|----------|--------------------|
| Headless CI / servers with no GPU | `-DOBOL_USE_SWRAST=ON -DOBOL_USE_SYSTEM_GL=OFF` |
| Desktop with GPU; no headless fallback needed | `-DOBOL_USE_SYSTEM_GL=ON -DOBOL_USE_SWRAST=OFF` |
| Desktop with GPU + need for per-renderer OSMesa | `-DOBOL_USE_SWRAST=ON -DOBOL_USE_SYSTEM_GL=ON` (or leave both at auto-detect defaults) |
| Embedded / custom GL driver, no system GL | `-DOBOL_USE_SWRAST=OFF -DOBOL_USE_SYSTEM_GL=OFF` + custom `ContextManager` |

For detailed information on implementing a context manager for any of these
scenarios, see `docs/CONTEXT_MANAGEMENT_API.md`.
