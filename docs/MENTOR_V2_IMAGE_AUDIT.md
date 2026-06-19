# Mentor v2 Image Audit

This audit compares the current v2 Mentor control images against the `main`
branch controls. Passing v2-vs-v2 image tests is not sufficient: this document
tracks whether visual differences preserve the original user-facing behavior or
represent regressions introduced during API migration.

## How to Reproduce

```sh
mkdir -p /tmp/obol_main_controls
git archive main examples/Mentor/control_images | tar -x -C /tmp/obol_main_controls

main_dir=/tmp/obol_main_controls/examples/Mentor/control_images
cur_dir=examples/Mentor/control_images
report=/tmp/obol_control_compare.tsv
: > "$report"
find "$main_dir" -name '*_control.png' | sort | while IFS= read -r main_img; do
  rel=${main_img#"$main_dir"/}
  cur_img="$cur_dir/$rel"
  if [ ! -f "$cur_img" ]; then
    printf 'MISSING_CURRENT\tNA\tNA\t%s\n' "$rel" >> "$report"
    continue
  fi
  out=$(build/tests/bin/image_comparator --verbose --threshold 64 --rmse 9999 "$main_img" "$cur_img" 2>&1 || true)
  hash=$(printf '%s\n' "$out" | sed -n 's/^Perceptual hash distance: \([0-9][0-9]*\).*/\1/p')
  rmse=$(printf '%s\n' "$out" | sed -n 's/^RMSE: \([0-9.][0-9.]*\).*/\1/p')
  printf 'COMMON\t%s\t%s\t%s\n' "${rmse:-NA}" "${hash:-NA}" "$rel" >> "$report"
done
find "$cur_dir" -name '*_control.png' | sort | while IFS= read -r cur_img; do
  rel=${cur_img#"$cur_dir"/}
  [ -f "$main_dir/$rel" ] || printf 'V2_ONLY\tNA\tNA\t%s\n' "$rel" >> "$report"
done
```

Current comparison summary:

| Chapter | Common | Changed | Avg RMSE | Max RMSE | Max-diff image | Initial assessment |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| 02 | 41 | 41 | 0.57 | 1.88 | `02.2.EngineSpin_frame06_control.png` | Fixed/aligned. Cone examples now preserve the original diffuse material and `viewAll()`-derived camera/viewer state while keeping v2 app-owned animation and camera updates. Remaining delta is rasterization noise. |
| 03 | 11 | 11 | 60.86 | 105.93 | `03.3.Naming_before_control.png` | Mixed. Molecule/Robot color regression was fixed. `03.3.Naming` separated cube/sphere composition is accepted as clearer v2 intent. |
| 04 | 10 | 10 | 33.56 | 37.69 | `04.2.Lights_frame01_control.png` | Mostly acceptable after light-state bridge fix; gradient behavior restored. |
| 05 | 23 | 23 | 1.25 | 6.23 | `05.6.TransformOrdering_front_control.png` | Fixed/aligned. FaceSet, IndexedFaceSet, TriangleStripSet, QuadMesh, Binding, and TransformOrdering now preserve original camera/light/material intent; native strip/quad mesh bridge paths avoid fallback normal artifacts. Remaining delta is minor rasterization/subpixel noise. |
| 06 | 10 | 10 | 6.50 | 16.40 | `06.2.Simple3DText_front_control.png` | Fixed/aligned. `06.1.Text` and `06.2.Simple3DText` now preserve the original `viewAll()`/orbit camera sequence and default lighting; `06.3.Complex3DText` preserves its explicit camera/orbit sequence while keeping the v2 text-profile fallback. Remaining delta is text rasterization and sphere/text tessellation shading noise. |
| 07 | 9 | 9 | 2.43 | 6.71 | `07.3.TextureFunction_angle_control.png` | Fixed/aligned. `07.1.BasicTexture`, `07.2.TextureCoordinates`, and `07.3.TextureFunction` now preserve the original texture data/UV behavior, `viewAll()`/orbit camera sequence, and default lighting. Remaining delta is minor rasterization/tessellation noise. |
| 08 | 16 | 16 | 20.95 | 29.60 | `08.2.UniCurve_top_control.png` | Intentional fallback, with fixes. NURBS examples are sampled/tessellated; `08.3`/`08.4` lighting/normals corrected. |
| 09 | 13 | 13 | 34.10 | 56.04 | `09.4.PickAction_pick_star2_control.png` | Improved. `09.1.Print` camera framing fixed; `09.2.Texture` source texture framing fixed; `09.4.PickAction` cube/star regression fixed. |
| 10 | 38 | 38 | 28.52 | 52.48 | `10.2.setEventCB_rotated_control.png` | Improved. `10.2.setEventCB` primary frame is back to the initial light-blue sphere and uses suffixed frames for event-state changes; `10.5.SelectionCB`, `10.6.PickFilterTopLevel`, and `10.7.PickFilterManip` framing/selection-state visibility fixed. `10.8.PickFilterNodeKit` remains visually close to `main`; manipulator geometry remains a first-class v2 interaction follow-up. |
| 11 | 2 | 2 | 0.94 | 1.18 | `11.1.ReadFile_control.png` | Fixed/aligned. Imported/string scenes use `viewAll()`-derived v2 cameras, and v2 lights are ordered before legacy fallback roots so `ReadFile` and `ReadString` match the original lit imported content. |
| 12 | 24 | 21 | 18.88 | 34.44 | `12.4.TimerSensor_frame08_control.png` | Improved. `12.1.FieldSensor` now preserves the original camera-position callback plus `viewAll()` render behavior, and `12.2.NodeSensor` uses real `Scene::removeObject()` with overlapping default cube/sphere geometry. Remaining deltas are `AlarmSensor`/`TimerSensor` presentation differences. |
| 13 | 109 | 77 | 20.37 | 51.99 | `13.4.Gate_enabled_04_control.png` | Improved. `13.1.GlobalFlds` time formatting fixed; `13.3.TimeCounter` intentionally shows clearer time progression with adjusted initial framing; `13.4.Gate` has explicit open/closed cues; `13.5.Boolean` view framing fixed; `13.7.Rotor` now uses the original windmill `.iv` assets through transformable v2 legacy-graph objects; `13.8.Blinker` camera framing fixed. Remaining max is the intentionally clearer Gate visualization. |
| 14 | 8 | 8 | 21.63 | 29.34 | `14.2.Editors_light_finish_control.png` | Improved. `14.2.Editors` desk geometry/camera were brought closer to the imported `desk.iv` view; v2 `light_off` remains intentionally darker because the light-off editor state is actually applied. |
| 15 | 12 | 12 | 21.64 | 33.46 | `15.3.AttachManip_frame05_cone_transformbox_control.png` | Improved. `15.1.ConeRadius` now uses `viewAll()`-derived framing, gray cone material, primitive-option radius edits, and a portable dragger proxy; `15.3.AttachManip` now matches main object spacing/framing more closely and uses a white transform-box proxy. Remaining delta is simplified manipulator handle geometry pending first-class manipulator/dragger support. |
| 16 | 11 | 11 | 8.78 | 18.03 | `16.3.AttachEditor_orange_control.png` | Fixed/aligned. Callback/editor examples now import the original `dogDish.iv` through transformable v2 legacy-graph objects, preserve red food, apply edited material through the inherited bowl material, and keep unsuffixed controls mapped to main's blue state. Remaining delta is minor material/shading noise. |
| 17 | 5 | 5 | 14.30 | 18.96 | `17.2.GLCallback_00_default_control.png` | Fixed/aligned. `17.2.GLCallback` uses `Scene::addOpenGLCallback()` and raw OpenGL drawing again, with legacy camera orientations and cumulative transform semantics preserved in v2 object state. Remaining delta is minor rasterization/shading noise. |

## Confirmed Regressions

- `02.1.HelloCone`, `02.2.EngineSpin`, `02.3.Trackball`, and
  `02.4.Examiner`: v2 used hand-authored camera parameters, and the animated
  viewer examples also added specular material state absent from `main`. Fixed
  by deriving camera parameters from the original `viewAll()` setup, preserving
  the Trackball/Examiner app-owned camera operations on top of that state, and
  restoring plain diffuse red cone materials.
- `.iv` file usage in migrated examples: v2 still has `obol::SceneIO` and the
  Chapter 11 read examples use it. The import path now extracts a conservative
  simple subset into native v2 objects when the entire parsed graph is
  supported: separators/groups, single-value materials, simple transforms,
  primitive shapes, basic lights, and `Coordinate3`/`IndexedFaceSet` polygon
  meshes with common material bindings, explicit normals, and texture
  coordinates. Unsupported content, including primitive material bindings and
  complex mesh/state graphs, still falls back to a legacy root so rendering
  remains correct. Hand-translated geometry in examples such as
  `09.4.PickAction` is therefore still a migration workaround for richer native
  IDs/picking/editing, not a decision to drop `.iv` loading.
- `09.4.PickAction`: v2 hard-coded cube primitives instead of the `star.iv`
  geometry. Source has been patched to use a v2 triangle-strip mesh derived
  from `examples/Mentor/data/star.iv`; controls have been regenerated and the
  pick tests hit the star objects by v2 ID.
- `09.2.Texture`: v2 generated a cropped red-cone texture, so the final cube
  showed a close-up red surface instead of the original centered cone texture.
  Fixed by restoring the rotated texture-source scene and widening the
  offscreen camera to match the original `viewAll()` behavior.
- `09.1.Print`: v2 camera was too close, clipping the cube and sphere relative
  to the original `viewAll()`-framed offscreen render. Fixed by moving the v2
  camera back to restore the original wide composition.
- `13.1.GlobalFlds`: v2 rendered raw Unix second values instead of the
  formatted date/time strings produced by the legacy `SoSFTime -> SoText3`
  field conversion. Fixed by using the deterministic formatted strings
  matching `SbTime::formatDate()` output and scaling text to fit the old
  clock composition.
- `13.3.TimeCounter`: v2 animation is accepted as a clearer demonstration of
  time-varying position than the old sequence, but the initial cube was
  excessively cropped. Fixed by adjusting the v2 camera target/distance so
  frame 0 is visible while keeping the full jump/travel progression legible.
- `13.4.Gate`: both main and the first v2 translation were visually weak as
  demonstrations of a gate. Fixed by adding explicit open/closed indicators
  plus a motion track, while keeping the actual behavior as app-owned state
  that either freezes or applies the time-driven transform.
- `13.8.Blinker`: v2 content was close but zoomed too far in, cropping the
  cylinder and making the sign/object proportions look unlike `main`. Fixed by
  moving the camera back and slightly adjusting the target while preserving
  the fast/slow blink timing.
- `13.5.Boolean`: v2 preserved the cube/sphere alternation but used a fixed
  camera, making the switch output substantially smaller than `main`. Fixed by
  deriving the v2 camera from the original `viewAll()` framing of the visible
  cube while keeping app-owned boolean state over stable v2 object transforms.
- `13.7.Rotor`: v2 still used the fallback cylinder/cube windmill even when
  `windmillTower.iv` and `windmillVanes.iv` were available. Fixed by adding
  transformable v2 legacy-graph import (`Scene::addLegacySceneGraph()` and
  `SceneIO::addInventorFile()`), then loading the original assets and rotating
  the vanes through a v2 group transform.
- `14.2.Editors`: v2 used a simplified tall cube table and closer camera
  instead of the low imported `desk.iv` shape from `main`. Fixed by replacing
  the approximation with a lower/wider desk assembly and matching the
  `viewAll()`-style framing more closely.
- `15.1.ConeRadius`: v2 originally used different cone material/framing and a
  decorative marker instead of a dragger cue. Fixed by using the original
  `viewAll()` framing, gray cone material, `Scene::setObjectPrimitiveOptions()`
  radius edits, and a portable Translate1-style dragger proxy.
- `15.3.AttachManip`: v2 had two separate differences: simplified manipulator
  overlays and an accidental camera/framing drift. Fixed the object spacing,
  derived the camera from the same base-scene `viewAll()` setup as `main`, and
  aligned the transform-box proxy color/style more closely; native manipulator
  geometry/behavior remains a compatibility-gap item.
- `08.3.BezSurf` / `08.4.TrimSurf`: v2 tessellated fallbacks had odd dark
  surface lighting due custom light direction plus reversed/flat face normals.
  Fixed by restoring default lighting, emitting front-facing polygon winding,
  and adding smooth per-vertex normals from Bezier derivatives.
- `05.5.Binding`: v2 changed the light direction, making the per-face palette
  appear different from `main`. Source has been patched back to default
  lighting and controls have been regenerated.
- `05.6.TransformOrdering`: v2 used hand-picked cameras and a custom light
  direction, clipping the transformed cubes and lighting side faces that were
  dark in `main`. Fixed by deriving the front camera from the original
  `viewAll()` setup, applying the same camera orbit for the angle frame, and
  restoring the default directional light.
- `05.1.FaceSet`, `05.3.TriangleStripSet`, and `05.4.QuadMesh`: v2 used
  fixed cameras and extra/custom lights rather than the original
  `viewAll()`/`rotateCamera()` sequences. Fixed by deriving v2 cameras from
  the legacy scene bounds/orbit setup and restoring default lighting where the
  original used it. `05.3.TriangleStripSet` also exposed that lowering strips
  to indexed faces changed generated normals/side lighting; the OpenGL2 bridge
  now preserves native `SoTriangleStripSet` nodes when the v2 strip data can be
  represented directly.
- `07.3.TextureFunction`: v2 correctly baked the texture-coordinate plane into
  portable mesh UVs, but used fixed cameras and a custom light that obscured
  comparison with the original coordinate-generation result. Fixed by deriving
  front and angle cameras from the original `viewAll()` plus camera-orbit
  sequence and restoring default lighting; remaining image delta is minor
  sphere tessellation/rasterization noise.
- `07.1.BasicTexture`: v2 used fixed cameras and a custom light for the
  textured cube, making the checkerboard appear over-bright and zoomed in
  relative to `main`. Fixed by deriving front and angle cameras from the
  original `viewAll()` plus camera-orbit sequence and restoring default
  lighting.
- `07.2.TextureCoordinates`: v2 preserved the explicit square UVs but used
  fixed cameras and a custom light, making the brick square appear framed and
  shaded differently from `main`. Fixed by deriving front and angle cameras
  from the original `viewAll()` plus camera-orbit sequence and restoring
  default lighting.
- `06.3.Complex3DText`: v2 used a custom light and a hand-picked angle camera
  for the beveled text fallback, exaggerating bright yellow bevels and shifting
  the close-up composition. Fixed by restoring default lighting and using the
  original explicit camera plus `rotateCamera()` orbit for the angle frame.
- `06.1.Text` / `06.2.Simple3DText`: v2 used fixed camera positions and a
  custom light direction instead of the original `viewAll()` and camera-orbit
  sequence. Fixed by deriving v2 cameras from the legacy scene bounds/orbit
  setup and restoring default lighting. Remaining differences are minor
  text-rasterization and tessellation/shading details.
- `03.1.Molecule`, `03.2.Robot`, `04.2.Lights`, `13.7.Rotor`: a v2 bridge
  ordering/scoping bug made grouped geometry render before lights, causing dark
  silhouettes or missing-looking geometry. Fixed by hoisting transformed lights
  into root render state before grouped geometry.
- `10.6.PickFilterTopLevel` / `10.7.PickFilterManip`: v2 camera framing
  clipped the selection demonstration subjects, hiding bench/cone context.
  Fixed by restoring centered, `viewAll()`-style framing while keeping v2
  object-ID selection/material updates.
- `10.2.setEventCB`: v2 used the cleared/rotated frame as the unsuffixed
  compatibility control. Fixed by rendering the primary frame at the initial
  light-blue sphere state while keeping explicit suffixed controls for points,
  rotation, and clearing.
- `12.1.FieldSensor` / `12.2.NodeSensor`: v2 separated object/camera state so
  far that the controls no longer matched the original sensor examples.
  Fixed by deriving the rendered cameras from legacy `viewAll()` behavior,
  restoring the overlapping default cube/sphere setup, and adding
  `Scene::removeObject()` for real backend-neutral object deletion.
- `16.2.Callback` / `16.3.AttachEditor`: v2 initially made the dog dish read
  as simplified cylinder/ring geometry and aliased the unsuffixed `_control`
  frame to the default white material instead of the blue edited state visible
  in `main`. Fixed first by keeping red food as non-editable geometry and
  rendering the unsuffixed compatibility frame after the blue edit, then fully
  aligned by importing the original `dogDish.iv` content through transformable
  v2 legacy-graph objects and applying material-editor changes through the
  inherited bowl material.
- `17.2.GLCallback`: v2 had replaced raw OpenGL callback drawing with
  portable polyline geometry. Fixed the API regression by adding
  `Scene::addOpenGLCallback()` for backend-native OpenGL drawing, with
  renderer diagnostics that reject the object on non-OpenGL/no-context
  backends. The example once again draws the floor through raw GL calls, and
  its controls were aligned by preserving legacy camera orientations and the
  original cumulative transform result for the sphere.

## Accepted Or Likely Intentional Differences

- Chapter 8 NURBS examples are no longer legacy NURBS nodes. They intentionally
  render v2 sampled curves and tessellated mesh fallbacks.
- `03.3.Naming` intentionally separates the named cube and sphere. The original
  overlap mostly hid the sphere before cube removal; the v2 image better
  communicates app-owned names mapped to stable object IDs.
- Chapter 14 and 15 manipulator/editor chapters intentionally no longer use
  legacy manipulator nodes directly. This is not permission to drop
  manipulator/dragger capabilities: v2 needs first-class backend-neutral
  tools/controllers for standard handles, constraints, picking, callbacks, and
  attachment behavior. Current static/procedural replacements still need
  per-example signoff because some outputs are too simplified.
- `14.2.Editors_light_off` intentionally differs from `main`: the legacy image
  remains brightly lit, while v2 applies the editor state by setting light
  intensity to zero. That is a clearer demonstration of the state transition.

## Review Rules

- If an example was intended to teach a visible shape, color, texture, pick
  target, selection state, sensor state, or animation state, v2 should preserve
  that behavior unless a better abstraction makes the legacy behavior impossible.
- Camera-only changes are acceptable only when they improve inspection without
  hiding geometry or changing the state being demonstrated.
- Generated v2-only control frames are acceptable additions, but they do not
  justify changing common `main` frames.
- Each regression fix should be followed by regenerating controls and rerunning:

```sh
ctest --test-dir build -R '^[0-9][0-9]\.' --output-on-failure
```
