# Obol v2 API Modernization

## Intent

Obol v2 is a source-breaking modernization of the public API.  The v2 API is
backend-neutral: applications describe scene content, render requests, and
expected results without depending on OpenGL state objects or Open Inventor
node construction.

The legacy Inventor-derived implementation remains useful during migration. In
the initial v2 rollout, `obol::Scene` can produce a legacy scene graph bridge so
the existing OpenGL2, system OpenGL, OSMesa, and non-GL render paths continue to
exercise the same tested behavior.

## Initial Public Surface

The initial API tier lives under `include/Obol/`:

- `obol::Scene` owns modern scene data.
- `obol::Material`, `Texture2D`, `Image2D`, `Transform`, `Mesh`, `Primitive`,
  `DirectionalLight`, `PointLight`, `SpotLight`, `Text2D`, `Text3D`,
  `PerspectiveCamera`, and `OrthographicCamera` describe portable scene
  intent. `Material::unlit` covers base-color rendering without exposing the
  legacy `SoLightModel` node.
- `obol::Mesh` supports triangle lists, indexed polygon faces, triangle
  strips, and quad grids, with optional per-face normals and face/vertex color
  palettes for portable
  FaceSet/IndexedFaceSet/TriangleStripSet/QuadMesh-style data.
- `obol::SceneGroupId` and `Scene::addGroup()` provide hierarchical
  transforms without exposing legacy separator/group nodes.
- `Scene::setGroupTransform()` and `Scene::setObjectTransform()` update stable
  scene IDs for animation-style workflows without exposing mutable Inventor
  nodes. `Scene::getGroupTransform()` and `Scene::getObjectTransform()` let
  interaction controllers resync overlays without keeping a parallel transform
  cache.
- `Scene::setObjectVisible()` and `Scene::setGroupVisible()` provide
  non-destructive visibility toggles. Hidden objects and hidden group
  descendants remain queryable and mutable, but are omitted from render packets
  and the OpenGL2 legacy bridge. Backend-native preflight checks operate on
  the same visible packet contents, so hidden OpenGL callbacks or compatibility
  objects do not poison otherwise portable frames.
- `Scene::removeGroup()` removes empty groups without cascading into child
  groups or scene objects, preserving stable IDs while giving temporary
  overlays an explicit cleanup path.
- `Scene::capturePacket()` produces an immutable backend-neutral
  `ScenePacket` snapshot with camera state, group hierarchy, active object
  records, local transforms, resolved column-major `localToWorld` matrices,
  value payloads, CAD handles, and explicit backend-native/legacy markers.
  This is the first scene-extraction layer for non-OpenGL backends; the
  OpenGL2 bridge still renders through the legacy graph adapter during
  migration.
- `obol::collectPacketTriangles()` lowers v2 packet primitives and meshes into
  backend-neutral world-space triangles with material, color, normal, and
  texture-coordinate payloads. It is intentionally separate from
  `ScenePacket`, so specialized CAD, text, legacy graph, and backend-native
  paths can degrade or provide their own extraction without forcing OpenGL2
  compatibility behavior into the neutral packet contract.
- `obol::collectPacketLineSegments()` and `obol::collectPacketPoints()` lower
  v2 polylines and point clouds into backend-neutral world-space draw records,
  preserving line width, point size, material, and color intent for renderers
  that do not consume the legacy scene graph.
- `obol::collectPacketLights()` lowers v2 directional, point, and spot lights
  into backend-neutral world-space light records, preserving color, intensity,
  spot cutoff/dropoff, and group-transform effects.
- `obol::collectPacketText()` lowers v2 2D and 3D text into backend-neutral
  text records with material, world transform/origin, font metadata,
  justification, depth-test intent, 3D parts, part colors, and profile data.
- `obol::collectPacketCadAssemblies()` lowers v2 CAD assemblies into
  backend-neutral packet records containing draw/pick options, part geometry,
  instance records, composed world transforms, styles, and selected/hidden
  state without exposing `SoCADAssembly`.
- `obol::extractPacketScene()` aggregates packet triangles, line segments,
  points, lights, text, and CAD records into one backend-facing
  `ExtractedPacketScene`, preserving support counts and diagnostics. It marks
  extraction incomplete when a packet still depends on a legacy fallback root,
  raw OpenGL callback, or other backend-specific compatibility path, while
  still returning any portable records that can be rendered or inspected.
- `obol::inspectPacketGeometrySupport()` lets packet renderers classify the
  same packet before rendering, distinguishing portable geometry from light,
  text, CAD, backend-native, and legacy content so graceful degradation
  diagnostics stay consistent across backends.
- `obol::OffscreenRenderer` renders a scene through an application-provided
  `obol::RenderBackend`. Backends without a legacy context can implement
  `RenderBackend::renderPacket()` to render directly from `ScenePacket` and
  return an owned pixel buffer exposed through `OffscreenRenderer::pixels()`
  and `writeRGB()`. The default unsupported-backend path now reports
  `extractPacketScene()` diagnostics, so malformed portable records and
  backend-native/legacy dependencies degrade through the same aggregate packet
  contract future renderers are expected to consume. The no-context renderer
  path preflights `extractPacketScene()` before invoking packet rendering; if a
  packet cannot be fully lowered, rendering fails with the packet diagnostics
  instead of allowing a packet backend to silently ignore unsupported content.
- `obol::RenderBackend::capabilities()` lets packet-only, CPU, and future
  backends report backend capabilities without exposing a native context.
  `legacyContextHandle()` is now an optional compatibility hook used by the
  current OpenGL2/Open Inventor bridge, not a required backend-neutral concept.
- `obol::CameraFraming` provides v2 camera fitting/orbit helpers such as
  `viewAllPerspective()` and `orbit()` so examples and applications do not
  need to expose legacy camera nodes for common framing workflows.
- `obol::ContextManagerBackend` adapts the existing application-provided
  context-manager pointer as an opaque `NativeContextHandle`, keeping
  `SoDB::ContextManager` out of v2 public headers while the OpenGL2 bridge
  still consumes it internally.
- Legacy scene graph import/export hooks use opaque
  `NativeSceneGraphHandle`/`NativeNodeHandle` values in the v2 API, so
  compatibility bridges can still exchange native Open Inventor nodes without
  requiring v2 application headers to name `SoSeparator` or `SoCADAssembly`.
- `obol::RenderTarget` and `obol::PixelFormat` describe the requested output
  surface explicitly.
- `obol::FrameRequest` and `obol::Renderer` provide the request-oriented
  rendering facade planned for v2. The initial implementation delegates to
  `OffscreenRenderer`, preserving the existing OpenGL2/swrast bridge and
  packet-only backend behavior while making scene, target, options, and
  background explicit inputs. `Renderer` exposes the current rendered target,
  dimensions, pixel format, capabilities/profile support, pixel pointer, and
  SGI RGB writeback so callers do not need to depend on `OffscreenRenderer`
  for ordinary frame readback.
- `obol::SceneIO` preserves Open Inventor `.iv` string/file compatibility
  during migration and extracts simple fully-supported primitive scenes plus
  v2-generated `ObolSceneGroup_*` separator hierarchies with local group and
  object transforms,
  `PerspectiveCamera` and `OrthographicCamera` nodes with portable position,
  orientation-derived target/up, projection height/FOV, near, and far state,
  `Coordinate3`/`IndexedFaceSet` polygon meshes and
  `IndexedTriangleStripSet` and non-indexed `TriangleStripSet` strip meshes,
  non-indexed `QuadMesh` grids, `LineSet` polylines, and `PointSet` point
  clouds with common material and draw-style state, explicit normals, and
  texture coordinates into native v2 objects. Inline `SoTexture2` image data
  and `SoLightModel` base-color/Phong state are preserved in native v2 material
  state where supported. `SoFont`, `SoText2`, and `SoText3` nodes with direct
  string, spacing, justification, depth-test, parts, and per-part material
  state are extracted into native v2 text objects, and direct
  `SoProfileCoordinate2` plus `SoLinearProfile` profiles are preserved for
  `Text3D`, with legacy-root fallback for unsupported content.
  Native v2 export through `SceneIO::writeInventorString()` is regression
  covered for perspective and orthographic cameras, primitive variants with
  material fields, generated group hierarchies with local transforms, polygon
  meshes with face colors/normals/texture coordinates, strip meshes, quad-grid
  meshes, polyline geometry, point-cloud geometry, directional/point/spot
  lights, inline texture images, unlit material state, 2D/3D text, and 3D text
  profile data by reading the emitted `.iv` back through
  `SceneIO::readInventorString()`.
  Unsupported Inventor content loaded through `SceneIO::readInventor*()` or
  `SceneIO::addInventor*()` is retained as transformable v2 legacy-graph
  objects with stable IDs. Captured packets expose those objects as
  `SceneObjectType::LegacySceneGraph` records rather than anonymous fallback
  roots, so packet-only/non-GL backends can report a clear incomplete
  compatibility-path diagnostic while preserving object identity.
- `obol::Picker` provides portable CPU picking results over v2 scenes, using
  either viewport coordinates or an explicit world-space ray.
- `obol::PickHit::objectId` reports the v2 `SceneObjectId` for geometry
  created through the v2 scene bridge, including native objects extracted from
  supported `.iv` imports and transformable legacy objects imported through
  `SceneIO`.
- `obol::PickHit::cad` carries optional CAD instance/part detail when a CAD
  backend provides it.
- `obol::ObservableValue` provides backend-neutral application state
  notifications for field/sensor-style workflows without exposing `SoField` or
  backend sensor nodes in the v2 core API.
- `obol::Time` and `obol::TimeSpan` provide backend-neutral time values and
  UTC formatting for field/engine-style application state without exposing
  `SoSFTime` as a v2 core dependency.
- `obol::TransformDragger` is the first backend-neutral interaction API slice:
  it computes axis, plane, and free translation edits plus axis-rotation edits
  plus trackball-vector rotation edits and component scale edits over v2 object
  IDs, with optional
  scalar/component snapping and bounds where applicable.
  It can emit portable translate-axis, trackball, and box overlay geometry
  through `obol::Scene`. `obol::ManipulatorOverlay` adds target-object
  attachment metadata for handle-box, trackball, and transform-box overlays,
  `TransformDragger::syncOverlayToTarget()` updates attached overlay groups
  from the current target transform,
  `TransformDragger::setOverlayVisible()` toggles temporary handle visibility,
  and `TransformDragger::removeOverlay()` removes overlay-owned scene objects
  and their now-empty overlay group without deleting the controlled object.
  `obol::ManipulatorAttachment` plus
  `TransformDragger::attachManipulator()`, `syncManipulator()`,
  `setManipulatorVisible()`, and `detachManipulator()` provide persistent
  attachment state over those overlay primitives so applications can attach,
  resync, hide/show, and detach common handles without manually carrying loose
  scene IDs. `obol::TransformEditState` plus `beginTransformEdit()`, the
  `updateEdit*()` methods, `commitTransformEdit()`, and
  `cancelTransformEdit()` provide baseline persistent edit-session state for
  applying translation, rotation, and scale updates from current scene state
  while keeping attached handles synchronized. `obol::InteractionHandle`
  metadata, `InteractionHandleKind`, and
  `resolveOverlayHandle()`/`resolveManipulatorHandle()` provide picker-facing
  handle routing from picked v2 overlay object IDs to backend-neutral
  translate-axis, rotate-axis, uniform-scale, or bounds-box roles.
  `obol::InteractionHandleEditRequest`, `InteractionHandleEditResult`, and
  `updateEditFromHandle()` provide picked-handle-to-edit dispatch for
  translate-axis, rotate-axis, and uniform-scale handles using the same
  edit-session state. `obol::ManipulatorEditSession`,
  `ManipulatorEditRequest`, `beginManipulatorEdit()`,
  `updateManipulatorEdit()`, `commitManipulatorEdit()`, and
  `cancelManipulatorEdit()` package the resolved handle and transform edit
  state for common attached-manipulator workflows. `obol::PointerDragGesture`,
  `PointerDragGestureResult`, and `mapPointerDragToManipulatorEdit()` provide a
  first backend-neutral gesture policy layer by projecting application pointer
  deltas onto a supplied screen-space handle direction and producing the
  existing translate, rotate, or scale edit requests.
  OpenGL2/swrast and non-GL backends get the same baseline dragger semantics.
- `obol::CadAssembly` provides a modern CAD-facing value API over neutral
  `CadPartGeometry`, `CadWireRep`, `CadTriMesh`, `CadMatrix4`, and
  `CadInstanceRecord` payloads that can be added to `obol::Scene` while still
  materializing the existing `SoCADAssembly` backend for OpenGL2/swrast
  compatibility.
- `obol::RenderCapabilities`, `RenderFeatureProfile`, `RenderOptions`,
  `FrameResult`, and `RenderDiagnostic` report backend capability and graceful
  degradation. Render diagnostics propagated from packet extraction and
  backend-native preflight checks preserve the source `SceneObjectId` where
  available, so applications can connect warnings about malformed geometry,
  backend-native content, or legacy compatibility paths back to the v2 object
  that caused them.
  `supportsRenderFeatureProfile()` gives applications a stable way to branch
  between CorePortable, RasterExtended, and BackendNative paths without testing
  raw GL versions or legacy context types.
- CAD-specific APIs added for Obol live under the same tree. The modern CAD
  surface is `Obol/cad/CadAssembly.h`, `Obol/cad/CadIds.h`, and
  `Obol/cad/CadTypes.h`; Inventor-node CAD compatibility declarations live
  under `Obol/compat/cad/SoCADAssembly.h` and
  `Obol/compat/cad/SoCADDetail.h`, with old `Obol/cad/SoCAD*` forwarding
  headers retained for existing callers. The v2 umbrella includes only the
  neutral CAD headers; compatibility backends and Open Inventor-style callers
  include the `Obol/compat` headers directly.
- Modern v2 headers use `OBOL_V2_API` and opaque native handle aliases from
  `Obol/base/Export.h`, so `Obol/Obol.h` can be included without pulling in
  Inventor headers. Legacy compatibility headers keep using `OBOL_DLL_API`.
  `test_v2_public_headers` enforces that non-compat `include/Obol` headers do
  not directly include Inventor headers and that the v2 umbrella does not pull
  in compatibility or old `SoCAD*` forwarding headers.

This is intentionally a small foundation.  Future phases should route more
backends through `ScenePacket`, expand richer v2-native scene import/export,
and add first-class render targets.

## Backend Contract

OpenGL2/OSMesa swrast is a required compatibility backend, but it is not the
public API design center.  v2 APIs must avoid exposing GL concepts such as
display lists, texture units, matrix modes, fixed-function lights, and raw GL
state stacks. Public v2 headers also avoid exposing `SoDB`/`SbBasic`; backend
bridges cast opaque native handles to legacy types inside implementation files.

When a requested feature is unavailable on a backend, rendering should either
use the closest portable fallback or fail the specific request with a diagnostic.
The renderer should not crash or silently corrupt output.

Current degradation rules:

- Advanced material models fall back to the legacy Phong/material-color path.
- Native shader requests produce diagnostics unless a backend explicitly
  supports the BackendNative profile.
- Advanced transparency uses the backend default until a portable policy is
  implemented; requests on backends without RasterExtended support record a
  diagnostic.
- Shadow requests require RasterExtended framebuffer support; otherwise the
  frame renders without shadows and records a diagnostic.
- Non-GL renderers may bypass OpenGL entirely through
  `SoDB::ContextManager::renderScene()`.

## Migration Rule

New examples and v2 documentation should construct scenes through `obol::Scene`
and render by submitting `obol::FrameRequest` values to `obol::Renderer`.
`obol::OffscreenRenderer` remains the compatibility-backed implementation layer
for offscreen pixel ownership and SGI RGB output during migration. Existing
tests should continue to pass throughout migration; changing APIs must not
remove user-facing rendering, picking, text, or .iv behavior without
replacement coverage.

`test_v2_example_render_api` enforces this rule for migrated examples by
failing on direct `obol::OffscreenRenderer` use outside the documented
allowlist: `05.1.FaceSet`, `05.2.IndexedFaceSet`, `05.4.QuadMesh`, and
`06.2.Simple3DText`. A direct migration attempt for those four examples still
exceeded current image thresholds on the angle/top/side control frames, so they
remain explicit holdbacks until that control-image drift is resolved.

Current migrated Mentor examples:

- `02.1.HelloCone` uses v2 scene construction, request-oriented v2 rendering,
  and `CameraFraming::viewAllPerspective()` while preserving the original
  `viewAll()` camera framing and diffuse red material.
- `02.2.EngineSpin` uses app-owned time/angle state to update v2 object
  transforms while preserving the original `viewAll()` camera framing via
  `CameraFraming` and diffuse material.
- `02.3.Trackball` and `02.4.Examiner` use app-owned camera operations over v2
  camera state, initialized through `CameraFraming` for visual parity.
- `03.1.Molecule` uses v2 scene groups, primitive spheres, and
  request-oriented rendering for hierarchy.
- `03.2.Robot` uses v2 scene groups and request-oriented rendering for
  hierarchy.
- `03.3.Naming` uses app-owned names mapped to v2 object IDs instead of backend
  node names.
- `04.1.Cameras` uses v2 perspective/orthographic cameras and
  request-oriented rendering.
- `04.2.Lights` uses v2 light objects, request-oriented rendering, and
  `Scene::setGroupTransform()` for multi-frame light animation.
- `05.1.FaceSet` uses v2 polygon meshes with per-face normals and preserves
  the original `viewAll()`/orbit camera sequence through `CameraFraming`; it
  remains on the compatibility `OffscreenRenderer` call surface until current
  control-image drift is resolved.
- `05.2.IndexedFaceSet` uses v2 indexed polygon meshes with per-face colors
  and camera fitting/orbit through `CameraFraming`; it remains on the
  compatibility `OffscreenRenderer` call surface until current control-image
  drift is resolved.
- `05.3.TriangleStripSet` uses v2 triangle-strip mesh topology and
  request-oriented rendering; the OpenGL2 bridge preserves native strip nodes
  when representable and keeps indexed-face fallback available for richer
  indexed cases, with camera fitting/orbit through `CameraFraming`.
- `05.4.QuadMesh` uses v2 quad-grid mesh topology; the OpenGL2 bridge
  preserves native quad mesh nodes when representable and keeps indexed-face
  fallback available for richer indexed cases, while preserving the original
  `viewAll()`/orbit camera sequence through `CameraFraming`. It remains on the
  compatibility `OffscreenRenderer` call surface until current control-image
  drift is resolved.
- `05.5.Binding` uses v2 face colors, vertex colors, indexed face color
  palettes, and request-oriented rendering to cover the old material-binding
  variants, with camera fitting through `CameraFraming`.
- `05.6.TransformOrdering` uses nested v2 scene groups and request-oriented
  rendering to express transform ordering without exposing legacy transform
  nodes, while deriving its cameras from the original `viewAll()`/orbit setup
  through `CameraFraming` for visual parity.
- `06.1.Text` uses v2 `Text2D` labels with font metadata, world transforms,
  and request-oriented rendering while preserving the original
  `viewAll()`/orbit camera sequence through `CameraFraming` and default
  lighting for parity.
- `06.2.Simple3DText` uses v2 `Text3D` labels with font metadata, transform
  scaling, part selection, and part colors while preserving the original
  `viewAll()`/orbit camera sequence through `CameraFraming` and default
  lighting for parity; it remains on the compatibility `OffscreenRenderer`
  call surface until current angled-frame control drift is resolved.
- `06.3.Complex3DText` uses v2 `Text3D` profile points and request-oriented
  rendering for beveled extrusion while degrading through legacy profile nodes
  on OpenGL2/swrast, preserving the original explicit camera/orbit sequence
  through `CameraFraming` for parity.
- `07.1.BasicTexture` uses v2 `Texture2D` image data attached through
  `Material::baseColorTexture`, request-oriented rendering, and preserves the
  original `viewAll()`/orbit camera sequence through `CameraFraming` for
  parity.
- `07.2.TextureCoordinates` uses v2 mesh UV attributes, optional texture
  coordinate indices, and request-oriented rendering, while preserving the
  original `viewAll()`/orbit camera sequence through `CameraFraming` for
  parity.
- `07.3.TextureFunction` bakes procedural plane texture-coordinate generation
  into portable v2 mesh UVs, uses request-oriented rendering, and preserves the
  original `viewAll()`/orbit camera sequence through `CameraFraming` for
  parity.
- `08.1.BSCurve` samples the B-spline into a v2 `Polyline` and uses
  request-oriented rendering; the example target and sampled-control image
  comparisons are enabled.
- `08.2.UniCurve` samples the uniform B-spline into a v2 `Polyline`; the
  example uses request-oriented rendering, and sampled-control image
  comparisons are enabled.
- `08.3.BezSurf` tessellates the Bezier patch into a v2 quad-grid mesh and
  uses request-oriented rendering; the example target and tessellated-control
  image comparisons are enabled.
- `08.4.TrimSurf` tessellates the trimmed Bezier patch into a v2 polygon mesh
  with a coarse parameter-space trim hole and uses request-oriented rendering;
  the example target and tessellated-control image comparisons are enabled.
- `09.1.Print` uses the v2 scene and request-oriented rendering as the
  portable replacement for the original print-to-PostScript flow.
- `09.2.Texture` renders a v2 source scene to offscreen RGB pixels, copies them
  into `Texture2D` image data, and applies the generated texture through
  material state using the request-oriented renderer facade.
- `09.3.Search` uses v2 `SceneQuery` and stable object IDs as the portable
  replacement for Inventor search actions and backend node paths, rendered
  through explicit frame requests.
- `09.4.PickAction` uses v2 world-ray picking and material updates by stable
  object ID as the portable replacement for Inventor pick paths and per-node
  highlight insertion, rendered through explicit frame requests.
- `09.5.GenSph` uses v2 CPU-side sphere tessellation into portable `Mesh` data
  as the replacement for Inventor primitive-generation callbacks, rendered
  through explicit frame requests.
- `10.1.addEventCB` uses application-owned keyboard event dispatch to update
  selected v2 object transforms by stable object ID, with all frames rendered
  through explicit frame requests.
- `10.2.setEventCB` uses application-owned pointer event translation, v2 camera
  updates, v2 `PointCloud` updates, and an unlit v2 material as the portable
  replacement for toolkit/render-area event callbacks, point-set nodes, and
  base-color light model state. The compatibility primary frame is rendered at
  the initial event state through the request-oriented renderer, with explicit
  suffixed frames for points, rotation, and clearing.
- `10.5.SelectionCB` uses application-owned selection callbacks over stable v2
  object IDs and material updates, avoiding backend selection nodes, with
  fitted camera setup through `CameraFraming` and request-oriented rendering.
- `10.6.PickFilterTopLevel` uses application-owned pick filtering: top-level
  selection highlights all component object IDs in a model group, while default
  selection highlights only the leaf component object ID, with fitted camera
  setup through `CameraFraming` and request-oriented rendering.
- `10.7.PickFilterManip` uses the same application-owned filtering pattern for
  manipulator-like overlays by highlighting/restoring the selected controlled
  object through v2 material updates, with fitted camera setup through
  `CameraFraming` and request-oriented rendering.
- `10.8.PickFilterNodeKit` uses application-owned pick filtering and
  material-editor callbacks over multiple selected v2 object IDs, rendered
  through explicit frame requests.
- `11.1.ReadFile` and `11.2.ReadString` use v2 `SceneIO` for legacy Inventor
  file/string import. Simple supported primitive imports can become native v2
  objects; unsupported parsed graphs remain wrapped as legacy content behind v2
  camera/light state so they render correctly on OpenGL2/swrast. Unsupported
  imports also appear in `ScenePacket` as legacy graph objects with explicit
  packet extraction diagnostics for non-compatibility backends. The examples
  derive their v2 camera through `CameraFraming` to preserve imported scene
  framing and render through explicit frame requests.
- `12.1.FieldSensor`, `12.2.NodeSensor`, `12.3.AlarmSensor`, and
  `12.4.TimerSensor` use application-owned field/node/alarm/timer callbacks
  that update v2 camera, transform, and material/geometry state without
  exposing backend sensor nodes. `12.1` and `12.2` now use
  `obol::ObservableValue` for watched camera/object state and
  `obol::CameraFraming` for view-all camera setup. `Scene::removeObject()`
  provides the backend-neutral deletion operation needed by node-change examples
  without forcing applications to move removed objects offscreen. All chapter
  12 frames render through explicit frame requests.
- `13.1.GlobalFlds`, `13.2.ElapsedTime`, `13.3.TimeCounter`, and `13.4.Gate`
  use application-owned field/engine evaluation that updates v2 text and
  object transforms without exposing backend field or engine nodes. `13.1`,
  `13.2`, `13.3`, and `13.4` now use `obol::Time`/`TimeSpan` for
  deterministic clock formatting and elapsed-time progression, and `13.4` uses
  `obol::ObservableValue` for gate-enable state plus bounded
  `TransformDragger` free translation for the moving object, while using
  `Scene::setObjectVisible()` for the open/closed gate state. All chapter 13
  frames render through explicit frame requests.
- `13.5.Boolean`, `13.6.Calculator`, `13.7.Rotor`, and `13.8.Blinker` use
  application-owned boolean/calculator/rotor/blinker evaluation over stable v2
  object and group transforms. `13.5.Boolean` preserves the original
  switch-framed `viewAll()` camera through `CameraFraming` while toggling v2
  object visibility through `Scene::setObjectVisible()`, `13.8.Blinker` uses
  the same visibility API for blink state, and `13.7.Rotor` imports the
  windmill `.iv` assets as
  transformable legacy-graph v2 objects, with camera fitting/orbit through
  `CameraFraming`, so the rotor group drives the original vanes geometry. All
  chapter 13 frames render through explicit frame requests.
- `14.1.FrolickingWords`, `14.2.Editors`, and `14.3.Balance` use
  application-owned kit/editor/event state over v2 text, primitive, group,
  material, and light state. `14.2.Editors` controls were refreshed for the v2
  procedural/editor rendering, and all chapter 14 frames render through
  explicit frame requests.
- `15.1.ConeRadius`, `15.2.SliderBox`, `15.3.AttachManip`, and
  `15.4.Customize` use application-owned manipulator/slider state over v2
  object IDs. `TransformDragger` now provides the first reusable v2
  translation/rotation/scale edit-session state, picked-handle role resolution
  and dispatch, packaged manipulator edit sessions, pointer-delta gesture
  mapping, and portable overlay geometry.
  `15.1`, `15.2`, and `15.4` now use the shared axis-translation path, `15.1`
  and `15.3` use `CameraFraming`, and `15.3` uses the shared persistent
  manipulator attachment path for portable target-attached overlays and
  overlay-to-target sync. Richer handle-box, transform-box, and trackball
  controller semantics remain follow-up work. All chapter 15 frames render
  through explicit frame requests.
- `16.2.Callback` and `16.3.AttachEditor` use toolkit-owned material editor
  callbacks/attachments over v2 object IDs and `Scene::setObjectMaterial()`.
  They import the original `dogDish.iv` content as a transformable legacy-graph
  v2 object, preserving red food while applying edited material to the bowl via
  the inherited material override; the unsuffixed compatibility frame maps to
  the blue edited state. Both examples render through explicit frame requests.
- `17.2.GLCallback` uses `Scene::addOpenGLCallback()` to exercise an explicit
  backend-native OpenGL callback facility for applications that require
  pre-existing third-party GL drawing code. OpenGL backends execute it with a
  current context; non-OpenGL or no-context backends fail with clear
  diagnostics. The example preserves the original camera-orientation sequence
  using v2 value types plus local vector math, leaving raw GL drawing as its
  only intentional backend-native dependency, and encodes legacy cumulative
  transform results explicitly in v2 object transforms. It renders through an
  explicit frame request while preserving the original gray clear color.

In dual-GL builds, Mentor examples can force the OSMesa/swrast headless helper
with `OBOL_HEADLESS_FORCE_SWRAST`, so migrated v2 examples can render without a
display server while the system-GL path remains available for Xvfb/GLX runs.
