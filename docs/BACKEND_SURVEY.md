# Obol Rendering Backend Feature Survey

This document surveys Obol's feature set across four rendering backend tiers:
1. **Any backend** (including raytracing or Vulkan rasterization)
2. **OSMesa OpenGL 2.0 + extensions** (headless software rasterizer)
3. **Full system OpenGL** (hardware-accelerated, all extensions)
4. **Vulkan rasterization** (`SoVulkanContextManager`)

It also describes what changes were made to allow non-GL backends to work
with Obol's scene graph, and which features can be recast generically.

For details on how system GL and OSMesa coexist in a single library build, see
`docs/DUAL_GL_ARCHITECTURE.md`.

---

## Tier 1 — Works with Any Rendering Backend (Including Raytracing)

These features work entirely in CPU/C++ code and have no dependency on OpenGL.
A raytracing backend such as [nanort](https://github.com/lighttransport/nanort)
can use them directly.

### Scene Graph Traversal (OpenGL-free)

| Feature | Classes | Notes |
|---------|---------|-------|
| Scene graph nodes | `SoGroup`, `SoSeparator`, `SoSwitch`, `SoLOD`, `SoLevelOfDetail`, `SoMultipleCopy` | Pure C++ container nodes |
| Transforms | `SoTransform`, `SoTranslation`, `SoRotation`, `SoRotationXYZ`, `SoScale`, `SoMatrixTransform`, `SoResetTransform`, `SoAntiSquish`, `SoSurroundScale` | All math is backend-independent |
| Materials (data) | `SoMaterial`, `SoBaseColor`, `SoPackedColor`, `SoMaterialBinding` | Store color/PBR data; no GL calls |
| Normals | `SoNormal`, `SoNormalBinding` | Geometry metadata |
| Coordinates | `SoCoordinate3`, `SoCoordinate4` | Vertex position data |
| Texture coordinates | `SoTextureCoordinate2`, `SoTextureCoordinate3`, `SoTextureCoordinatePlane`, `SoTextureCoordinateCylinder`, `SoTextureCoordinateSphere` | UV data; mapping functions |
| Lights (data) | `SoDirectionalLight`, `SoPointLight`, `SoSpotLight`, `SoLight` | Light parameters are backend-independent; OpenGL state update happens in `SoGLRenderAction` only |
| Camera math | `SoPerspectiveCamera`, `SoOrthographicCamera`, `SoFrustumCamera`, `SoReversePerspectiveCamera` | View/projection matrix computation |
| Vertex property | `SoVertexProperty` | Combined vertex data node |
| Draw style (data) | `SoShapeHints`, `SoComplexity` | Shape rendering hints |

### Actions (OpenGL-free)

| Action | Purpose | Backend |
|--------|---------|---------|
| `SoGetBoundingBoxAction` | Compute scene bounding boxes | Any |
| `SoGetMatrixAction` | Compute transformation matrices | Any |
| `SoSearchAction` | Find nodes in the scene graph | Any |
| `SoCallbackAction` | Traverse graph with user callbacks; generate geometry | **Any — primary hook for raytracing** |
| `SoRayPickAction` | CPU-based ray intersection picking | Any (uses `generatePrimitives()`) |
| `SoGetPrimitiveCountAction` | Count primitives in scene | Any |
| `SoHandleEventAction` | Distribute events through graph | Any |
| `SoWriteAction` | Write scene graph to file | Any |
| `SoSceneRenderAction` | **New** — traverse scene for raytracing backends | **Any** (see below) |

### Shape Geometry Generation (OpenGL-free)

Every `SoShape` subclass implements `generatePrimitives(SoAction*)`, which
decomposes the shape into `SoPrimitiveVertex` triangles/lines/points and
invokes callbacks via `invokeTriangleCallbacks()` etc.  This code path:

- Has **no OpenGL dependency**
- Is used by `SoCallbackAction`, `SoRayPickAction`, and the new
  `SoSceneRenderAction`
- Is also the fallback rendering path in `SoShape::GLRender()` for shapes
  that don't override it

Internal geometry generators in `src/misc/SoGenerate.cpp` provide the actual
tessellation for the primitive shapes:

| Function | Shape |
|----------|-------|
| `sogen_generate_cone()` | `SoCone` |
| `sogen_generate_cylinder()` | `SoCylinder` |
| `sogen_generate_sphere()` | `SoSphere` |
| `sogen_generate_cube()` | `SoCube` |

For mesh shapes (`SoFaceSet`, `SoIndexedFaceSet`, `SoTriangleStripSet`, etc.),
`generatePrimitives()` directly walks the coordinate and index arrays.

### Math Utilities (OpenGL-free)

All `Sb*` math types are fully backend-independent:

`SbVec2f`, `SbVec3f`, `SbVec4f`, `SbMatrix`, `SbRotation`, `SbViewVolume`,
`SbBox3f`, `SbPlane`, `SbLine`, `SbSphere`, `SbCylinder`, `SbColor`, etc.

---

## Tier 2 — Works with OSMesa OpenGL 2.0 + Extensions

OSMesa provides OpenGL 2.0 (not 2.1) with key extensions:
`GL_EXT_framebuffer_object`, `GL_ARB_vertex_buffer_object`,
`GL_ARB_shader_objects` / `GL_ARB_fragment_shader` (GLSL 1.10),
`GL_ARB_multitexture`, `GL_ARB_texture_non_power_of_two`.

### Rendering Actions

| Feature | Classes | Notes |
|---------|---------|-------|
| OpenGL render action | `SoGLRenderAction` | Core GL rendering loop |
| Render manager | `SoRenderManager`, `SoSceneManager` | Manages GL state and scene rendering |
| Offscreen rendering | `SoOffscreenRenderer`, `CoinOffscreenGLCanvas` | Uses `GL_EXT_framebuffer_object`; works with OSMesa |

### Shape Rendering

All standard shape nodes render via either direct GL calls (optimized path)
or `generatePrimitives()` fallback.  Both paths work with OpenGL 2.0:

`SoCone`, `SoCube`, `SoCylinder`, `SoSphere`, `SoFaceSet`, `SoIndexedFaceSet`,
`SoTriangleStripSet`, `SoIndexedTriangleStripSet`, `SoQuadMesh`,
`SoLineSet`, `SoIndexedLineSet`, `SoPointSet`, `SoIndexedPointSet`,
`SoMarkerSet`, `SoIndexedMarkerSet`, `SoAsciiText`, `SoText2`, `SoText3`,
`SoImage`.

### GL State Elements

All `SoGL*Element` classes (e.g. `SoGLLazyElement`, `SoGLModelMatrixElement`,
`SoGLClipPlaneElement`, etc.) are instantiated by `SoGLRenderAction` and work
with any OpenGL 2.0 context.  They are **not usable** with a non-GL backend.

### Textures and Multi-Texturing

| Feature | Classes | Requires |
|---------|---------|---------|
| 2D textures | `SoTexture2`, `SoTexture2Transform` | GL 1.1 / OSMesa 2.0 ✓ |
| 3D textures | `SoTexture3`, `SoTexture3Transform` | `GL_EXT_texture3D` / OSMesa 2.0 ✓ |
| Cube map textures | `SoTextureCubeMap` | `GL_ARB_texture_cube_map` / OSMesa 2.0 ✓ |
| Multi-texturing | `SoTextureUnit`, `SoTextureCombine` | `GL_ARB_multitexture` / OSMesa 2.0 ✓ |
| Scene-rendered texture | `SoSceneTexture2`, `SoSceneTextureCubeMap` | `GL_EXT_framebuffer_object` / OSMesa 2.0 ✓ |

### GLSL Shader System

| Feature | Classes | Requires |
|---------|---------|---------|
| Shader programs | `SoShaderProgram`, `SoVertexShader`, `SoFragmentShader`, `SoGeometryShader` | `GL_ARB_shader_objects` / OSMesa 2.0 ✓ |
| GLSL shaders | `SoGLSLShaderProgram`, `SoGLSLShaderObject` | `GL_ARB_vertex_shader` / OSMesa 2.0 ✓ |
| ARB programs (legacy) | `SoGLARBShaderProgram`, `SoGLARBShaderObject` | `GL_ARB_fragment_program` |
| Shader parameters | `SoShaderParameter*` | Same as shader type |

**Note on `SoGeometryShader`**: Geometry shaders require
`GL_EXT_geometry_shader4` which is an extension beyond OpenGL 2.0.  OSMesa may
or may not support it.  Check at runtime with `SoGLDriverDatabase::isSupported`.

### Vertex Buffer Objects (VBO)

`SoVBO` and `SoVertexArrayIndexer` manage GPU-side vertex data using
`GL_ARB_vertex_buffer_object`.  OSMesa supports VBOs; they are used
automatically for `SoIndexedFaceSet` and related mesh nodes when available.

### Shadow Rendering

`SoShadowGroup` and related nodes (`SoShadowDirectionalLight`,
`SoShadowSpotLight`) use Variance Shadow Maps implemented with GLSL shaders
and FBOs.  This requires:
- `GL_ARB_shader_objects` (GLSL)
- `GL_EXT_framebuffer_object`
- `GL_ARB_texture_float` (for the depth map texture)

OSMesa 2.0 supports all of these, so shadow rendering should work in software
rendering mode, though performance will be low.

### Bump Mapping

`SoBumpMap`, `SoBumpMapCoordinate`, `SoBumpMapTransform` implement
per-fragment normal mapping using GLSL shaders.  Requires:
- `GL_ARB_shader_objects`
- `GL_ARB_multitexture`

OSMesa supports both; bump mapping should work.

---

## Tier 3 — Full System OpenGL Only

These features require capabilities beyond OpenGL 2.0 or depend on
hardware-specific extensions that OSMesa may not provide:

| Feature | Classes | Requires |
|---------|---------|---------|
| Occlusion queries | Used internally for LOD | `GL_ARB_occlusion_query` — may not be present in OSMesa |
| Anisotropic filtering | `SoGLDriverDatabase::SO_GL_ANISOTROPIC_FILTERING` | `GL_EXT_texture_filter_anisotropic` — hardware only |
| Sorted-layers blending | `SoGLRenderAction::SORTED_LAYERS_BLEND` transparency | Complex multi-pass FBO + NV or ARB extensions |
| NV vertex array range | `SO_GL_NV_VERTEX_ARRAY_RANGE` | NVIDIA-specific `GL_NV_vertex_array_range` |
| Stereo rendering (quad buffer) | `SoRenderManager::QUAD_BUFFER` | Hardware quad-buffer stereo, not in OSMesa |
| Hardware-accelerated performance | All GL rendering | Depends on GPU and system drivers |
| Texture compression | `SO_GL_TEXTURE_COMPRESSION` | `GL_ARB_texture_compression` — driver-dependent |
| Paletted textures | `SO_GL_PALETTED_TEXTURES` | `GL_EXT_paletted_texture` — largely deprecated |

---

## Features that Cannot Work with a Raytracing Backend

The following features are inherently tied to the OpenGL rasterization pipeline
and have **no meaningful analogue** in a raytracing backend:

| Feature | Reason |
|---------|--------|
| `SoGLRenderAction` | Drives the GL rendering loop; issues GL draw calls |
| All `SoGL*Element` classes | Track GL state for the rasterizer |
| `SoVBO`, `SoVertexArrayIndexer` | GPU-resident vertex data management |
| `SoPolygonOffset` | Rasterizer-specific depth bias |
| `SoDepthBuffer` | Z-buffer control |
| `SoColorIndex` | Color-index mode (removed from modern OpenGL) |
| `SoGLDisplayList` | OpenGL display lists |
| `SoGLRenderPassElement` | Multi-pass rasterizer state |
| `SoAlphaTest` | Fragment-level alpha testing in rasterizer |
| `CoinOffscreenGLCanvas` | GL framebuffer management |
| GLSL shaders | Not directly executable by raytracers; would need reimplementation as material/BxDF functions |

---

## Features that Can Be Recast for Raytracing

The following features have an OpenGL-specific implementation today, but their
**scene graph data** is backend-independent and can serve a raytracing backend:

| Feature | OpenGL Class | Raytrace Equivalent |
|---------|-------------|---------------------|
| Shape geometry | `sogl_render_*()` via `SoGLRenderAction` | Use `generatePrimitives()` via `SoSceneRenderAction` |
| Materials | `SoGLLazyElement` pushes to GL | Read from `SoLazyElement` / `SoCallbackAction::getMaterial()` |
| Lights | `SoGLLightIdElement`, GL light state | `SoLightElement::getLights()` gives `SoLight*` nodes with properties |
| Camera | `SoGLProjectionMatrixElement`, `SoGLViewingMatrixElement` | `SoCallbackAction::getViewingMatrix()`, `getProjectionMatrix()`, `getViewVolume()` |
| Transforms | `SoGLModelMatrixElement` pushes to GL | `SoCallbackAction::getModelMatrix()` |
| Textures (image data) | `SoGLTextureImageElement`, `SoGLMultiTextureImageElement` | `SoCallbackAction::getTextureImage()` returns raw pixel data |
| Normal vectors | `SoGLNormalElement` | `SoPrimitiveVertex::getNormal()` in geometry callbacks |
| Texture coordinates | `SoGLTextureCoordinateElement` | `SoPrimitiveVertex::getTextureCoords()` in geometry callbacks |
| Draw style | `SoGLDrawStyleElement` | `SoCallbackAction::getDrawStyle()` |
| Line width, point size | `SoGLLineWidthElement`, `SoGLPointSizeElement` | `SoCallbackAction::getLineWidth()`, `getPointSize()` |

The key insight is that Obol already maintains two parallel representations:
1. **OpenGL state** (via `SoGL*Element` classes) — used by `SoGLRenderAction`
2. **Generic state** (via non-GL elements and `SoCallbackAction` accessors) — usable by any backend

---

## How to Implement a Raytracing Backend

### Approach 1: Use `SoCallbackAction` Directly

`SoCallbackAction` already provides everything a raytracer needs.  A nanort
backend can be built like this:

```cpp
// Collect scene geometry into raytracer
SoCallbackAction ca;
ca.addTriangleCallback(SoShape::getClassTypeId(), collectTriangle, myScene);
ca.apply(root);
// Then configure camera and lights from state accessors, and raytrace
```

Inside `collectTriangle`, use:
- `action->getModelMatrix()` — world transform
- `action->getMaterial()` — diffuse/specular/emission/shininess/transparency
- `action->getTextureImage()` — raw texture pixels
- `SoLightElement::getLights(action->getState())` — all active lights

### Approach 2: Use the New `SoSceneRenderAction`

`SoSceneRenderAction` is a new action class that inherits from
`SoCallbackAction` and provides:
- A named type (`SoSceneRenderAction::getClassTypeId()`) so shape nodes
  can check `action->isOfType(SoSceneRenderAction::getClassTypeId())`
  if they want to detect the raytrace path
- A `setViewportRegion()` / `getViewportRegion()` interface matching
  `SoGLRenderAction` for symmetry
- A convenience `getLights()` method that returns the active scene lights
  without requiring direct access to the `SoState`
- Documentation that makes the raytracing use case explicit

```cpp
SoSceneRenderAction rta(SbViewportRegion(800, 600));
rta.addTriangleCallback(SoShape::getClassTypeId(), collectTriangle, myScene);
rta.apply(root);
// rta.getLights() gives the lights collected during traversal
```

### Approach 3: Use `SoSceneCollector` (Recommended)

`SoSceneCollector` is the highest-level integration point and handles
everything required by a practical raytracing backend:

- World-space triangle collection with per-vertex normals and materials
- World-space light extraction (directional, point, spot)
- Proxy geometry for `SoLineSet`, `SoIndexedLineSet`, `SoPointSet`
- Wireframe cylinder ring proxies for rotational dragger widgets
- `DrawStyle INVISIBLE` pruning (picks/draggers that shouldn't cast shadows)
- `SoText2`, `SoHUDLabel`, `SoHUDButton` pixel overlays
- Scene change detection with `needsRebuild()` to avoid redundant BVH rebuilds
- `compositeOverlays()` to alpha-blend text/HUD onto the framebuffer

```cpp
SoSceneCollector collector;
SbViewportRegion vp(800, 600);
SoCamera * cam = findCamera(root);

if (collector.needsRebuild(root, cam)) {
    collector.reset();
    collector.collect(root, vp);
    buildMyBVH(collector.getTriangles());   // backend-specific
    collector.updateCacheKeysAfterRebuild(root, cam);
} else {
    collector.collectOverlaysOnly(root, vp);
}

// ... raytrace ...
collector.compositeOverlays(pixels, width, height, 3);
collector.updateCameraId(cam);
```

See `tests/utils/nanort_context_manager.h` (nanort) and
`tests/utils/embree_context_manager.h` (Intel Embree 4) for complete
worked examples of this pattern.

### What a Complete Raytracing Integration Looks Like

A full integration (outside Obol itself) would:

1. **Scene collection pass**: Call `SoSceneCollector::collect()` to
   extract all triangles with materials, world-space normals, lights, and
   text/HUD overlays.
2. **BVH build**: Feed `collector.getTriangles()` into your backend's
   acceleration structure builder.
3. **Render loop**: For each pixel, fire a primary ray using the camera
   parameters from `SoViewVolume::projectPointToLine()`.  Shade hits
   using `collector.getLights()`.
4. **Overlay composite**: Call `collector.compositeOverlays()` to blend
   text and HUD elements onto the rendered framebuffer.

No OpenGL is required at any step.  The existing `SoRayPickAction` uses the
same `generatePrimitives()` path and demonstrates that CPU-side ray
intersection works today without any OpenGL context.

---

## Tier 4 — Vulkan Rasterization Backend

`SoVulkanContextManager` (`tests/utils/vulkan_context_manager.h`) implements the
`renderScene()` virtual to rasterize the Obol scene graph using the Vulkan API.
It uses `SoRaytracerSceneCollector` (a subclass of `SoSceneCollector`) for
geometry and light extraction, then issues Vulkan draw calls for rasterization.

### Key characteristics

| Property | Detail |
|---------|--------|
| API | Vulkan 1.0+ |
| CI driver | Mesa lavapipe (`mesa-vulkan-drivers`) |
| Pixel row order | Bottom-to-top (matching `SoOffscreenRenderer::getBuffer()`) |
| Row-order explanation | Vulkan y-down NDC + OpenGL projection matrix = double-flip = bottom-up rows; no explicit flip needed |
| Geometry pipeline | `SoRaytracerSceneCollector::collect()` → Vulkan vertex/index buffers |
| Shading | CPU Phong pre-baking via scene collector; no GLSL required |
| Text/HUD | `SoSceneCollector::compositeOverlays()` alpha-blended onto the framebuffer |

### Limitations vs. full system GL

- No GLSL shader support (materials are Phong-baked at scene collection time)
- No shadow maps, bump mapping, or FBO-based effects
- No texture mapping (planned)
- Requires Vulkan SDK (`vulkan-sdk`, `libvulkan-dev`) or Mesa lavapipe for CI

---

## Summary Table

| Feature Category | Nanort/Embree/Raytrace | Vulkan Rasterization | OSMesa GL 2.0 | Full System GL |
|-----------------|------------------------|----------------------|---------------|----------------|
| Scene graph traversal | ✓ | ✓ | ✓ | ✓ |
| Bounding box computation | ✓ | ✓ | ✓ | ✓ |
| Ray picking | ✓ | ✓ | ✓ | ✓ |
| Transform/matrix math | ✓ | ✓ | ✓ | ✓ |
| Material data | ✓ (via SoSceneCollector) | ✓ (Phong pre-baked) | ✓ | ✓ |
| Light data | ✓ (via SoSceneCollector) | ✓ (via SoSceneCollector) | ✓ | ✓ |
| Camera data | ✓ (via SoCallbackAction) | ✓ (via SoSceneCollector) | ✓ | ✓ |
| Shape geometry (triangles) | ✓ (via generatePrimitives) | ✓ (via SoSceneCollector) | ✓ | ✓ |
| Line/point geometry (proxy) | ✓ (via SoSceneCollector) | ✓ (via SoSceneCollector) | ✓ | ✓ |
| Text/HUD overlays | ✓ (via SoSceneCollector) | ✓ (compositeOverlays) | ✓ | ✓ |
| Scene change detection | ✓ (needsRebuild/updateCacheKeys) | ✓ (needsRebuild) | — | — |
| Texture image data | ✓ (via SoCallbackAction) | ~ (planned) | ✓ | ✓ |
| Basic rasterization | ✗ | ✓ (Vulkan) | ✓ | ✓ |
| VBO/vertex array rendering | ✗ | ✗ (Vulkan buffers used) | ✓ | ✓ |
| GLSL shaders | ✗ | ✗ | ✓ (GLSL 1.10) | ✓ |
| Shadow rendering (VSM) | ✗ | ✗ | ✓ | ✓ |
| Bump mapping | ✗ | ✗ | ✓ | ✓ |
| FBO offscreen rendering | ✗ | ✗ | ✓ | ✓ |
| Occlusion queries | ✗ | ✗ | ~(extension) | ✓ |
| Anisotropic filtering | ✗ | ✗ | ✗ | ✓ |
| Stereo (quad buffer) | ✗ | ✗ | ✗ | ✓ (hardware) |
| NV vertex array range | ✗ | ✗ | ✗ | ✓ (NVIDIA) |
| Hardware acceleration | ✗ | ✓ (GPU Vulkan) | ✗ | ✓ |

Legend: ✓ = supported, ✗ = not supported, ~ = partial / planned
