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
cmake -S . -B .build -DOBOL_BUILD_TESTS=ON
cmake --build .build
ctest --test-dir .build -L unit --output-on-failure
ctest --test-dir .build -L integration --output-on-failure
ctest --test-dir .build -L render --output-on-failure
```

The modern suite uses GTest's CTest discovery.  A test process receives an
explicit `SoDB::ContextManager`; non-rendering tests use a no-op manager and
render fixtures select either OSMesa or native GLX through their CTest lane.
Tests must not change the global rendering backend as part of ordinary setup.
Test configuration prefers an installed GoogleTest 1.10 or newer and falls
back to the pinned `external/googletest` submodule. Recursive repository
checkouts already populate it; otherwise run
`git submodule update --init external/googletest`.

Stress and sanitizer-oriented checks are built explicitly, so a normal CTest
run stays fast and repeatable:

```sh
cmake -S . -B .build-stress \
  -DOBOL_BUILD_TESTS=ON -DOBOL_BUILD_STRESS_TESTS=ON
cmake --build .build-stress --target obol_stress_tests
ctest --test-dir .build-stress -L stress --output-on-failure
```

## Adding a test

Put a new test in the smallest appropriate layer and add its source to the
matching manifest under `tests/framework/cmake/`. Use standard GTest `TEST`
or `TEST_F` cases. Do not add new `REGISTER_TEST` entries, standalone `main()`
functions, or CMake-generated main wrappers.

Rendering tests first assert a semantic contract: render succeeded,
the image has expected dimensions, known pixels/regions are present, or a
measured property is within tolerance.  Add a golden image only when visual
appearance itself is the contract. Sequence tests must compare every frame.

For golden-image cases, load the PNG with
`ObolTestSupport::loadRgbPng()`, compare it with `compareRgb()`, and assert
against an explicit `ImageTolerance`.  The helper reports differing pixels,
maximum channel error, and RMS error; there is no suite-wide hidden threshold.
The retained files in `tests/control_images/` are GLX references. Portable
scenes compare against them with an explicit RMS tolerance in both render
lanes. Bump mapping, 3-D textures, and GLSL shadows retain native-GL golden
coverage while the software lane checks their deterministic semantic and
pixel-region contracts. New backend-specific references belong below
`tests/rendering/golden/<backend>/`; update either set only through a reviewed,
intentional render change.

The CAD IDs, assembly records, resolved-draw oracle, CPU-picking tests,
subpixel/progressive rendering contracts, base math, geometry, value types,
and base utilities, fields, core engines, nodes, actions (including ray
picking), scene management, paths, events, in-memory scene-I/O, sensor, and
offscreen-renderer API suites, plus the high-contention thread suite, have
completed their primary migration. The former primitives,
materials, transforms, and offscreen-rendering wrappers are now fixture-backed
OSMesa feature contracts; their viewer scenes live only in
`examples/demo_scenes`.

The former standalone rendering sources now register one independently
discoverable GTest scenario per source in each enabled backend target. Their
scene construction, rendering, picking, interaction, and pixel contracts
remain covered. They call scenario code directly and use build-local output
stems only for optional diagnostic images; there is no command-line or
standalone-program compatibility adapter. Non-rendering source coverage is
split into ordinary independently filterable tests in `obol_unit_tests`,
including typed tests for the field initialization matrix. There are no
aggregate result recorders, per-source executables, or generated test-main
adapters in the active test graph.

Run the complete modern lanes explicitly when needed:

```sh
cmake --build .build --target obol_unit_tests obol_render_tests
ctest --test-dir .build -L 'unit|render' --output-on-failure
```

Performance characterization remains outside CTest. Configure with
`OBOL_BUILD_BENCHMARKS=ON` to build `obol_scalability_benchmark` and, when a
rendering backend is available, `obol_cad_render_benchmark`.

Native OpenGL conformance requires an X server.  On headless Linux, run the
system-GL lane through Xvfb:

```sh
cmake -S . -B .build-glx -DOBOL_BUILD_TESTS=ON \
  -DOBOL_USE_SYSTEM_GL=ON -DOBOL_USE_SWRAST=OFF
cmake --build .build-glx --target obol_system_gl_render_tests
xvfb-run -a ctest --test-dir .build-glx -L system-gl --output-on-failure
```

## Demos

The FLTK viewers use `examples/demo_scenes`, not test fixtures.  Add a demo
scene when it helps a user explore a feature or compare render backends.  Demo
metadata describes title, category, backend support, and scene construction;
it must never contain test callbacks or test assertions.
