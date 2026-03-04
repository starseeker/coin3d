/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file test_scenes.h
 * @brief Scene factory declarations for the Obol test library.
 *
 * Each function builds a self-contained scene graph and returns a ref'd
 * SoSeparator.  The caller is responsible for calling unref().
 * Width/height hints allow the factory to set sensible defaults for cameras,
 * viewports, and HUD elements.
 */

#ifndef OBOL_TEST_SCENES_H
#define OBOL_TEST_SCENES_H

#include <Inventor/nodes/SoSeparator.h>

// Forward declaration so callers don't need to include all dragger headers
// just to use buildDraggerTestScene().
class SoDragger;

namespace ObolTest {
namespace Scenes {

/** Simple 2×2 grid of primitives: sphere, cube, cone, cylinder. */
SoSeparator* createPrimitives(int width = 800, int height = 600);

/** Four spheres showing different material properties. */
SoSeparator* createMaterials(int width = 800, int height = 600);

/** Scene lit by multiple directional / point lights. */
SoSeparator* createLighting(int width = 800, int height = 600);

/** Hierarchical rotation and translation transforms. */
SoSeparator* createTransforms(int width = 800, int height = 600);

/** Scene with an explicit perspective camera (viewAll applied). */
SoSeparator* createCameras(int width = 800, int height = 600);

/** Checkerboard-textured cube. */
SoSeparator* createTexture(int width = 800, int height = 600);

/** SoText2 and SoText3 labels in the same scene. */
SoSeparator* createText(int width = 800, int height = 600);

/** Multiple SoText2 (2-D billboard) labels at different positions and sizes. */
SoSeparator* createText2(int width = 800, int height = 600);

/** Multiple SoText2 (2-D billboard) labels rendered with the Iosevka Aile font. */
SoSeparator* createIosevkaText2(int width = 800, int height = 600);

/** SoText3 extruded 3-D text rendered with the Iosevka Aile font. */
SoSeparator* createIosevkaText3(int width = 800, int height = 600);

/** Background gradient using a callback node. */
SoSeparator* createGradient(int width = 800, int height = 600);

/** Simple red cube with lighting (smoke test). */
SoSeparator* createColoredCube(int width = 800, int height = 600);

/** Coordinate-axis visualisation (XYZ colour-coded lines). */
SoSeparator* createCoordinates(int width = 800, int height = 600);

/** Scene using SoShadowGroup for shadow casting. */
SoSeparator* createShadow(int width = 800, int height = 600);

/** Scene containing interactive draggers. */
SoSeparator* createDraggers(int width = 800, int height = 600);

/** Head-up display overlay (SoOrthographicCamera + SoText2). */
SoSeparator* createHUD(int width = 800, int height = 600);

/** Level-of-detail (SoLOD) node switching between three spheres. */
SoSeparator* createLOD(int width = 800, int height = 600);

/** Transparent spheres demonstrating alpha blending. */
SoSeparator* createTransparency(int width = 800, int height = 600);

/** Filled, wireframe, and points draw style comparison. */
SoSeparator* createDrawStyle(int width = 800, int height = 600);

/** SoIndexedFaceSet tetrahedron. */
SoSeparator* createIndexedFaceSet(int width = 800, int height = 600);

/** Manipulators demo: SoTrackballManip and SoTabBoxManip on geometry. */
SoSeparator* createManips(int width = 800, int height = 600);

/** 2×2 grid of primitives with a perspective camera (render_scene). */
SoSeparator* createScene(int width = 800, int height = 600);

/** SoFaceSet green quad in the lower-left quadrant. */
SoSeparator* createFaceSet(int width = 800, int height = 600);

/** SoLineSet red horizontal line across the viewport. */
SoSeparator* createLineSet(int width = 800, int height = 600);

/** SoIndexedLineSet: green horizontal, red diagonal, blue V-shape. */
SoSeparator* createIndexedLineSet(int width = 800, int height = 600);

/** SoPointSet: four distinctly coloured points in the four quadrants. */
SoSeparator* createPointSet(int width = 800, int height = 600);

/** SoTriangleStripSet: emissive blue strip quad in lower half. */
SoSeparator* createTriangleStripSet(int width = 800, int height = 600);

/** SoQuadMesh: 5×5 colour-gradient grid (red → blue across columns). */
SoSeparator* createQuadMesh(int width = 800, int height = 600);

/** Vertex-coloured quad using SoPackedColor + SoIndexedFaceSet. */
SoSeparator* createVertexColors(int width = 800, int height = 600);

/** Two coloured spheres each wrapped in a SoSwitch (both switches on). */
SoSeparator* createSwitchVisibility(int width = 800, int height = 600);

/** Emissive sphere positioned off-centre (SoOrthographicCamera). */
SoSeparator* createSpherePosition(int width = 800, int height = 600);

/** Checkerboard-textured cube (SoTexture2 with procedural data). */
SoSeparator* createCheckerTexture(int width = 800, int height = 600);

/** Large sphere clipped in half by SoClipPlane at Y=0. */
SoSeparator* createClipPlane(int width = 800, int height = 600);

/** 3×3 SoArray grid of spheres + three SoMultipleCopy cubes. */
SoSeparator* createArrayMultipleCopy(int width = 800, int height = 600);

/** SoAnnotation sphere composited on top of a background sphere. */
SoSeparator* createAnnotation(int width = 800, int height = 600);

/** SoAsciiText "HELLO" centred with perspective camera. */
SoSeparator* createAsciiText(int width = 800, int height = 600);

/** SoResetTransform: blue sphere at offset + red sphere reset to origin. */
SoSeparator* createResetTransform(int width = 800, int height = 600);

/** SoShapeHints SOLID+CCW sphere (backface culling enabled). */
SoSeparator* createShapeHints(int width = 800, int height = 600);

/** SoImage: red/green checkerboard image node centred in viewport. */
SoSeparator* createImageNode(int width = 800, int height = 600);

/** SoMarkerSet: five markers arranged in a cross pattern. */
SoSeparator* createMarkerSet(int width = 800, int height = 600);

/** SoMaterialBinding PER_FACE: red left quad, blue right quad. */
SoSeparator* createMaterialBinding(int width = 800, int height = 600);

// ---- Texture / Visual / HUD factories ----

/** Textured quad with SoAlphaTest in GREATER mode (checkerboard RGBA). */
SoSeparator* createAlphaTest(int width = 800, int height = 600);

/** 2×2 primitive grid (background gradient applied by renderer, not scene). */
SoSeparator* createBackgroundGradient(int width = 800, int height = 600);

/** Sphere with SoBumpMap normal-map texture applied. */
SoSeparator* createBumpMap(int width = 800, int height = 600);

/** Sphere with two SoTextureUnit texture units (multi-texture). */
SoSeparator* createMultiTexture(int width = 800, int height = 600);

/** Cube with a procedural 3-D SoTexture3 applied. */
SoSeparator* createTexture3(int width = 800, int height = 600);

/** Two textured quads: one plain, one with SoTexture2Transform applied. */
SoSeparator* createTextureTransform(int width = 800, int height = 600);

/** Sphere with SoEnvironment (no fog, high ambient intensity). */
SoSeparator* createEnvironment(int width = 800, int height = 600);

/** Sphere with SoTextureCubeMap cube-map texture. */
SoSeparator* createCubemap(int width = 800, int height = 600);

/** Flat quad with SoSceneTexture2 render-to-texture (orange cone sub-scene). */
SoSeparator* createSceneTexture(int width = 800, int height = 600);

/** Blue sphere scene with HUD overlay (status bar + side-menu buttons). */
SoSeparator* createHUDOverlay(int width = 800, int height = 600);

/** Pure 2-D HUD scene without 3-D geometry. */
SoSeparator* createHUDNo3D(int width = 800, int height = 600);

/** Blue sphere with interactive HUD buttons (static layout, no callbacks). */
SoSeparator* createHUDInteraction(int width = 800, int height = 600);

/** SoText3 with FRONT, ALL, and BACK parts visible in one scene. */
SoSeparator* createText3Parts(int width = 800, int height = 600);

/** Near red cube + far blue sphere with SoDepthBuffer (LEQUAL depth test). */
SoSeparator* createDepthBuffer(int width = 800, int height = 600);

/** Solid and wireframe truncated-cone SoProceduralShape side by side. */
SoSeparator* createProceduralShape(int width = 800, int height = 600);

/** Textured quad with SoTextureScalePolicy::FRACTURE (SoGLBigImage path). */
SoSeparator* createGLBigImage(int width = 800, int height = 600);

/** SoImage node with an RGBA checkerboard image centred in the viewport. */
SoSeparator* createImageDeep(int width = 800, int height = 600);

/** Sphere with a basic GLSL vertex + fragment SoShaderProgram. */
SoSeparator* createShaderProgram(int width = 800, int height = 600);

/** Camera + light + SoCube — the scene rendered via SoRenderManager. */
SoSeparator* createSoRenderManager(int width = 800, int height = 600);

/** Textured sphere exercising SoGLImage / SoGLDriverDatabase code paths. */
SoSeparator* createGLFeatures(int width = 800, int height = 600);

/** 4×4 SoQuadMesh with PER_FACE material binding (nine coloured quad faces). */
SoSeparator* createQuadMeshDeep(int width = 800, int height = 600);

/** Red sphere scene for SoOffscreenRenderer API coverage. */
SoSeparator* createOffscreen(int width = 800, int height = 600);

// ---- Actions / Events / Sensors factories ----

/** Three coloured spheres spread along X, used as input for SoGetBoundingBoxAction tests. */
SoSeparator* createBBoxAction(int width = 800, int height = 600);

/** Hierarchical scene (sphere1, sphere2, cube, cone) with named nodes for SoSearchAction tests. */
SoSeparator* createSearchAction(int width = 800, int height = 600);

/** Three shapes (sphere, cube, cone) illustrating SoCallbackAction triangle traversal. */
SoSeparator* createCallbackAction(int width = 800, int height = 600);

/** All primitive shape types (sphere, cone, cylinder, cube, quad) in a row for deep callback coverage. */
SoSeparator* createCallbackActionDeep(int width = 800, int height = 600);

/** Scene containing three SoCallback nodes interleaved with geometry. */
SoSeparator* createCallbackNode(int width = 800, int height = 600);

/** Event propagation demo: SoEventCallback nodes in nested separators with sphere + cube. */
SoSeparator* createEventPropagation(int width = 800, int height = 600);

/** Two shapes (sphere left, cube right) for SoPath pick-and-copy tests. */
SoSeparator* createPathOperations(int width = 800, int height = 600);

/** Red sphere + blue cube with SoTranslation nodes — input scene for SoWriteAction tests. */
SoSeparator* createWriteReadAction(int width = 800, int height = 600);

/** Sphere whose material colour is driven by a SoComposeVec3f engine. */
SoSeparator* createFieldConnections(int width = 800, int height = 600);

/** Coloured sphere representing a static "good frame" from a sensor-driven animation. */
SoSeparator* createSensorsRendering(int width = 800, int height = 600);

/** Camera + light + SoCube — scene for SoRenderManager comprehensive API tests. */
SoSeparator* createRenderManagerFull(int width = 800, int height = 600);

/** 9-point grid with PER_VERTEX material binding — exercises SoGL PointSet variants. */
SoSeparator* createSOGLBindings(int width = 800, int height = 600);

/** Two semi-transparent overlapping objects for SoGLRenderAction transparency-mode tests. */
SoSeparator* createGLRenderActionModes(int width = 800, int height = 600);

/** Three semi-transparent spheres for deep SoGLRenderAction coverage. */
SoSeparator* createGLRenderDeep(int width = 800, int height = 600);

/** Camera + light + SoCube — input scene for SoOffscreenRenderer advanced API tests. */
SoSeparator* createOffscreenAdvanced(int width = 800, int height = 600);

/** Perspective camera + sphere — exercises camera→view-volume code paths. */
SoSeparator* createViewVolumeOps(int width = 800, int height = 600);

/** Three SoLOD nodes side-by-side with sphere/cube/cone levels for LOD + picking tests. */
SoSeparator* createLODPicking(int width = 800, int height = 600);

// ---- Interaction / Dragger / Special factories ----

/** Three-object scene (sphere, cube, cone) for camera manipulation testing. */
SoSeparator* createCameraInteraction(int width = 800, int height = 600);

/** Dynamic scene (sphere, cube, cylinder) for scene-graph mutation testing. */
SoSeparator* createSceneInteraction(int width = 800, int height = 600);

/** Sphere driven by SoComposeVec3f engine — engine-driven animation scene. */
SoSeparator* createEngineInteraction(int width = 800, int height = 600);

/** Sphere whose material color is driven via automatic field-type conversion. */
SoSeparator* createEngineConverter(int width = 800, int height = 600);

/** Green sphere behind an SoSwitch with an SoEventCallback node in the graph. */
SoSeparator* createEventCallbackInteraction(int width = 800, int height = 600);

/** Blue sphere for SoRayPickAction pick/highlight testing. */
SoSeparator* createPickInteraction(int width = 800, int height = 600);

/** SoSelection node containing sphere (left) and cube (right) for pick-filter tests. */
SoSeparator* createPickFilter(int width = 800, int height = 600);

/** SoSelection (SHIFT policy) with sphere, cube, and cone for selection API tests. */
SoSeparator* createSelectionInteraction(int width = 800, int height = 600);

/** Gray sphere scene used for sensor-driven interaction tests. */
SoSeparator* createSensorInteraction(int width = 800, int height = 600);

/** SoShapeKit containing a sphere — node-kit interaction scene. */
SoSeparator* createNodeKitInteraction(int width = 800, int height = 600);

/** Sphere with SoCenterballManip attached — complex manipulator sequence scene. */
SoSeparator* createManipSequences(int width = 800, int height = 600);

/** Three spheres + floor lit by a SoDirectionalLightManip. */
SoSeparator* createLightManips(int width = 800, int height = 600);

/** Cube + SoTranslate1Dragger — simple dragger types scene. */
SoSeparator* createSimpleDraggers(int width = 800, int height = 600);

/** ARB8-style box (solid + wireframe) for dragger topology tests. */
SoSeparator* createArb8Draggers(int width = 800, int height = 600);

/** ARB8 box with corner-handle spheres — edit-cycle visualization. */
SoSeparator* createArb8EditCycle(int width = 800, int height = 600);

/** SoExtSelection (LASSO, FULL_BBOX) with three pickable shapes. */
SoSeparator* createExtSelection(int width = 800, int height = 600);

/** SoExtSelection (RECTANGLE, PART_BBOX) with three shapes for event tests. */
SoSeparator* createExtSelectionEvents(int width = 800, int height = 600);

/** Four-quadrant scene: SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder. */
SoSeparator* createRaypickShapes(int width = 800, int height = 600);

/** Advanced shadow scene: SoShadowGroup + SoShadowSpotLight + sphere + ground. */
SoSeparator* createShadowAdvanced(int width = 800, int height = 600);

/** RT proxy shapes: SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder quad. */
SoSeparator* createRTProxyShapes(int width = 800, int height = 600);

/** Four primitives + SoRaytracingParams — NanoRT raytracing scene. */
SoSeparator* createNanoRT(int width = 800, int height = 600);

/** Ground plane + red sphere + SoRaytracingParams(shadows) — NanoRT shadow scene. */
SoSeparator* createNanoRTShadow(int width = 800, int height = 600);

/** Blue sphere scene for SoViewport API tests. */
SoSeparator* createViewport(int width = 800, int height = 600);

/** Green sphere scene rendered via SoViewport (control image source). */
SoSeparator* createViewportScene(int width = 800, int height = 600);

/** LOD scene (sphere/cube/cone) for SoQuadViewport multi-view tests. */
SoSeparator* createQuadViewport(int width = 800, int height = 600);

/** LOD scene for SoQuadViewport composite regression (control image source). */
SoSeparator* createQuadViewportLOD(int width = 800, int height = 600);

/** SoSceneTexture2 flat-quad scene for multi-context-manager regression test. */
SoSeparator* createSceneTextureMultiMgr(int width = 800, int height = 600);

// -------------------------------------------------------------------------
// Utility helpers for interaction tests
//
// These functions build reusable scene bases that are shared between the
// viewer factory scenes and the interactive test executables.  They return
// an UNREF'd SoSeparator (ref count = 0); callers must call root->ref().
// -------------------------------------------------------------------------

/** camera + directional light + green reference cube + given dragger.
 *  Used by render_draggers.cpp, render_simple_draggers.cpp, and the
 *  createSimpleDraggers() factory so both viewer and tests start from the
 *  same scene setup. */
SoSeparator* buildDraggerTestScene(SoDragger* dragger,
                                    int width = 800, int height = 600);

/** camera at (0,0,8) + directional light + purple sphere with a plain
 *  SoTransform (no manipulator pre-attached).  Tests use this base to
 *  exercise the replaceNode/replaceManip lifecycle.
 *  Matches the geometry in createManipSequences() so the viewer and the
 *  manip lifecycle tests share the same scene layout. */
SoSeparator* buildManipTestBase(int width = 800, int height = 600);

} // namespace Scenes
} // namespace ObolTest

#endif // OBOL_TEST_SCENES_H
