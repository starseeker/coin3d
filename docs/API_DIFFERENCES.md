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

---

## 1. Design Philosophy

Coin is a full-featured Open Inventor 2.1 implementation that targets GUI
toolkits (Qt, Xt/Motif, Win32), VRML/VRML97 file formats, audio, geospatial
data, and an array of other features that make it a broad-purpose scene graph.
Obol deliberately strips the library back to a self-contained core suitable for
a CAD scene-management back end:

* **No GUI toolkit dependencies** — the application owns the OpenGL context and
  feeds events in through Obol's abstract interfaces.
* **No VRML/XML/audio** — file-format I/O beyond Open Inventor `.iv` and in-memory
  scene construction is out of scope.
* **C++17 throughout** — platform shims written for C89 compilers are replaced
  with standard-library equivalents.
* **OSMesa / headless first** — full test coverage in a completely headless
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

`SoDB::ContextManager` is an abstract base class declared inside `SoDB`:

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

`SoDB::getContextManager()` returns the registered instance.
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

**Screen resolution detection** now returns a fixed 72 DPI instead of
querying the display server:

```cpp
// Coin (upstream) — queried display server
#ifdef HAVE_GLX
  pixmmres = SoOffscreenGLXData::getResolution();
#elif defined(HAVE_WGL)
  pixmmres = SoOffscreenWGLData::getResolution();
#endif

// Obol — portable constant
SbVec2f pixmmres(72.0f / 25.4f, 72.0f / 25.4f);
```

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
  means a WGL-based manager; on Linux a GLX- or EGL-based manager; on macOS a
  CGL-based manager.  OSMesa-based managers work on any platform.
* **Fixed DPI:** Applications that relied on Coin's automatic screen DPI
  detection for `SoOffscreenRenderer` pixel size calculations must now apply
  their own DPI scaling.

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
`-DCOIN_PROFILING=ON`**.  With that option off (the default) the entire
profiling code path is unreachable at runtime regardless of the `COIN_PROFILER`
environment variable.

### Why

Profiling instrumentation adds dead-code weight to every hot traversal path
(`SoSeparator`, `SoGroup`, `SoMaterial`).  Until the overhead has been
benchmarked against realistic scenes the subsystem is kept disabled by default
so that performance regressions cannot be silently introduced.

### User implications

`SoProfilingReportGenerator`, `SbProfilingData`, and related classes are
always compiled but are not reachable at runtime unless `-DCOIN_PROFILING=ON`
is specified at CMake configure time:

```bash
cmake -S . -B build_dir -DCOIN_PROFILING=ON
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

| CMake option | Default | Description |
|---|---|---|
| `COIN_PROFILING` | `OFF` | Enable profiling subsystem (`SoProfiler::isEnabled()` always FALSE when OFF) |
| `COIN3D_USE_OSMESA` | `ON` | Link OSMesa for offscreen/headless rendering |
| `COIN_BUILD_TESTS` | `ON` | Build the Catch2-based test suite |
| `COIN_BUILD_EXAMPLES` | `OFF` | Build example applications |
| `BUILD_SHARED_LIBS` | `ON` | Shared vs. static library |
| `COIN_THREADSAFE` | `ON` | Thread-safe render traversals |
| `COIN_USE_PRECOMPILED_HEADERS` | `ON` | PCH for faster incremental builds (CMake ≥ 3.16) |
| `HAVE_NODEKITS` | `ON` | NodeKit support |
| `HAVE_DRAGGERS` | `ON` | Dragger support |
| `HAVE_MANIPULATORS` | `ON` | Manipulator support |

**Minimum compiler standard:** C++17.

**Submodules are required** (`git clone --recurse-submodules`); the
`external/libpng` submodule provides the PNG library needed by
`SoOffscreenRenderer`.

**Out-of-source builds are enforced.**  Attempting to configure in the source
directory will fail immediately.

Upstream Coin's Autoconf/Automake build system is not present in Obol; only CMake is supported.

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
| **Fixed DPI** | Offscreen renderer uses 72 DPI constant | Apply application-level DPI scaling if needed |
| **PostScript/hardcopy** | Always compiled; API always available | No action needed; `SoVectorizePSAction` usable in all builds |
| **Profiling** | Always compiled; off by default (`-DCOIN_PROFILING=ON` to enable) | Add CMake option; `COIN_PROFILER` env var activates it when enabled |
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
| **Build system** | CMake only; C++17 required; submodules required | Update build scripts accordingly |
