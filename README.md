# Obol - minimalist rework of Coin

## Background

Obol is an OpenGL-based, 3D graphics library that has its roots in the Coin
implementation of the Open Inventor 2.1 API.  Open Inventor was a scene graph
based, retained mode, rendering and model manipulation API originally designed
by SGI.  It quickly became the de facto standard graphics library for 3D
visualization and visual simulation software in the scientific and engineering
community after its release.  Coin implements this API, but was developed from
scratch independently before SGI Open Inventor became open source.

**Note**: Obol does not maintain API compatibility with either Coin or Open
Inventor 2.1 itself.  There are a number of changes, including the deliberate
removal of all VRML/XML related code, but the one users will see most
immediately is the strict context management requirement — you must pass a
`ContextManager*` to `SoDB::init()`.  This isolates all platform-specific
OpenGL interaction in the application; most modern toolkits have their own
solutions to context management, so Obol leverages what they already do rather
than defining its own toolkit bindings.  For event-driven interactions the
application translates its own events into Obol's terms.  See
`docs/API_DIFFERENCES.md` for the complete API delta versus upstream Coin.

The upstream sources used as the starting point can be found in the `upstream/`
directory and are available for reference when debugging.

## Why call this Obol?

The name Obol refers to an ancient small-denomination Greek coin worth one
sixth of a drachma — a play on the upstream [Coin](https://github.com/coin3d/coin)
project name, the relatively minimalist goals of this effort, and the age of the
library itself.

The upstream Coin3D project has a number of dependencies and features orthogonal
to the primary use case here: a self-contained, dependency-light CAD scene
manager.  Since application data is already in memory there is no need for file
I/O, and the core API should be as portable and dependency-free as possible.
The original code, written for C89-era compilers, has many opportunities for
simplification and modernization under C++17.

Since the changes required are invasive and strict compatibility with Coin3D or
Open Inventor is not a goal, it makes sense to rename the library.  Expect
ongoing breaking changes until the codebase reaches a stable point.

Because Obol's OpenGL backend needs only OpenGL 2.0, the
[osmesa](https://github.com/starseeker/osmesa) offscreen rasterizer can serve as
a fully headless fallback.  This means the same feature set works in both
hardware-accelerated and headless/CI modes, with the only difference being
rendering speed.

## Documentation

* `docs/API_DIFFERENCES.md` — comprehensive API migration guide (Obol vs. Coin)
* `docs/BUILD_OPTIONS.md` — CMake build options reference
* `docs/CONTEXT_MANAGEMENT_API.md` — `SoDB::ContextManager` API reference with worked examples
* `docs/TESTING.md` — test suite overview and how to write new tests
* `docs/COIN_MIGRATION_PLAN.md` — modernization status and completed work log
* `docs/THREADING_MIGRATION.md` — C++17 threading migration details
* `docs/STORAGE_MIGRATION.md` — thread-local storage migration analysis and status
* `docs/PLATFORM_CLEANUP_SUMMARY.md` — platform-specific code removal summary

## License and trademarks

BSD License (c) Kongsberg Oil & Gas Technologies AS

OpenGL and Open Inventor are trademarks of SGI Inc.

