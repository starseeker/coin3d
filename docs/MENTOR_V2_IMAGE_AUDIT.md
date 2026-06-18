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
| 02 | 41 | 41 | 17.14 | 51.43 | `02.2.EngineSpin_frame06_control.png` | Needs review. Likely camera/timing drift after app-owned animation conversion. |
| 03 | 11 | 11 | 60.86 | 105.93 | `03.3.Naming_before_control.png` | Mixed. Molecule/Robot color regression was fixed. `03.3.Naming` separated cube/sphere composition is accepted as clearer v2 intent. |
| 04 | 10 | 10 | 33.56 | 37.69 | `04.2.Lights_frame01_control.png` | Mostly acceptable after light-state bridge fix; gradient behavior restored. |
| 05 | 23 | 23 | 34.78 | 99.24 | `05.6.TransformOrdering_front_control.png` | Mixed. `05.5.Binding` lighting was unintentional and fixed; `05.6` needs review. |
| 06 | 10 | 10 | 65.21 | 126.33 | `06.3.Complex3DText_angle_control.png` | Probably acceptable v2 text/profile fallback plus camera shift, but needs explicit signoff. |
| 07 | 9 | 9 | 46.88 | 63.26 | `07.3.TextureFunction_angle_control.png` | Needs review for texture-coordinate/function fidelity. |
| 08 | 16 | 16 | 20.95 | 29.60 | `08.2.UniCurve_top_control.png` | Intentional fallback, with fixes. NURBS examples are sampled/tessellated; `08.3`/`08.4` lighting/normals corrected. |
| 09 | 13 | 13 | 34.10 | 56.04 | `09.4.PickAction_pick_star2_control.png` | Improved. `09.1.Print` camera framing fixed; `09.2.Texture` source texture framing fixed; `09.4.PickAction` cube/star regression fixed. |
| 10 | 38 | 38 | 46.35 | 75.75 | `10.5.SelectionCB_control.png` | Needs review. Selection and pick-filter state must preserve visible selection behavior. |
| 11 | 2 | 2 | 41.82 | 45.06 | `11.2.ReadString_control.png` | Needs review. Imported/string scenes should remain visually close unless camera changed intentionally. |
| 12 | 24 | 24 | 76.00 | 163.70 | `12.2.NodeSensor_removed_sphere_control.png` | Needs signoff. Source review shows main controls are mostly `viewAll()` artifacts on simple sensor demos; v2 makes object/camera state more explicit while preserving callback semantics. |
| 13 | 109 | 83 | 31.85 | 61.33 | `13.7.Rotor_control.png` | Improved. `13.1.GlobalFlds` time formatting fixed; `13.3.TimeCounter` intentionally shows clearer time progression with adjusted initial framing; `13.4.Gate` has explicit open/closed cues; `13.8.Blinker` camera framing fixed; Rotor geometry visibility was fixed. Remaining animation/camera drift needs per-example review. |
| 14 | 8 | 8 | 21.63 | 29.34 | `14.2.Editors_light_finish_control.png` | Improved. `14.2.Editors` desk geometry/camera were brought closer to the imported `desk.iv` view; v2 `light_off` remains intentionally darker because the light-off editor state is actually applied. |
| 15 | 12 | 12 | 47.12 | 62.37 | `15.1.ConeRadius_frame04_radius2.5_control.png` | Mixed. `15.3.AttachManip` camera/object framing fixed; manipulator examples still use simplified v2 overlays pending first-class manipulator/dragger support. |
| 16 | 11 | 11 | 63.42 | 74.15 | `16.2.Callback_default_control.png` | Mixed. Callback/editor bowl now preserves red food and bowl-only material edits; unsuffixed controls match main's blue state. Remaining delta is procedural bowl/camera fidelity versus imported `dogDish.iv`. |
| 17 | 5 | 5 | 61.15 | 65.19 | `17.2.GLCallback_00_default_control.png` | Improved API fidelity. `17.2.GLCallback` now uses `Scene::addOpenGLCallback()` and raw OpenGL drawing again; remaining delta is visual/camera parity versus `main`. |

## Confirmed Regressions

- `.iv` file usage in migrated examples: v2 still has `obol::SceneIO` and the
  Chapter 11 read examples use it, but the current import path preserves parsed
  files as a legacy root rather than decomposing supported nodes into native v2
  objects with stable IDs. Hand-translated geometry in examples such as
  `09.4.PickAction` is therefore a migration workaround, not a decision to drop
  `.iv` loading. Circle back item: implement native v2 extraction for the
  supported `.iv` subset so imported meshes/materials can participate in
  picking, callbacks, scene queries, and editing.
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
- `14.2.Editors`: v2 used a simplified tall cube table and closer camera
  instead of the low imported `desk.iv` shape from `main`. Fixed by replacing
  the approximation with a lower/wider desk assembly and matching the
  `viewAll()`-style framing more closely.
- `15.3.AttachManip`: v2 had two separate differences: simplified manipulator
  overlays and an accidental camera/framing drift. Fixed the camera/object
  spacing so the cube, sphere, and cone match `main` framing more closely;
  native manipulator geometry/behavior remains a compatibility-gap item.
- `08.3.BezSurf` / `08.4.TrimSurf`: v2 tessellated fallbacks had odd dark
  surface lighting due custom light direction plus reversed/flat face normals.
  Fixed by restoring default lighting, emitting front-facing polygon winding,
  and adding smooth per-vertex normals from Bezier derivatives.
- `05.5.Binding`: v2 changed the light direction, making the per-face palette
  appear different from `main`. Source has been patched back to default
  lighting and controls have been regenerated.
- `03.1.Molecule`, `03.2.Robot`, `04.2.Lights`, `13.7.Rotor`: a v2 bridge
  ordering/scoping bug made grouped geometry render before lights, causing dark
  silhouettes or missing-looking geometry. Fixed by hoisting transformed lights
  into root render state before grouped geometry.
- `16.2.Callback` / `16.3.AttachEditor`: v2 initially made the dog dish read
  as simplified cylinder/ring geometry and aliased the unsuffixed `_control`
  frame to the default white material instead of the blue edited state visible
  in `main`. Fixed by using a smoother bowl mesh, keeping red food as
  non-editable geometry, keeping material callbacks attached only to the bowl,
  and rendering the unsuffixed compatibility frame after the blue edit.
- `17.2.GLCallback`: v2 had replaced raw OpenGL callback drawing with
  portable polyline geometry. Fixed the API regression by adding
  `Scene::addOpenGLCallback()` for backend-native OpenGL drawing, with
  renderer diagnostics that reject the object on non-OpenGL/no-context
  backends. The example once again draws the floor through raw GL calls.

## Accepted Or Likely Intentional Differences

- Chapter 8 NURBS examples are no longer legacy NURBS nodes. They intentionally
  render v2 sampled curves and tessellated mesh fallbacks.
- `12.1.FieldSensor` and `12.2.NodeSensor` differ substantially from `main`,
  but source review indicates the old images are mostly camera `viewAll()`
  artifacts rather than meaningful visual content. The v2 versions separate
  object state and camera state more clearly while retaining the sensor
  callbacks. Keep this as signoff-needed, not a confirmed lost-behavior
  regression.
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
