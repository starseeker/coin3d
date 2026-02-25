# Obol - minimalist rework of Coin

## Background

Obol is an OpenGL-based, 3D graphics library that has its roots in the the Coin
implementation of the Open Inventor 2.1 API.  Open Inventor was a scene graph
based, retained mode, rendering and model manipulation API originally designed
by SGI.  It quickly became the de facto standard graphics library for 3D
visualization and visual simulation software in the scientific and engineering
community after its release.  Coin implements this API, but was developed from
scratch independently before SGI Open Inventor became open source.

**Note**: Obol does not maintain API compatibility with either Coin or Open
Inventor 2.1 itself.  There are a number of changes, including the deliberate
removal of all VRML/XML related code,, but the one users will see most
immediately strict context management requirements - i.e you need a
`ContextManager*` parameter for `SoDB::init()` - this isolates the platform
specific elements of OpenGL interaction.  Most modern toolkits have their own
solutions to such things, so Obol leverages what they do rather than being in
the business of defining specific toolkit interactions.  For event driven
interactions, the application needs to translate its specific events into
Obol's terms.  See `docs/API_DIFFERENCES.md` for details on the new public API
and how it differs from Coin's.

The upstream sources we started from can be found in the "upstream" top level
directory, if they need to be referred to when debugging changes

## Why call this Obol?

The name Obol itself is a reference to an ancient small denomination Greek coin
worth one sixth of a drachma - a play on the upstream Coin project
(https://github.com/coin3d/coin) name, the relatively minimalist goals of this
effort, and the age of the library itself.

The upstream Coin3d project has a number of dependencies and features that are
orthogonal to my interest in this code - I am investigating whether it would
make a good API to target for a CAD 3D scene manager.  We have our own data in
memory to use, so we don't need things like file I/O, and we want the core
aspects of this API to be as self-contained and dependency free as possible.
The original code, developed decades ago, had to work around many issues modern
standards like C++17 have addressed - there should be many opportunities for
simplification, stripping down, and cleaning up.

Since "invasive" would be a mild word for what this will entail, and we are not
overly concerned with strict compatibility (binary or source) with either
Coin3D or Open Inventor itself, it makes sense to rename the library.  We're
not (so far at least) looking to make major changes to the pieces we want to
keep, beyond those needed for modernization or simplicity, but neither are we
concerned about making such expeditious changes if it seems useful.  We're also
willing to tear things down and build them back up to working again, so for a
while at least don't expect stability out of this effort.  If it ever reaches
that point we'll update the README!

The age is actually to our advantage in at least one respect - because it's
OpenGL backend does not need (at least not for the features we are targeting)
OpenGL beyond version 2.0, the https://github.com/starseeker/osmesa offscreen
rasterizer should (in principle) be able to support it in a completely
offscreen fallback mode.  That in turn means that when we use osmesa as our
fallback rendering layer of last resort, we should be able to support the full
feature stack of Coin3D (or at least Obol) we want to use in both system and
fallback modes, with the only loss in fallback being speed.

## License and trademarks

BSD License (c) Kongsberg Oil & Gas Technologies AS

OpenGL and Open Inventor are trademarks of SGI Inc. 

