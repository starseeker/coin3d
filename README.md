# Obol - 3D Scene Display and Interaction Library

![Obol_Demo](docs/obol_viewer.png)
![Obol_Demo_Moving](docs/obol_viewer_moving.png)

## Introduction

Obol is a 3D graphics library intended to support easy definition of and
interaction with geometric scenes.  It largely implements the Open Inventor
2.1 API, but does not maintain strict compatibility - see ![API_DIFFERENCES](docs/API_DIFFERENCES.md)
for more details.

Major design philosophies include broad cross platform compatibility and minimal
external dependencies.  Most modern toolkits provide their own means
of integrating with system OpenGL contexts or other means of 3D display
visualization - Obol accordingly provides an API to allow the application
to specify how to use their display mechanisms and works within that framework.
There are multiple worked examples in the ![examples](examples) directory.

## Requirements

Obol needs a C++17 compiler.  Further requirements depend on what backends
the user is interested in - typically, you'll want a system OpenGL available
for the best combination of features and performance.  The libosmesa backend
provides a software-only OpenGL 2.0 + extensions stack that can support most
of what Obol needs, but it will be slower.  Integration with other backends
is also possible - see the ![examples](examples) for raytracing and Vulkan based
approaches.

## Why call this Obol?

The name Obol refers to an ancient small-denomination Greek coin worth one
sixth of a drachma — a play on the name of the upstream [Coin](https://github.com/coin3d/coin)
project that Obol was originally derived from.  Obol has a much smaller
scope than Coin and its primary OpenGL code is fixed function pipeline based
so a small, ancient coin seemed like a good fit.

## Documentation

* ![docs/API_DIFFERENCES.md](docs/API_DIFFERENCES.md) — comprehensive API migration guide (Obol vs. Coin)
* ![docs/BACKEND_SURVEY.md](docs/BACKEND_SURVEY.md) — Documents which parts of the API can work without OpenGL
* ![docs/CONTEXT_MANAGEMENT_API.md](docs/CONTEXT_MANAGEMENT_API.md) — `SoDB::ContextManager` API reference with worked examples

## License and trademarks

Most Obol code is from Coin, and maintains its copyright and license:

BSD License (c) Kongsberg Oil & Gas Technologies AS

Contributions by the Obol project are under the same terms and license as
Coin.  Embedded code from other sources has its own licenses, but we do not
include any code incompatible with the BSD license in core Obol - see ![COPYING](COPYING)
for details.  The Mentor example files are LGPL.

OpenGL and Open Inventor are trademarks of SGI Inc.

