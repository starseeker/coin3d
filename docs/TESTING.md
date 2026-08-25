# Testing Obol

Obol's test and demonstration code are deliberately separate:

| Area | Purpose | Default CI lane |
| --- | --- | --- |
| `tests/unit` | Deterministic API and math behavior | `unit` |
| `tests/integration` | Scene graph traversal, actions, events, and I/O | `integration` |
| `tests/rendering` | Explicit backend conformance | `render` |
| `tests/threads` | Stress and sanitizer-oriented checks | `stress` |
| `examples/demo_scenes` | Curated application-facing scenes | example build |
| `examples/Mentor` | Retained LGPL educational examples | opt-in example lane |

## Normal commands

```sh
cmake -S . -B .build
cmake --build .build
ctest --test-dir .build -L unit --output-on-failure
ctest --test-dir .build -L integration --output-on-failure
ctest --test-dir .build -L render --output-on-failure
```

The modern suite uses GTest's CTest discovery.  A test process receives an
explicit `SoDB::ContextManager`; non-rendering tests use a no-op manager and
render tests own a per-fixture OSMesa manager.  Tests must not change the
global rendering backend as part of ordinary setup.

Stress and sanitizer-oriented checks are built explicitly, so a normal CTest
run stays fast and repeatable:

```sh
cmake -S . -B .build-stress -DOBOL_BUILD_STRESS_TESTS=ON
cmake --build .build-stress --target obol_stress_tests
ctest --test-dir .build-stress -L stress --output-on-failure
```

## Adding a test

Put a new test in the smallest appropriate layer and add its source to
`tests/framework/CMakeLists.txt`.  Use standard GTest `TEST` or `TEST_F`
cases.  Do not add new `REGISTER_TEST` entries, standalone `main()` functions,
or CMake-generated main wrappers.

Rendering tests should first assert a semantic contract: render succeeded,
the image has expected dimensions, known pixels/regions are present, or a
measured property is within tolerance.  Add a golden image only when visual
appearance itself is the contract.  References must be backend-specific, and
sequence tests must compare every frame.

For golden-image cases, load the PNG with
`ObolTestSupport::loadRgbPng()`, compare it with `compareRgb()`, and assert
against an explicit `ImageTolerance`.  The helper reports differing pixels,
maximum channel error, and RMS error; there is no suite-wide hidden threshold.
Keep references beneath `tests/rendering/golden/<backend>/` and update them
only through a reviewed, intentional render change.

The CAD IDs, assembly records, resolved-draw oracle, CPU-picking tests, base
math, geometry, value types, and base utilities, fields, core engines, nodes,
actions (including ray picking), scene management, paths, events, in-memory
scene-I/O, sensor, and offscreen-renderer API suites, plus the high-contention
thread suite, have completed their primary migration. The former primitives,
materials, transforms, and offscreen-rendering wrappers are now fixture-backed
OSMesa feature contracts; their viewer scenes live only in
`examples/demo_scenes`.

The former standalone rendering sources now register GTest cases directly in
`obol_render_tests`; their original scene construction, rendering, picking,
interaction, and return-code checks remain covered.  The upstream-derived
non-rendering sources likewise register directly in `obol_unit_tests`, so all
of the preserved coverage uses the same GTest/CTest discovery and support
library as the focused suites.  There are no per-source executables or
generated test-main adapters in the active test graph.

Run the complete modern lanes explicitly when needed:

```sh
cmake --build .build --target obol_unit_tests obol_render_tests
ctest --test-dir .build -L 'unit|render' --output-on-failure
```

## Demos

The FLTK viewers use `examples/demo_scenes`, not test fixtures.  Add a demo
scene when it helps a user explore a feature or compare render backends.  Demo
metadata describes title, category, backend support, and scene construction;
it must never contain test callbacks or test assertions.
