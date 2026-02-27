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

} // namespace Scenes
} // namespace ObolTest

#endif // OBOL_TEST_SCENES_H
