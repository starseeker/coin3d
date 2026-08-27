# Migrating to Obol 2

Obol 2 intentionally changes the shared-library ABI and therefore has a new
package major version and SONAME. Applications and plugins must be rebuilt.

The `SoCallbackList` object layout remains stable, but its callback entries are
now owning, type-erased objects. The new typed `addCallback()` overloads avoid
calling a function through an incompatible function-pointer type. Because
those overloads are templates instantiated in consumers, Obol 2 headers must
not be mixed with an Obol 1 runtime.

`SoNanoRTContextManager` is now a compiled Obol component. Consumers continue
to include `<Obol/render/SoNanoRTContextManager.h>`, but no longer need NanoRT
headers or include paths. Remove direct dependencies on the old test/example
`nanort_context_manager.h` helpers.

The supported extension-loader model uses a shared Obol library. A plugin
linked to a separate static Obol archive has a different process-global type
registry and cannot register types in its host. Fully static applications can
still register extension types explicitly in the application.

The test tree now uses independently discoverable GoogleTest cases. The old
`CheckRecorder`, aggregate `RetainedCoverage` cases, and field-initialization
test macros have been removed. Use normal `TEST`, fixture, typed-test, or
parameterized-test registration for new coverage.
