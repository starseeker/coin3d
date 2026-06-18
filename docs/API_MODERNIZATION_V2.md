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
  intent.
- `obol::Mesh` supports triangle lists, indexed polygon faces, triangle
  strips, and quad grids, with optional per-face normals and face/vertex color
  palettes for portable
  FaceSet/IndexedFaceSet/TriangleStripSet/QuadMesh-style data.
- `obol::SceneGroupId` and `Scene::addGroup()` provide hierarchical
  transforms without exposing legacy separator/group nodes.
- `Scene::setGroupTransform()` and `Scene::setObjectTransform()` update stable
  scene IDs for animation-style workflows without exposing mutable Inventor
  nodes.
- `obol::OffscreenRenderer` renders a scene through an application-provided
  `obol::RenderBackend`.
- `obol::ContextManagerBackend` adapts the existing application-provided
  `SoDB::ContextManager` interface into the v2 backend model.
- `obol::RenderTarget` and `obol::PixelFormat` describe the requested output
  surface explicitly.
- `obol::SceneIO` preserves Open Inventor `.iv` string/file compatibility
  during migration.
- `obol::Picker` provides portable CPU picking results over v2 scenes, using
  either viewport coordinates or an explicit world-space ray.
- `obol::PickHit::objectId` reports the v2 `SceneObjectId` for geometry
  created through the v2 scene bridge, or `InvalidSceneObjectId` for legacy
  imported content without v2 identity.
- `obol::PickHit::cad` carries optional CAD instance/part detail when a CAD
  backend provides it.
- `obol::CadAssembly` provides a modern CAD-facing value API that can be added
  to `obol::Scene` while still materializing the existing `SoCADAssembly`
  backend for OpenGL2/swrast compatibility.
- `obol::RenderCapabilities`, `RenderOptions`, `FrameResult`, and
  `RenderDiagnostic` report backend capability and graceful degradation.
- CAD-specific APIs added for Obol live under the same tree:
  `Obol/cad/CadAssembly.h`, `Obol/cad/CadIds.h`, `Obol/cad/SoCADAssembly.h`,
  `Obol/cad/SoCADDetail.h`, and `Obol/render/DepthPolicy.h`.

This is intentionally a small foundation.  Future phases should expand this
surface toward `RenderBackend`, immutable scene packets, richer v2-native
scene import/export, and first-class render targets.

## Backend Contract

OpenGL2/OSMesa swrast is a required compatibility backend, but it is not the
public API design center.  v2 APIs must avoid exposing GL concepts such as
display lists, texture units, matrix modes, fixed-function lights, and raw GL
state stacks.

When a requested feature is unavailable on a backend, rendering should either
use the closest portable fallback or fail the specific request with a diagnostic.
The renderer should not crash or silently corrupt output.

Current degradation rules:

- Advanced material models fall back to the legacy Phong/material-color path.
- Native shader requests produce diagnostics unless a backend explicitly
  supports them.
- Advanced transparency uses the backend default until a portable policy is
  implemented.
- Shadow requests require framebuffer support; otherwise the frame renders
  without shadows and records a diagnostic.
- Non-GL renderers may bypass OpenGL entirely through
  `SoDB::ContextManager::renderScene()`.

## Migration Rule

New examples and v2 documentation should construct scenes through `obol::Scene`
and render through `obol::OffscreenRenderer`.  Existing tests should continue to
pass throughout migration; changing APIs must not remove user-facing rendering,
picking, text, or .iv behavior without replacement coverage.

Current migrated Mentor examples:

- `02.1.HelloCone` uses v2 scene construction and v2 offscreen rendering.
- `02.2.EngineSpin` uses app-owned time/angle state to update v2 object
  transforms.
- `02.3.Trackball` and `02.4.Examiner` use app-owned camera operations over v2
  camera state.
- `03.1.Molecule` uses v2 scene groups and primitive spheres for hierarchy.
- `03.2.Robot` uses v2 scene groups for hierarchy and v2 offscreen rendering.
- `03.3.Naming` uses app-owned names mapped to v2 object IDs instead of backend
  node names.
- `04.1.Cameras` uses v2 perspective and orthographic cameras.
- `04.2.Lights` uses v2 light objects and `Scene::setGroupTransform()` for
  multi-frame light animation.
- `05.1.FaceSet` uses v2 polygon meshes with per-face normals.
- `05.2.IndexedFaceSet` uses v2 indexed polygon meshes with per-face colors.
- `05.3.TriangleStripSet` uses v2 triangle-strip mesh topology; the OpenGL2
  bridge lowers strips to indexed faces for swrast compatibility.
- `05.4.QuadMesh` uses v2 quad-grid mesh topology; the OpenGL2 bridge lowers
  grid cells to indexed quad faces for swrast compatibility.
- `05.5.Binding` uses v2 face colors, vertex colors, and indexed face color
  palettes to cover the old material-binding variants.
- `05.6.TransformOrdering` uses nested v2 scene groups to express transform
  ordering without exposing legacy transform nodes.
- `06.1.Text` uses v2 `Text2D` labels with font metadata and world
  transforms.
- `06.2.Simple3DText` uses v2 `Text3D` labels with font metadata, transform
  scaling, part selection, and part colors.
- `06.3.Complex3DText` uses v2 `Text3D` profile points for beveled extrusion
  while degrading through legacy profile nodes on OpenGL2/swrast.
- `07.1.BasicTexture` uses v2 `Texture2D` image data attached through
  `Material::baseColorTexture`.
- `07.2.TextureCoordinates` uses v2 mesh UV attributes and optional texture
  coordinate indices.
- `07.3.TextureFunction` bakes procedural plane texture-coordinate generation
  into portable v2 mesh UVs.
- `08.1.BSCurve` samples the B-spline into a v2 `Polyline`; the example target
  is enabled, while the old NURBS-control image comparison remains skipped
  until controls are refreshed for the sampled output.
- `08.2.UniCurve` samples the uniform B-spline into a v2 `Polyline`; the
  example target is enabled with image comparison still skipped pending
  refreshed controls.
- `08.3.BezSurf` tessellates the Bezier patch into a v2 quad-grid mesh; the
  example target is enabled with image comparison still skipped pending
  refreshed controls.
- `08.4.TrimSurf` tessellates the trimmed Bezier patch into a v2 polygon mesh
  with a coarse parameter-space trim hole; the example target is enabled with
  image comparison still skipped pending refreshed controls.
- `09.1.Print` uses the v2 scene and offscreen renderer as the portable
  replacement for the original print-to-PostScript flow.
- `09.2.Texture` renders a v2 source scene to offscreen RGB pixels, copies them
  into `Texture2D` image data, and applies the generated texture through
  material state.
- `09.3.Search` uses v2 `SceneQuery` and stable object IDs as the portable
  replacement for Inventor search actions and backend node paths.
- `09.4.PickAction` uses v2 world-ray picking and material updates by stable
  object ID as the portable replacement for Inventor pick paths and per-node
  highlight insertion.
- `09.5.GenSph` uses v2 CPU-side sphere tessellation into portable `Mesh` data
  as the replacement for Inventor primitive-generation callbacks.
- `10.1.addEventCB` uses application-owned keyboard event dispatch to update
  selected v2 object transforms by stable object ID.
- `10.2.setEventCB` uses application-owned pointer event translation, v2 camera
  updates, and v2 `PointCloud` updates as the portable replacement for
  toolkit/render-area event callbacks and point-set nodes.
- `10.5.SelectionCB` uses application-owned selection callbacks over stable v2
  object IDs and material updates, avoiding backend selection nodes.
- `10.6.PickFilterTopLevel` uses application-owned pick filtering: top-level
  selection highlights all component object IDs in a model group, while default
  selection highlights only the leaf component object ID.
- `10.7.PickFilterManip` uses the same application-owned filtering pattern for
  manipulator-like overlays by highlighting/restoring the selected controlled
  object through v2 material updates.
- `10.8.PickFilterNodeKit` uses application-owned pick filtering and
  material-editor callbacks over multiple selected v2 object IDs.
- `11.1.ReadFile` and `11.2.ReadString` use v2 `SceneIO` for legacy Inventor
  file/string import. The legacy-root bridge now wraps imported content behind
  v2 camera/light state so imported content renders correctly on OpenGL2/swrast.
- `12.1.FieldSensor`, `12.2.NodeSensor`, `12.3.AlarmSensor`, and
  `12.4.TimerSensor` use application-owned field/node/alarm/timer callbacks
  that update v2 camera, transform, and material/geometry state without
  exposing backend sensor nodes.
- `13.1.GlobalFlds`, `13.2.ElapsedTime`, `13.3.TimeCounter`, and `13.4.Gate`
  use application-owned field/engine evaluation that updates v2 text and
  object transforms without exposing backend field or engine nodes.
- `13.5.Boolean`, `13.6.Calculator`, `13.7.Rotor`, and `13.8.Blinker` use
  application-owned boolean/calculator/rotor/blinker evaluation over stable v2
  object and group transforms.
- `14.1.FrolickingWords`, `14.2.Editors`, and `14.3.Balance` use
  application-owned kit/editor/event state over v2 text, primitive, group,
  material, and light state. `14.2.Editors` controls were refreshed for the v2
  procedural/editor rendering.
- `15.1.ConeRadius`, `15.2.SliderBox`, `15.3.AttachManip`, and
  `15.4.Customize` use application-owned manipulator/slider state over v2
  object IDs. `15.1` uses `Scene::setObjectPrimitiveOptions()` for parameter
  edits, while `15.3` uses portable overlay geometry instead of backend
  manipulator nodes.
- `16.2.Callback` and `16.3.AttachEditor` use toolkit-owned material editor
  callbacks/attachments over v2 object IDs and `Scene::setObjectMaterial()`.
  Their controls preserve red food while applying edited material only to the
  bowl; the unsuffixed compatibility frame maps to the blue edited state.
- `17.2.GLCallback` uses `Scene::addOpenGLCallback()` to exercise an explicit
  backend-native OpenGL callback facility for applications that require
  pre-existing third-party GL drawing code. OpenGL backends execute it with a
  current context; non-OpenGL or no-context backends fail with clear
  diagnostics.

In dual-GL builds, Mentor examples can force the OSMesa/swrast headless helper
with `OBOL_HEADLESS_FORCE_SWRAST`, so migrated v2 examples can render without a
display server while the system-GL path remains available for Xvfb/GLX runs.
