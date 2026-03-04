# Test Consolidation Plan

## Overview

This document tracks the ongoing reorganization of the Obol test suite into the
proposed two-tier framework. Sessions should update the status tables and
check off completed items as work progresses.

## Target Architecture

```
obol_unittest     – non-graphical unit tests; subcommand pattern
                    Usage: obol_unittest [list|run <name|cat>|help]

obol_render       – rendering/visual tests; produces control images
                    Usage: obol_render [list|run <name> <outpath>|run_all <dir>|help]
                    Single image OR a series of images per test.

obol_viewer       – interactive FLTK viewer (unchanged)
                    Shows all render tests with full interactivity.
```

**Key constraint**: every `obol_render` test must also work in `obol_viewer` with
meaningful interactivity.  Tests that currently show only a static scene must be
updated to include interactive elements (pick targets, draggers, animated content,
etc.) so the viewer user can see the feature in action.

---

## Implementation Status

### Phase 1 — Infrastructure (New Executables)

- [x] Add `render_sequence` field to `TestEntry` (test_registry.h)
- [x] Create `tests/obol_unittest.cpp`
- [x] Create `tests/obol_render.cpp`
- [x] Update `tests/CMakeLists.txt` to build `obol_unittest` and `obol_render`
- [x] Update `tests/rendering/CMakeLists.txt` — point `add_rendering_image_test`
      at `obol_render` instead of `obol_test`

### Phase 2 — Shadow Test Consolidation

The `render_shadow.cpp` test uses `ObolTest::Scenes::createShadow` (testlib
factory) and is already `add_testlib_rendering_test`.

The `render_shadow_advanced.cpp` test builds its **own** 4-scenario scene
inline, duplicating scene-graph logic already present in
`ObolTest::Scenes::createShadowAdvanced`.  The `obol_viewer` renders
`createShadowAdvanced` correctly.

- [x] Refactor `render_shadow_advanced.cpp` to use `createShadowAdvanced`
      factory for its primary image and retain inline sub-tests as validation
      passes only (no duplicated scene graph).
- [x] Change `add_rendering_test(render_shadow_advanced)` →
      `add_testlib_rendering_test(render_shadow_advanced)`.

### Phase 3 — Standalone → Testlib Render Test Conversions

The 62 standalone `add_rendering_test` tests below each build their own scene
graph, despite having matching `create_scene` factories registered in the
`TestRegistry`.  Converting them to use the shared factory ensures the viewer
and the CLI image-generation path render **identical scenes**.

**Important constraint**: Tests that have existing control images
(`tests/control_images/<name>_control.png`) cannot be converted simply by
swapping in the testlib factory.  The testlib factories call `cam->viewAll()`
after setting a fixed camera position, whereas the original standalone tests
used fixed camera positions only.  The `viewAll` call adjusts the camera
distance to frame the scene's bounding sphere, producing a different (larger)
rendered object.  The RMSE difference between factory output and the original
control image can exceed the 15.0 RMSE threshold.

**Conversion strategy for tests with control images:**
1. Convert the test source to use the factory.
2. Render the new output and commit updated control images alongside the source.
3. Widen the RMSE threshold in `add_rendering_image_test` if the factory's
   `viewAll` camera framing produces measurably different output.
   Alternatively, update the factory to match the old camera framing.

Tests without control images can be converted freely (done = ✅, safe = 🔓).

Priority order: tests whose standalone scenes already closely match the factory
output go first (marked **easy**).  Tests that run multiple sub-scenarios and
cannot be represented as a single scene go last (marked **hard**).

| Render test                        | Factory in testlib?       | Has control image | Difficulty | Status                |
|------------------------------------|---------------------------|-------------------|------------|-----------------------|
| render_shadow_advanced             | createShadowAdvanced      | No                | medium     | ✅ Done                |
| render_cubemap                     | createCubemap             | No                | easy       | ✅ Done                |
| render_gl_big_image                | createGLBigImage          | No                | easy       | ✅ Done                |
| render_multi_texture               | createMultiTexture        | No                | easy       | ✅ Done                |
| render_switch_visibility           | createSwitchVisibility    | No                | easy       | ✅ Done                |
| render_sorender_manager            | createSoRenderManager     | No                | easy       | ✅ Done                |
| render_gl_features                 | createGLFeatures          | No                | easy       | ✅ Done                |
| render_sogl_bindings               | createSOGLBindings        | No                | easy       | ✅ Done                |
| render_material_binding            | createMaterialBinding     | No                | easy       | ✅ Done                |
| render_image_deep                  | createImageDeep           | No                | easy       | ✅ Done                |
| render_quad_mesh_deep              | createQuadMeshDeep        | No                | easy       | ✅ Done                |
| render_offscreen                   | createOffscreen           | No                | easy       | ✅ Done                |
| render_offscreen_advanced          | createOffscreenAdvanced   | No                | medium     | ✅ Done                |
| render_render_manager_full         | createRenderManagerFull   | No                | medium     | ✅ Done                |
| render_glrender_deep               | createGLRenderDeep        | No                | medium     | ✅ Done                |
| render_glrender_action_modes       | createGLRenderActionModes | No                | medium     | ✅ Done                |
| render_view_volume_ops             | createViewVolumeOps       | No                | medium     | ✅ Done                |
| render_field_connections           | createFieldConnections    | No                | medium     | ✅ Done                |
| render_engine_converter            | createEngineConverter     | No                | medium     | ✅ Done                |
| render_engine_interaction          | createEngineInteraction   | No                | medium     | ✅ Done                |
| render_sensors_rendering           | createSensorsRendering    | No                | medium     | ✅ Done                |
| render_sensor_interaction          | createSensorInteraction   | No                | medium     | ✅ Done                |
| render_path_operations             | createPathOperations      | No                | medium     | ✅ Done                |
| render_write_read_action           | createWriteReadAction     | No                | medium     | ✅ Done                |
| render_search_action               | createSearchAction        | No                | medium     | ✅ Done                |
| render_text3_parts                 | createText3Parts          | No                | medium     | ✅ Done                |
| render_arb8_draggers               | createArb8Draggers        | No                | medium     | ✅ Done                |
| render_rt_proxy_shapes             | createRTProxyShapes       | No                | medium     | ✅ Done                |
| render_raypick_shapes              | createRaypickShapes       | No                | medium     | ✅ Done                |
| render_ext_selection               | createExtSelection        | No                | medium     | ✅ Done                |
| render_ext_selection_events        | createExtSelectionEvents  | No                | medium     | ✅ Done                |
| render_nodekit_interaction         | createNodeKitInteraction  | No                | medium     | ✅ Done                |
| render_scene_interaction           | createSceneInteraction    | No                | medium     | ✅ Done                |
| render_camera_interaction          | createCameraInteraction   | No                | medium     | ✅ Done                |
| render_hud_interaction             | createHUDInteraction      | No                | easy       | ✅ Done                |
| render_event_callback_interaction  | createEventCallbackInteraction | No          | medium     | ✅ Done                |
| render_selection_interaction       | createSelectionInteraction | No               | medium     | ✅ Done                |
| render_pick_interaction            | createPickInteraction     | No                | medium     | ✅ Done                |
| render_pick_filter                 | createPickFilter          | No                | medium     | ✅ Done                |
| render_lod_picking                 | createLODPicking          | No                | medium     | ✅ Done                |
| render_callback_action             | createCallbackAction      | No                | hard       | ✅ Done                |
| render_callback_action_deep        | createCallbackActionDeep  | No                | hard       | ✅ Done                |
| render_callback_node               | createCallbackNode        | No                | hard       | ✅ Done                |
| render_event_propagation           | createEventPropagation    | No                | hard       | ✅ Done                |
| render_bbox_action                 | createBBoxAction          | No                | hard       | ✅ Done                |
| render_background_gradient         | createBackgroundGradient  | No                | easy       | ✅ Done                |
| render_shader_program              | createShaderProgram       | No                | medium     | ✅ Done                |
| render_shape_hints                 | createShapeHints          | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_environment                 | createEnvironment         | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_depth_buffer                | createDepthBuffer         | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_alpha_test                  | createAlphaTest           | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_scene_texture               | createSceneTexture        | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_procedural_shape            | createProceduralShape     | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_arb8_edit_cycle             | createArb8EditCycle       | **Yes**           | medium     | ✅ Done (ctrl regen)   |
| render_texture3                    | createTexture3            | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_bump_map                    | createBumpMap             | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_texture_transform           | createTextureTransform    | **Yes**           | easy       | ✅ Done (ctrl regen)   |
| render_scene_texture_multi_mgr     | createSceneTextureMultiMgr | No               | hard       | ✅ Done                |
| render_viewport                    | createViewport            | No                | hard       | ✅ Done                |
| render_quad_viewport               | createQuadViewport        | No                | hard       | ✅ Done                |
| render_quad_viewport_lod           | createQuadViewportLOD     | **Yes**           | hard       | ✅ Done (ctrl regen)   |
| render_viewport_scene              | createViewportScene       | **Yes**           | hard       | ✅ Done (ctrl regen)   |

### Phase 4 — Interactive Elements for Visual Tests

Every `obol_render` test must be interactable in `obol_viewer`.  Most tests
already have `has_interactive = true` and include pick targets or camera
controls.  The list below tracks tests that may need explicit interaction
support added (e.g., picking callback, visible state change, animated element).

**Currently `has_interactive = false`:**
- `hud` (the HUDOverlay factory) – needs button callbacks or animated HUD text

**Tests where current interactive support may be insufficient** (static scene,
nothing to "do" in the viewer beyond camera orbit):
- `gradient` – pure background; needs a foreground interactive element
- `colored_cube` – smoke test; fine for basic orbit but needs pick target
- `sphere_position` – diagnostic check; fine as-is
- `checker_texture` – needs at least a pick-to-toggle behavior
- `switch_visibility` – should show a way to toggle the switch interactively
- `shape_hints` – could benefit from a pick-to-toggle hints button
- `environment` – should show animated fog density or light change
- `depth_buffer` – needs an interactive depth comparison element
- `alpha_test` – needs pick-to-toggle alpha threshold visualization
- `view_volume_ops` – camera manipulation already interactive; fine as-is

**Tests with strong interactive requirements (render_sequence needed):**

These tests verify *behavioral* changes and need multiple frames to show
interactions.  `obol_render` should produce a frame sequence; `obol_viewer`
should support the corresponding live interactions.

| Test                         | Interaction sequence needed                                      |
|------------------------------|------------------------------------------------------------------|
| render_pick_interaction      | frame 0: no selection; frame 1: sphere picked (highlighted)     |
| render_pick_filter           | frame 0: no selection; frame 1: sphere picked; frame 2: blocked |
| render_selection_interaction | frame 0: no sel; frame 1: sphere sel; frame 2: multi-sel        |
| render_lod_picking           | frame 0: close; frame 1: medium dist; frame 2: far dist         |
| render_draggers              | frame 0: default pos; frame 1: translated; frame 2: rotated     |
| render_simple_draggers       | frame 0: default; frame 1: drag applied                         |
| render_light_manips          | frame 0: default; frame 1: light moved                          |
| render_manip_sequences       | frame 0: base; frame 1: manip attached; frame 2: moved          |
| render_arb8_draggers         | frame 0: default; frame 1: corner dragged                       |
| render_arb8_edit_cycle       | frame 0: step 0; frame 1-3: edit steps                          |
| render_camera_interaction    | frame 0: perspective; frame 1: ortho; frame 2: zoomed           |
| render_scene_interaction     | frame 0: base; frame 1: node added; frame 2: node removed       |
| render_ext_selection         | frame 0: no sel; frame 1: lasso; frame 2: bbox                  |
| render_ext_selection_events  | frame 0: no sel; frame 1: rect sel; frame 2: part bbox          |

### Phase 5 — Viewport / Quad-Viewport Tests (New Factories Needed)

These tests lack matching testlib factories entirely:

- `render_viewport` — needs `createViewport` factory
- `render_quad_viewport` — needs `createQuadViewport` factory  
- `render_scene_texture_multi_mgr` — needs `createSceneTextureMultiMgr` factory

---

## Current Known Issues

### Shadow Tests

The `render_shadow_advanced.cpp` standalone test creates `SoShadowGroup` with
`getCoinHeadlessContextManager()` in each sub-test, whereas
`ObolTest::Scenes::createShadowAdvanced` creates it without any argument.
The viewer successfully renders `createShadowAdvanced`, confirming the factory
is correct.  The standalone test's extra `getCoinHeadlessContextManager()` call
was added to work around an earlier issue and is no longer needed (the context
manager is set globally by `initCoinHeadless()` before any rendering).

**Action:** Done – see Phase 2 ✅.

### Missing `render_sequence` Support

Currently `TestEntry::create_scene` returns a single static `SoSeparator*`.
There is no mechanism to drive a sequence of rendered frames from a single test
registration.  Adding `render_sequence` (Phase 1, implemented) provides this
capability; individual tests still need their sequence callbacks populated
(Phase 4).

---

## Test Category Inventory

### Non-Graphical Unit Tests (→ obol_unittest)

Total: ~213 tests across 21 categories.

| Category   | Count | Example tests                                                  |
|------------|-------|----------------------------------------------------------------|
| Actions    | ~15   | bbox_action, ray_pick, search_action, callback_action         |
| Base       | ~30   | vec2f, vec3f, matrix, rotation, box3f, viewvolume, dict       |
| Bundles    | ~3    | bundle_basics                                                  |
| Caches     | ~3    | cache_basics                                                   |
| Details    | ~6    | details_suite, details_deep                                    |
| Draggers   | ~8    | dragger lifecycle, state                                       |
| Elements   | ~3    | element_stack                                                  |
| Engines    | ~15   | engine_basics, deep traversal                                  |
| Errors     | ~3    | error_handling                                                 |
| Events     | ~6    | event_dispatch, event_filter                                   |
| Fields     | ~20   | sf_fields, mf_fields, connections, containers                  |
| IO         | ~12   | read/write, binary IO, SoDB                                    |
| Lists      | ~3    | list_basics                                                    |
| Manips     | ~6    | manip_lifecycle                                                |
| Misc       | ~6    | misc_basics                                                    |
| Nodes      | ~30   | shape nodes, nodekits, selection, animation                    |
| Profiler   | ~3    | profiler_basics                                                |
| Projectors | ~6    | line_projector, plane_projector                               |
| Sensors    | ~8    | sensor_dispatch                                                |
| Threads    | ~3    | thread_safety                                                  |
| Tools      | ~3    | tools_basics                                                   |

### Rendering / Visual Tests (→ obol_render + obol_viewer)

Total: ~100 tests across all rendering scenarios.

**Sub-groups:**

- **Basic geometry** (primitives, face/line/point sets, quad mesh, strip set): 15
- **Materials & lighting** (materials, lighting, vertex colors, material binding): 6
- **Textures** (texture, texture3, bump map, multi texture, cubemap, scene tex): 8
- **Text rendering** (text2, text3, ascii_text, text_demo, text3_parts): 5
- **Transforms & cameras** (transforms, cameras, reset_transform, view_volume): 5
- **Effects** (shadow, shadow_advanced, transparency, alpha_test, depth_buffer): 6
- **Draggers & manipulators** (draggers, simple_draggers, light_manips,
  manip_sequences, arb8_draggers, arb8_edit_cycle): 6
- **Selection & picking** (pick_interaction, pick_filter, selection_interaction,
  ext_selection, ext_selection_events, lod_picking, raypick_shapes): 7
- **Actions & sensors** (bbox_action, search_action, callback_action, path_ops,
  write_read, field_connections, sensors_rendering, sensor_interaction): 10
- **HUD & UI** (hud, hud_overlay, hud_no3d, hud_interaction, hud_demo): 5
- **NanoRT / raytracing** (nanort, nanort_shadow, rt_proxy_shapes): 3
- **Procedural & special** (procedural_shape, shader_program, gl_big_image,
  gl_features, offscreen, render_manager, sorender_manager): 7
- **Viewport** (viewport, quad_viewport, quad_viewport_lod, viewport_scene): 4
- **Interaction demos** (camera_interaction, scene_interaction,
  event_callback_interaction, nodekit_interaction, engine_interaction,
  engine_converter): 6
- **Misc visual** (coordinates, sphere_position, gradient, colored_cube,
  checker_texture, annotation, background_gradient, switch_visibility,
  shape_hints, environment, scene, clip_plane, array_multiple_copy,
  image_node, image_deep, marker_set, lod): 17

---

## Session Log

### Session 1 (2026-03-03)
- Performed full analysis of existing test infrastructure (317 REGISTER_TEST
  calls, 100 visual / 213 unit, 41 testlib-backed render tests, 62 standalone)
- Identified shadow consolidation issue: `render_shadow_advanced.cpp` does not
  use `createShadowAdvanced` testlib factory
- Identified `render_sequence` gap: no mechanism for multi-frame interaction
  testing  
- **Completed:**
  - Created this document
  - Added `render_sequence` field to `TestEntry` in `test_registry.h`
  - Created `tests/obol_unittest.cpp` (non-graphical unit test runner)
  - Created `tests/obol_render.cpp` (rendering test runner)
  - Updated `tests/CMakeLists.txt` to build both new executables
  - Fixed `render_shadow_advanced.cpp` to use testlib factory +
    changed to `add_testlib_rendering_test`
  - Updated `tests/rendering/CMakeLists.txt` `add_rendering_image_test` macro
    to use `obol_render` instead of `obol_test`
  - Converted 3 tests without control images to use testlib factories:
    `render_cubemap`, `render_gl_big_image`, `render_multi_texture`
- **Key finding:** Tests with existing control images cannot be trivially
  converted — the testlib factories call `cam->viewAll()` after setting a
  fixed camera position, which moves the camera closer than the standalone
  test's fixed position, producing ~32 RMSE difference (threshold: 15.0).
  These tests need control image regeneration in a future session.

### Session 2 (2026-03-03)
- **Phase 3 bulk conversions (no control images):** 32 tests converted using
  standard basepath-injection pattern. All inject the factory scene render at
  the top of main() before existing behavioral sub-tests:
  - Standard basepath tests: field_connections, engine_converter, engine_interaction,
    sensors_rendering, sensor_interaction, text3_parts, ext_selection, nodekit_interaction,
    scene_interaction, camera_interaction, event_callback_interaction, selection_interaction,
    pick_interaction, pick_filter, lod_picking, bbox_action, callback_node, event_propagation,
    shader_program, offscreen_advanced, render_manager_full, glrender_deep,
    glrender_action_modes, view_volume_ops, search_action, callback_action,
    path_operations, callback_action_deep, ext_selection_events
  - Void-argc pattern: write_read_action, raypick_shapes
  - Inline-root pattern: arb8_draggers, hud_interaction, rt_proxy_shapes, background_gradient
- **Phase 3 control-image-regen tests (10 tests):**
  Converted and regenerated control images for: texture3, bump_map, texture_transform,
  shape_hints, environment, depth_buffer, alpha_test, scene_texture, procedural_shape,
  arb8_edit_cycle. Strategy: inject factory render as primary output, keep behavioral
  multi-frame tests for validation only (not written to primary outpath).
  For arb8_edit_cycle: factory writes primary `.rgb`; inline step 4 now writes to
  `_step4.rgb` (not compared to control) to avoid overwriting the factory output.
- **Phase 5 remaining (5 tests need new factories):**
  render_viewport, render_quad_viewport, render_quad_viewport_lod, render_viewport_scene,
  render_scene_texture_multi_mgr — no testlib factories exist yet.

### Session 3 (2026-03-04)
- **Phase 5 complete: all 5 remaining tests converted.**
  - Added 5 new scene factories to `test_scenes.cpp`/`test_scenes.h`:
    `createViewport`, `createViewportScene`, `createQuadViewport`,
    `createQuadViewportLOD`, `createSceneTextureMultiMgr`.
  - Added 5 new `REGISTER_TEST` entries in `unit_test_wrappers.cpp` (session 9 namespace).
  - Converted all 5 test files: factory render injected, SoViewport/SoQuadViewport
    includes preserved, `render_scene_texture_multi_mgr` gained basepath parsing.
  - Updated CMakeLists: all 5 changed from `add_rendering_test` → `add_testlib_rendering_test`.
  - Regenerated control images for `render_viewport_scene` and `render_quad_viewport_lod`.
  - **Zero remaining `add_rendering_test(` entries in CMakeLists — full migration complete.**
- **Final state:** All 62 former standalone render tests now use testlib factories.
  Every test registers a `create_scene` factory in `TestRegistry`, ensuring
  `obol_render`, `obol_viewer`, and the legacy `render_test` dispatch all render
  the same authoritative scene from a single source.

### Outstanding work (Phase 4)
- Phase 4: implement `render_sequence` callbacks for pick/selection/dragger tests.
  These tests have `has_interactive = true` but no multi-frame sequence support yet.
  Adding sequences would improve interactive coverage in `obol_viewer`, but the
  current state is complete for the Phase 3/5 consolidation goals.
