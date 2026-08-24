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

These commands run the modern default suite.  The historical standalone GL
render probes and Mentor image checks are deliberately opt-in:

```sh
cmake -S . -B .build-legacy -DOBOL_BUILD_LEGACY_TESTS=ON \
  -DOBOL_BUILD_EXAMPLE_TESTS=ON
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

Performance characterization is a separate opt-in executable with no test
registry dependency:

```sh
cmake -S . -B .build-bench -DOBOL_BUILD_BENCHMARKS=ON
cmake --build .build-bench --target obol_scalability_benchmark
./.build-bench/bin/obol_scalability_benchmark
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

## Compatibility lane

`OBOL_BUILD_LEGACY_TESTS=ON` builds the historical standalone GL render
probes and their shared scene factories.  They are labelled `legacy;visual`
and are useful for backend bring-up or image-control maintenance, but they
are not the authoritative unit or conformance suite.  The old registry-based
unit wrapper and `obol_unittest` executable have been removed; new coverage
belongs in GTest.

The CAD IDs, assembly records, resolved-draw oracle, CPU-picking tests, base
math, geometry, value types, and base utilities, fields, core engines, nodes,
actions (including ray picking), scene management, paths, events, in-memory
scene-I/O, sensor, and offscreen-renderer API suites, plus the high-contention
thread suite, have completed their primary migration. The former primitives,
materials, transforms, and offscreen-rendering wrappers are now fixture-backed
OSMesa feature contracts; their viewer scenes live only in
`examples/demo_scenes`.

Legacy screenshot and backend programs are labelled `legacy;visual` rather
than `render`.  Consequently `ctest -L render` is a portable modern
conformance lane; select legacy visual coverage explicitly with
`ctest -L legacy` or `ctest -L visual` in an environment that provides its
declared GL backend.

## Demos

The FLTK viewers use `examples/demo_scenes`, not test fixtures.  Add a demo
scene when it helps a user explore a feature or compare render backends.  Demo
metadata describes title, category, backend support, and scene construction;
it must never contain test callbacks or test assertions.
