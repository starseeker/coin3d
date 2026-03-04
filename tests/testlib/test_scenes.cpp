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
 * @file test_scenes.cpp
 * @brief Scene factory implementations for the Obol test library.
 *
 * Each factory builds a self-contained scene (camera + light(s) + geometry)
 * and returns a ref'd SoSeparator.  The caller is responsible for unref().
 */

#include "test_scenes.h"
#include "headless_utils.h"

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbColor.h>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoTransparencyType.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/draggers/SoRotateSphericalDragger.h>
#include <Inventor/draggers/SoDragger.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/manips/SoTrackballManip.h>
#include <Inventor/manips/SoTabBoxManip.h>
#include <Inventor/annex/HUD/nodekits/SoHUDKit.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>
#include <Inventor/nodes/SoRaytracingParams.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowStyle.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>

#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoTriangleStripSet.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoClipPlane.h>
#include <Inventor/SbPlane.h>
#include <Inventor/nodes/SoArray.h>
#include <Inventor/nodes/SoMultipleCopy.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/nodes/SoAnnotation.h>
#include <Inventor/nodes/SoAsciiText.h>
#include <Inventor/nodes/SoResetTransform.h>
#include <Inventor/nodes/SoImage.h>
#include <Inventor/fields/SoSFImage.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/nodes/SoMarkerSet.h>
#include <Inventor/nodes/SoVertexProperty.h>
#include <Inventor/nodes/SoTextureCoordinateDefault.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoLevelOfDetail.h>
#include <Inventor/engines/SoComposeVec3f.h>

#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoExtSelection.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/manips/SoCenterballManip.h>
#include <Inventor/manips/SoDirectionalLightManip.h>
#include <Inventor/annex/FXViz/nodes/SoShadowSpotLight.h>

#include <vector>

#include <Inventor/gl.h>

#include <cstring>

namespace ObolTest {
namespace Scenes {

// =========================================================================
// Internal helpers
// =========================================================================

// Standard camera + one directional light.  Returns the camera (inserted
// at index 0) so the caller may call viewAll().
static SoPerspectiveCamera* addCameraAndLight(SoSeparator* root)
{
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);
    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.5f, -1.0f);
    root->addChild(light);
    return cam;
}

// Build a standard checkerboard texture (red/white, 64×64).
static void buildCheckerTexture(SoTexture2* tex, int tileSize = 8)
{
    const int SIZE = tileSize * 8;
    const int NC   = 3;
    unsigned char* buf = new unsigned char[SIZE * SIZE * NC];
    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            int tx = x / tileSize;
            int ty = y / tileSize;
            int idx = (y * SIZE + x) * NC;
            if ((tx + ty) % 2 == 0) {
                buf[idx] = 220; buf[idx+1] = 40;  buf[idx+2] = 40;
            } else {
                buf[idx] = 255; buf[idx+1] = 255; buf[idx+2] = 255;
            }
        }
    }
    tex->image.setValue(SbVec2s(SIZE, SIZE), NC, buf);
    tex->model.setValue(SoTexture2::MODULATE);
    tex->wrapS.setValue(SoTexture2::REPEAT);
    tex->wrapT.setValue(SoTexture2::REPEAT);
    delete[] buf;
}


// =========================================================================
// 1. Primitives: 2×2 grid (sphere, cube, cone, cylinder)
// =========================================================================
SoSeparator* createPrimitives(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // (col, row) -> (offset_x, offset_y) spacing of 2.5
    struct Cell { float x, y; SoNode* shape; const float color[3]; };
    const Cell cells[] = {
        { -1.4f,  1.2f, new SoSphere,   {0.8f, 0.2f, 0.2f} },
        {  1.4f,  1.2f, new SoCube,     {0.2f, 0.7f, 0.2f} },
        { -1.4f, -1.2f, new SoCone,     {0.2f, 0.3f, 0.9f} },
        {  1.4f, -1.2f, new SoCylinder, {0.9f, 0.7f, 0.1f} },
    };
    for (const auto& c : cells) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(c.x, c.y, 0.0f);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(c.color[0], c.color[1], c.color[2]);
        mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
        mat->shininess.setValue(0.4f);
        sep->addChild(mat);
        sep->addChild(c.shape);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 2. Materials
// =========================================================================
SoSeparator* createMaterials(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // Four spheres with the same base diffuse color (silver-gray) but
    // different material effect parameters to illustrate shininess,
    // emissive glow, and ambient response independently.
    struct MatSpec {
        float sr,sg,sb;   // specularColor
        float er,eg,eb;   // emissiveColor
        float shine;      // shininess 0..1
        float ambient;    // ambientColor (grey)
    };
    const MatSpec specs[] = {
        { 0.0f,0.0f,0.0f,  0.0f,0.0f,0.0f,  0.0f, 0.2f }, // flat/matte
        { 1.0f,1.0f,1.0f,  0.0f,0.0f,0.0f,  0.9f, 0.2f }, // very shiny
        { 0.0f,0.0f,0.0f,  0.4f,0.1f,0.5f,  0.0f, 0.2f }, // emissive glow
        { 0.6f,0.6f,0.6f,  0.0f,0.0f,0.0f,  0.4f, 0.8f }, // high ambient
    };
    const float xs[] = {-4.5f, -1.5f, 1.5f, 4.5f};
    for (int i = 0; i < 4; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(0.6f, 0.6f, 0.6f); // same grey for all
        mat->specularColor.setValue(specs[i].sr, specs[i].sg, specs[i].sb);
        mat->emissiveColor.setValue(specs[i].er, specs[i].eg, specs[i].eb);
        mat->shininess.setValue(specs[i].shine);
        mat->ambientColor.setValue(specs[i].ambient, specs[i].ambient, specs[i].ambient);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 3. Lighting
// =========================================================================
SoSeparator* createLighting(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Three spheres in a row, each lit by a different light type so the
    // difference in illumination is clearly visible.
    const float xs[] = { -3.5f, 0.0f, 3.5f };

    // Left sphere: directional light (warm white, from upper-left)
    {
        SoSeparator* lsep = new SoSeparator;
        SoDirectionalLight* dl = new SoDirectionalLight;
        dl->direction.setValue(-1.0f, -1.0f, -0.5f);
        dl->color.setValue(1.0f, 0.9f, 0.7f);
        lsep->addChild(dl);
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(xs[0], 0.0f, 0.0f);
        lsep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.8f, 0.8f);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess.setValue(0.3f);
        lsep->addChild(mat);
        lsep->addChild(new SoSphere);
        root->addChild(lsep);
    }

    // Middle sphere: point light (cool blue, close range)
    {
        SoSeparator* msep = new SoSeparator;
        SoPointLight* pl = new SoPointLight;
        pl->location.setValue(xs[1], 2.0f, 3.0f);
        pl->color.setValue(0.4f, 0.6f, 1.0f);
        pl->intensity.setValue(1.2f);
        msep->addChild(pl);
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(xs[1], 0.0f, 0.0f);
        msep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.8f, 0.8f);
        mat->specularColor.setValue(0.9f, 0.9f, 0.9f);
        mat->shininess.setValue(0.8f);
        msep->addChild(mat);
        msep->addChild(new SoSphere);
        root->addChild(msep);
    }

    // Right sphere: spot light (tight cone, yellow-white)
    {
        SoSeparator* rsep = new SoSeparator;
        SoSpotLight* sl = new SoSpotLight;
        sl->location.setValue(xs[2], 4.0f, 4.0f);
        sl->direction.setValue(0.0f, -0.7f, -0.7f);
        sl->cutOffAngle.setValue(0.35f);
        sl->dropOffRate.setValue(0.7f);
        sl->color.setValue(1.0f, 1.0f, 0.8f);
        sl->intensity.setValue(1.0f);
        rsep->addChild(sl);
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(xs[2], 0.0f, 0.0f);
        rsep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.8f, 0.8f);
        mat->specularColor.setValue(1.0f, 1.0f, 0.9f);
        mat->shininess.setValue(0.6f);
        rsep->addChild(mat);
        rsep->addChild(new SoSphere);
        root->addChild(rsep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 4. Transforms
// =========================================================================
SoSeparator* createTransforms(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    const float colors[9][3] = {
        {0.9f,0.2f,0.2f}, {0.2f,0.8f,0.2f}, {0.2f,0.2f,0.9f},
        {0.9f,0.7f,0.1f}, {0.7f,0.2f,0.8f}, {0.1f,0.8f,0.8f},
        {0.9f,0.5f,0.2f}, {0.5f,0.9f,0.2f}, {0.2f,0.5f,0.9f},
    };

    // Row 1 (top): Translations along X
    float xpos[3] = {-4.0f, 0.0f, 4.0f};
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(xpos[i], 4.0f, 0.0f);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i][0],colors[i][1],colors[i][2]);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }
    // Row 2 (middle): Rotations around X, Y, Z axes
    SbVec3f rotAxes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTransform* xf = new SoTransform;
        xf->translation.setValue(xpos[i], 0.0f, 0.0f);
        xf->rotation.setValue(rotAxes[i], float(M_PI / 4.0));
        sep->addChild(xf);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i+3][0],colors[i+3][1],colors[i+3][2]);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }
    // Row 3 (bottom): Scaling (small, medium, large)
    float scales[3] = {0.5f, 1.0f, 1.8f};
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTransform* xf = new SoTransform;
        xf->translation.setValue(xpos[i], -4.0f, 0.0f);
        xf->scaleFactor.setValue(scales[i], scales[i], scales[i]);
        sep->addChild(xf);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i+6][0],colors[i+6][1],colors[i+6][2]);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 5. Cameras
// =========================================================================
SoSeparator* createCameras(int width, int height)
{
    // Three cubes receding along the Z axis to illustrate perspective
    // foreshortening: equal-sized cubes appear progressively smaller as
    // their depth increases.
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->heightAngle.setValue(float(M_PI / 4.0));
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -1.0f, -0.7f);
    root->addChild(light);

    const float colors[3][3] = {
        {0.9f,0.2f,0.2f}, {0.2f,0.8f,0.2f}, {0.2f,0.3f,0.9f}
    };
    // Cubes placed at increasing Z depth: nearest (z=0), middle (z=-4), far (z=-8)
    const float depths[3] = { 0.0f, -4.0f, -8.0f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(0.0f, 0.0f, depths[i]);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i][0],colors[i][1],colors[i][2]);
        mat->specularColor.setValue(0.6f,0.6f,0.6f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 6. Texture
// =========================================================================
SoSeparator* createTexture(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    // Left: sphere with checkerboard texture applied
    {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(-1.5f, 0.0f, 0.0f);
        sep->addChild(t);
        SoTexture2* tex = new SoTexture2;
        buildCheckerTexture(tex);
        sep->addChild(tex);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    // Right: sphere without texture (plain diffuse shading)
    {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(1.5f, 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial* smat = new SoMaterial;
        smat->diffuseColor.setValue(0.6f, 0.6f, 0.9f);
        smat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        smat->shininess.setValue(0.4f);
        sep->addChild(smat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 7. Text (SoText3 — extruded 3-D geometry text)
// =========================================================================
SoSeparator* createText(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // SoText3 label (3-D geometry) — front face
    SoSeparator* t3sep = new SoSeparator;
    SoTranslation* t3pos = new SoTranslation;
    t3pos->translation.setValue(-1.0f, 1.0f, 0.0f);
    t3sep->addChild(t3pos);
    SoMaterial* t3mat = new SoMaterial;
    t3mat->diffuseColor.setValue(0.8f, 0.5f, 0.1f);
    t3sep->addChild(t3mat);
    SoFont* t3font = new SoFont;
    t3font->size.setValue(0.6f);
    t3sep->addChild(t3font);
    SoText3* text3 = new SoText3;
    text3->string.setValue("Obol");
    text3->parts.setValue(SoText3::ALL);
    t3sep->addChild(text3);
    root->addChild(t3sep);

    // Second SoText3 line below
    SoSeparator* t3bsep = new SoSeparator;
    SoTranslation* t3bpos = new SoTranslation;
    t3bpos->translation.setValue(-1.0f, 0.0f, 0.0f);
    t3bsep->addChild(t3bpos);
    SoMaterial* t3bmat = new SoMaterial;
    t3bmat->diffuseColor.setValue(0.2f, 0.5f, 0.9f);
    t3bsep->addChild(t3bmat);
    SoFont* t3bfont = new SoFont;
    t3bfont->size.setValue(0.5f);
    t3bsep->addChild(t3bfont);
    SoText3* text3b = new SoText3;
    text3b->string.setValue("3D Text");
    text3b->parts.setValue(SoText3::ALL);
    t3bsep->addChild(text3b);
    root->addChild(t3bsep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos[0], pos[1], pos[2] * 1.5f);
    return root;
}

// =========================================================================
// 7b. Text2 (SoText2 — 2-D screen-space billboard text)
// =========================================================================
SoSeparator* createText2(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // A reference sphere so there is 3-D geometry for the camera to frame
    SoSeparator* sphereSep = new SoSeparator;
    SoTranslation* sphT = new SoTranslation;
    sphT->translation.setValue(0.0f, 0.0f, 0.0f);
    sphereSep->addChild(sphT);
    SoMaterial* sphMat = new SoMaterial;
    sphMat->diffuseColor.setValue(0.3f, 0.4f, 0.7f);
    sphereSep->addChild(sphMat);
    SoSphere* sph = new SoSphere;
    sph->radius.setValue(0.5f);
    sphereSep->addChild(sph);
    root->addChild(sphereSep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos[0], pos[1], pos[2] * 1.4f);

    // Multiple SoText2 labels anchored at world positions in the frustum
    struct LabelSpec { float x, y, z; float r, g, b; float sz; const char* str; };
    const LabelSpec labels[] = {
        { -0.8f,  0.9f, 0.0f,  1.0f, 1.0f, 0.2f, 18.0f, "SoText2 Demo" },
        { -0.6f,  0.3f, 0.0f,  0.3f, 1.0f, 0.4f, 14.0f, "Green label"  },
        {  0.0f, -0.3f, 0.0f,  1.0f, 0.4f, 0.2f, 14.0f, "Orange label" },
        { -0.4f, -0.8f, 0.0f,  0.7f, 0.7f, 1.0f, 12.0f, "Small blue"   },
    };
    for (const auto& l : labels) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(l.x, l.y, l.z);
        sep->addChild(t);
        SoFont* font = new SoFont;
        font->size.setValue(l.sz);
        sep->addChild(font);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(l.r, l.g, l.b);
        mat->emissiveColor.setValue(l.r * 0.8f, l.g * 0.8f, l.b * 0.8f);
        sep->addChild(mat);
        SoText2* text2 = new SoText2;
        text2->string.setValue(l.str);
        sep->addChild(text2);
        root->addChild(sep);
    }

    return root;
}

// =========================================================================
// 7c. IosevkaText2 — SoText2 labels rendered with the Iosevka Aile font
// =========================================================================
#ifndef OBOL_FONTS_DIR
#define OBOL_FONTS_DIR ""
#endif
/* OBOL_FONTS_DIR is set by CMake to the source-tree fonts/ directory.
 * If it is empty (fallback, e.g. standalone builds) loadFont() will fail
 * gracefully and the scene will fall back to the embedded ProFont. */
static const char k_iosevka_regular[] =
    OBOL_FONTS_DIR "/Iosevka/IosevkaAile-Regular.ttc";

SoSeparator* createIosevkaText2(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // Reference sphere so the camera has geometry to frame
    SoSeparator* sphereSep = new SoSeparator;
    SoTranslation* sphT = new SoTranslation;
    sphT->translation.setValue(0.0f, 0.0f, 0.0f);
    sphereSep->addChild(sphT);
    SoMaterial* sphMat = new SoMaterial;
    sphMat->diffuseColor.setValue(0.25f, 0.35f, 0.6f);
    sphereSep->addChild(sphMat);
    SoSphere* sph = new SoSphere;
    sph->radius.setValue(0.5f);
    sphereSep->addChild(sph);
    root->addChild(sphereSep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos[0], pos[1], pos[2] * 1.4f);

    // SoText2 labels with Iosevka Aile Regular font
    struct LabelSpec { float x, y, z; float r, g, b; float sz; const char* str; };
    const LabelSpec labels[] = {
        { -0.8f,  0.9f, 0.0f,  1.0f, 1.0f, 0.3f, 20.0f, "Iosevka Aile"   },
        { -0.7f,  0.3f, 0.0f,  0.3f, 1.0f, 0.5f, 16.0f, "Hello World"    },
        {  0.0f, -0.3f, 0.0f,  1.0f, 0.5f, 0.2f, 14.0f, "OpenGL Text"    },
        { -0.5f, -0.8f, 0.0f,  0.8f, 0.8f, 1.0f, 12.0f, "AaBbCc 012"     },
    };
    for (const auto& l : labels) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(l.x, l.y, l.z);
        sep->addChild(t);
        SoFont* font = new SoFont;
        font->name.setValue(k_iosevka_regular);
        font->size.setValue(l.sz);
        sep->addChild(font);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(l.r, l.g, l.b);
        mat->emissiveColor.setValue(l.r * 0.8f, l.g * 0.8f, l.b * 0.8f);
        sep->addChild(mat);
        SoText2* text2 = new SoText2;
        text2->string.setValue(l.str);
        sep->addChild(text2);
        root->addChild(sep);
    }

    return root;
}

// =========================================================================
// 7d. IosevkaText3 — SoText3 extruded 3-D text with Iosevka Aile font
// =========================================================================
SoSeparator* createIosevkaText3(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // First SoText3 line
    SoSeparator* t3sep = new SoSeparator;
    SoTranslation* t3pos = new SoTranslation;
    t3pos->translation.setValue(-1.0f, 0.5f, 0.0f);
    t3sep->addChild(t3pos);
    SoMaterial* t3mat = new SoMaterial;
    t3mat->diffuseColor.setValue(0.9f, 0.6f, 0.1f);
    t3sep->addChild(t3mat);
    SoFont* t3font = new SoFont;
    t3font->name.setValue(k_iosevka_regular);
    t3font->size.setValue(0.6f);
    t3sep->addChild(t3font);
    SoText3* text3 = new SoText3;
    text3->string.setValue("Iosevka");
    text3->parts.setValue(SoText3::ALL);
    t3sep->addChild(text3);
    root->addChild(t3sep);

    // Second SoText3 line
    SoSeparator* t3bsep = new SoSeparator;
    SoTranslation* t3bpos = new SoTranslation;
    t3bpos->translation.setValue(-1.0f, -0.4f, 0.0f);
    t3bsep->addChild(t3bpos);
    SoMaterial* t3bmat = new SoMaterial;
    t3bmat->diffuseColor.setValue(0.2f, 0.6f, 0.9f);
    t3bsep->addChild(t3bmat);
    SoFont* t3bfont = new SoFont;
    t3bfont->name.setValue(k_iosevka_regular);
    t3bfont->size.setValue(0.5f);
    t3bsep->addChild(t3bfont);
    SoText3* text3b = new SoText3;
    text3b->string.setValue("Aile 3D");
    text3b->parts.setValue(SoText3::ALL);
    t3bsep->addChild(text3b);
    root->addChild(t3bsep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos[0], pos[1], pos[2] * 1.5f);
    return root;
}

// =========================================================================
// 8. Gradient background
// =========================================================================

// Background gradient drawn via a SoCallback node using Obol's dispatched
// GL wrappers (via <Inventor/gl.h>).  The gl*() calls below are macros
// that expand to SoGLContext_glXxx(sogl_current_render_glue(), ...) so
// they route to the correct backend in both system-GL and OSMesa renders.
static void gradientCB(void* /*data*/, SoAction* action)
{
    if (!action->isOfType(SoGLRenderAction::getClassTypeId())) return;
    SoGLRenderAction* ra = static_cast<SoGLRenderAction*>(action);
    const SbViewportRegion& vp = ra->getViewportRegion();
    int w = vp.getViewportSizePixels()[0];
    int h = vp.getViewportSizePixels()[1];

    glMatrixMode(GL_PROJECTION);
    glPushMatrix(); glLoadIdentity();
    glOrtho(0, w, 0, h, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix(); glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    glBegin(GL_QUADS);
        // bottom: dark blue
        glColor3f(0.05f, 0.05f, 0.20f); glVertex2f(0.0f, 0.0f);
        glColor3f(0.05f, 0.05f, 0.20f); glVertex2f((float)w, 0.0f);
        // top: lighter blue
        glColor3f(0.20f, 0.35f, 0.60f); glVertex2f((float)w, (float)h);
        glColor3f(0.20f, 0.35f, 0.60f); glVertex2f(0.0f, (float)h);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
}

SoSeparator* createGradient(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    // Gradient background drawn first (before depth-tested geometry).
    // Uses SoCallback + dispatched gl*() calls via <Inventor/gl.h> so
    // it works correctly with both System GL and OSMesa backends.
    SoCallback* bg = new SoCallback;
    bg->setCallback(gradientCB, nullptr);
    root->addChild(bg);

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.9f, 0.9f);
    mat->specularColor.setValue(1.0f, 1.0f, 1.0f);
    mat->shininess.setValue(0.7f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 9. Colored cube
// =========================================================================
SoSeparator* createColoredCube(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.85f, 0.10f, 0.10f);
    mat->specularColor.setValue(0.50f, 0.50f, 0.50f);
    mat->shininess.setValue(0.40f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 10. Coordinates (XYZ axis lines)
// =========================================================================
SoSeparator* createCoordinates(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoLightModel* lm = new SoLightModel;
    lm->model.setValue(SoLightModel::BASE_COLOR);
    root->addChild(lm);

    SoDrawStyle* ds = new SoDrawStyle;
    ds->lineWidth.setValue(3.0f);
    root->addChild(ds);

    // X axis – red
    {
        SoSeparator* axis = new SoSeparator;
        SoBaseColor* col = new SoBaseColor;
        col->rgb.setValue(1.0f, 0.0f, 0.0f);
        axis->addChild(col);
        SoCoordinate3* coords = new SoCoordinate3;
        SbVec3f pts[2] = { {0,0,0}, {3,0,0} };
        coords->point.setValues(0, 2, pts);
        axis->addChild(coords);
        SoLineSet* ls = new SoLineSet;
        ls->numVertices.setValue(2);
        axis->addChild(ls);
        root->addChild(axis);
    }
    // Y axis – green
    {
        SoSeparator* axis = new SoSeparator;
        SoBaseColor* col = new SoBaseColor;
        col->rgb.setValue(0.0f, 0.9f, 0.0f);
        axis->addChild(col);
        SoCoordinate3* coords = new SoCoordinate3;
        SbVec3f pts[2] = { {0,0,0}, {0,3,0} };
        coords->point.setValues(0, 2, pts);
        axis->addChild(coords);
        SoLineSet* ls = new SoLineSet;
        ls->numVertices.setValue(2);
        axis->addChild(ls);
        root->addChild(axis);
    }
    // Z axis – blue
    {
        SoSeparator* axis = new SoSeparator;
        SoBaseColor* col = new SoBaseColor;
        col->rgb.setValue(0.2f, 0.4f, 1.0f);
        axis->addChild(col);
        SoCoordinate3* coords = new SoCoordinate3;
        SbVec3f pts[2] = { {0,0,0}, {0,0,3} };
        coords->point.setValues(0, 2, pts);
        axis->addChild(coords);
        SoLineSet* ls = new SoLineSet;
        ls->numVertices.setValue(2);
        axis->addChild(ls);
        root->addChild(axis);
    }

    // Small sphere at origin
    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-1,-1,-1);
    root->addChild(light);
    SoLightModel* lm2 = new SoLightModel;
    lm2->model.setValue(SoLightModel::PHONG);
    root->addChild(lm2);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f,0.9f,0.2f);
    root->addChild(mat);
    SoSphere* orig = new SoSphere;
    orig->radius.setValue(0.15f);
    root->addChild(orig);

    cam->position.setValue(4.0f, 4.0f, 6.0f);
    cam->pointAt(SbVec3f(1.0f, 1.0f, 0.5f), SbVec3f(0,1,0));

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 11. Shadow
// =========================================================================
SoSeparator* createShadow(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    // NanoRT hint: enable shadow rays.  SoRaytracingParams is a no-op for
    // GL renderers; the SoShadowGroup below handles GL shadow maps.
    SoRaytracingParams* rtParams = new SoRaytracingParams;
    rtParams->shadowsEnabled.setValue(TRUE);
    rtParams->ambientIntensity.setValue(0.2f);
    root->addChild(rtParams);

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 4.0f, 8.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.45f);
    root->addChild(cam);

    // SoShadowGroup wraps the shadow-casting geometry and activates
    // OpenGL variance shadow maps for GL renderers.
    // NanoRT traverses SoShadowGroup as a plain separator and picks up
    // SoShadowDirectionalLight (a subclass of SoDirectionalLight) as a
    // normal directional light; shadow rays are controlled by SoRaytracingParams.
    SoShadowGroup* sg = new SoShadowGroup;
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.7f);
    sg->precision.setValue(0.5f);
    sg->quality.setValue(1.0f);
    root->addChild(sg);

    // Shadow-casting directional light (works for both GL shadow maps and
    // nanort shadow rays)
    SoShadowDirectionalLight* slight = new SoShadowDirectionalLight;
    slight->direction.setValue(-0.4f, -1.0f, -0.5f);
    slight->intensity.setValue(1.0f);
    slight->color.setValue(SbColor(1.0f, 0.95f, 0.85f));
    sg->addChild(slight);

    // Floor plane – receives shadows
    {
        SoSeparator* planeSep = new SoSeparator;

        SoShadowStyle* ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::SHADOWED);
        planeSep->addChild(ss);

        SoTranslation* pt = new SoTranslation;
        pt->translation.setValue(0.0f, -1.2f, 0.0f);
        planeSep->addChild(pt);

        SoScale* ps = new SoScale;
        ps->scaleFactor.setValue(8.0f, 0.08f, 8.0f);
        planeSep->addChild(ps);

        SoMaterial* pmat = new SoMaterial;
        pmat->diffuseColor.setValue(0.65f, 0.65f, 0.65f);
        planeSep->addChild(pmat);

        planeSep->addChild(new SoCube);
        sg->addChild(planeSep);
    }

    // Sphere – casts a shadow onto the floor
    {
        SoSeparator* sphSep = new SoSeparator;

        SoShadowStyle* ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        sphSep->addChild(ss);

        SoTranslation* st = new SoTranslation;
        st->translation.setValue(0.0f, 0.2f, 0.0f);
        sphSep->addChild(st);

        SoMaterial* smat = new SoMaterial;
        smat->diffuseColor.setValue(0.7f, 0.2f, 0.2f);
        smat->specularColor.setValue(0.8f, 0.8f, 0.8f);
        smat->shininess.setValue(0.6f);
        sphSep->addChild(smat);

        SoSphere* sphere = new SoSphere;
        sphere->radius.setValue(0.9f);
        sphSep->addChild(sphere);

        sg->addChild(sphSep);
    }

    // Small cube off to the side – also casts a shadow
    {
        SoSeparator* cubeSep = new SoSeparator;

        SoShadowStyle* ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        cubeSep->addChild(ss);

        SoTranslation* ct = new SoTranslation;
        ct->translation.setValue(2.0f, -0.4f, -0.5f);
        cubeSep->addChild(ct);

        SoMaterial* cmat = new SoMaterial;
        cmat->diffuseColor.setValue(0.2f, 0.4f, 0.8f);
        cmat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        cmat->shininess.setValue(0.4f);
        cubeSep->addChild(cmat);

        SoCube* cube = new SoCube;
        cube->width.setValue(0.9f);
        cube->height.setValue(0.9f);
        cube->depth.setValue(0.9f);
        cubeSep->addChild(cube);

        sg->addChild(cubeSep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 12. Draggers
// =========================================================================
SoSeparator* createDraggers(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // Translate1Dragger (horizontal arrow)
    SoSeparator* d1sep = new SoSeparator;
    SoTranslation* d1pos = new SoTranslation;
    d1pos->translation.setValue(0.0f, 1.5f, 0.0f);
    d1sep->addChild(d1pos);
    d1sep->addChild(new SoTranslate1Dragger);
    root->addChild(d1sep);

    // RotateSphericalDragger (ball-in-ring)
    SoSeparator* d2sep = new SoSeparator;
    SoTranslation* d2pos = new SoTranslation;
    d2pos->translation.setValue(0.0f, -1.5f, 0.0f);
    d2sep->addChild(d2pos);
    d2sep->addChild(new SoRotateSphericalDragger);
    root->addChild(d2sep);

    // Central geometry for reference
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f, 0.6f, 0.9f);
    root->addChild(mat);
    SoCube* cube = new SoCube;
    cube->width.setValue(0.6f);
    cube->height.setValue(0.6f);
    cube->depth.setValue(0.6f);
    root->addChild(cube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 13. HUD (head-up display overlay)
// =========================================================================
SoSeparator* createHUD(int width, int height)
{
    // Root: perspective 3-D scene + SoHUDKit pixel-space overlay.
    SoSeparator* root = new SoSeparator;
    root->ref();

    // --- 3-D scene ---
    SoSeparator* scene3d = new SoSeparator;
    SoPerspectiveCamera* cam3d = addCameraAndLight(scene3d);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f, 0.5f, 0.8f);
    scene3d->addChild(mat);
    scene3d->addChild(new SoCube);
    SbViewportRegion vp(width, height);
    cam3d->viewAll(scene3d, vp);
    root->addChild(scene3d);

    // --- 2-D HUD overlay (pixel-space, 1 unit = 1 pixel, origin = lower-left) ---
    SoHUDKit* hud = new SoHUDKit;

    SoHUDLabel* label = new SoHUDLabel;
    label->position.setValue(10.0f, (float)height - 30.0f);
    label->string.setValue("HUD Overlay");
    label->color.setValue(SbColor(1.0f, 1.0f, 0.2f));
    label->fontSize.setValue(14.0f);
    label->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(label);

    root->addChild(hud);
    return root;
}

// =========================================================================
// 14. Level of detail (SoLOD)
// =========================================================================
SoSeparator* createLOD(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    SoLOD* lod = new SoLOD;
    lod->range.set1Value(0, 5.0f);
    lod->range.set1Value(1, 12.0f);

    // High detail
    SoSeparator* hi = new SoSeparator;
    SoMaterial* himat = new SoMaterial;
    himat->diffuseColor.setValue(0.1f, 0.8f, 0.1f);
    hi->addChild(himat);
    SoSphere* hi_sphere = new SoSphere;
    hi_sphere->radius.setValue(1.0f);
    hi->addChild(hi_sphere);
    lod->addChild(hi);

    // Medium detail
    SoSeparator* med = new SoSeparator;
    SoMaterial* medmat = new SoMaterial;
    medmat->diffuseColor.setValue(0.8f, 0.6f, 0.1f);
    med->addChild(medmat);
    med->addChild(new SoCube);
    lod->addChild(med);

    // Low detail
    SoSeparator* lo = new SoSeparator;
    SoMaterial* lomat = new SoMaterial;
    lomat->diffuseColor.setValue(0.8f, 0.1f, 0.1f);
    lo->addChild(lomat);
    SoCone* lo_cone = new SoCone;
    lo_cone->bottomRadius.setValue(1.0f);
    lo_cone->height.setValue(2.0f);
    lo->addChild(lo_cone);
    lod->addChild(lo);

    root->addChild(lod);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 15. Transparency
// =========================================================================
SoSeparator* createTransparency(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // Enable blended transparency
    SoTransparencyType* ttype = new SoTransparencyType;
    ttype->value.setValue(SoTransparencyType::BLEND);
    root->addChild(ttype);

    // Back sphere: opaque red
    SoSeparator* back = new SoSeparator;
    SoTranslation* bt = new SoTranslation;
    bt->translation.setValue(0.0f, 0.0f, -1.5f);
    back->addChild(bt);
    SoMaterial* bmat = new SoMaterial;
    bmat->diffuseColor.setValue(0.9f, 0.1f, 0.1f);
    bmat->transparency.setValue(0.0f);
    back->addChild(bmat);
    SoSphere* bsphere = new SoSphere;
    bsphere->radius.setValue(0.9f);
    back->addChild(bsphere);
    root->addChild(back);

    // Front sphere: semi-transparent blue
    SoSeparator* front = new SoSeparator;
    SoMaterial* fmat = new SoMaterial;
    fmat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);
    fmat->transparency.setValue(0.55f);
    front->addChild(fmat);
    SoSphere* fsphere = new SoSphere;
    fsphere->radius.setValue(0.9f);
    front->addChild(fsphere);
    root->addChild(front);

    // Small green sphere (opaque, partially behind the transparent one)
    SoSeparator* mid = new SoSeparator;
    SoTranslation* mt = new SoTranslation;
    mt->translation.setValue(0.5f, 0.5f, 0.2f);
    mid->addChild(mt);
    SoMaterial* mmat = new SoMaterial;
    mmat->diffuseColor.setValue(0.1f, 0.9f, 0.1f);
    mid->addChild(mmat);
    SoSphere* msphere = new SoSphere;
    msphere->radius.setValue(0.4f);
    mid->addChild(msphere);
    root->addChild(mid);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 16. DrawStyle
// =========================================================================
SoSeparator* createDrawStyle(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 0.3f);
    mat->specularColor.setValue(0.7f, 0.7f, 0.7f);
    mat->shininess.setValue(0.4f);
    root->addChild(mat);

    const struct { SoDrawStyle::Style s; float tx; const char* label; } styles[] = {
        { SoDrawStyle::FILLED, -3.0f, "Filled" },
        { SoDrawStyle::LINES,   0.0f, "Lines"  },
        { SoDrawStyle::POINTS,  3.0f, "Points" },
    };
    for (const auto& st : styles) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(st.tx, 0.0f, 0.0f);
        sep->addChild(t);
        SoDrawStyle* ds = new SoDrawStyle;
        ds->style.setValue(st.s);
        ds->lineWidth.setValue(2.0f);
        ds->pointSize.setValue(4.0f);
        sep->addChild(ds);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 17. IndexedFaceSet (tetrahedron)
// =========================================================================
SoSeparator* createIndexedFaceSet(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // Octahedron: 6 vertices, 8 triangular faces with per-face colouring so
    // the distinct faces are immediately visible from any viewpoint.
    static const float pts[6][3] = {
        {  0.0f,  1.4f,  0.0f },  // top
        {  1.0f,  0.0f,  0.0f },  // right
        {  0.0f,  0.0f,  1.0f },  // front
        { -1.0f,  0.0f,  0.0f },  // left
        {  0.0f,  0.0f, -1.0f },  // back
        {  0.0f, -1.4f,  0.0f },  // bottom
    };
    // 8 CCW triangular faces (top half + bottom half)
    static const int32_t idx[] = {
        0, 1, 2, -1,   // top-right-front
        0, 2, 3, -1,   // top-front-left
        0, 3, 4, -1,   // top-left-back
        0, 4, 1, -1,   // top-back-right
        5, 2, 1, -1,   // bot-front-right
        5, 3, 2, -1,   // bot-left-front
        5, 4, 3, -1,   // bot-back-left
        5, 1, 4, -1,   // bot-right-back
    };
    // Per-face material colours (one per triangle)
    static const float faceColors[8][3] = {
        {0.9f,0.2f,0.2f}, {0.2f,0.8f,0.2f},
        {0.2f,0.3f,0.9f}, {0.9f,0.8f,0.1f},
        {0.8f,0.3f,0.8f}, {0.1f,0.8f,0.8f},
        {0.9f,0.5f,0.1f}, {0.4f,0.9f,0.5f},
    };

    SoCoordinate3* co = new SoCoordinate3;
    co->point.setValues(0, 6, pts);
    root->addChild(co);

    SoShapeHints* sh = new SoShapeHints;
    sh->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    sh->shapeType.setValue(SoShapeHints::SOLID);
    root->addChild(sh);

    // Build SoMaterial with per-face diffuse colours
    SoMaterial* mat = new SoMaterial;
    for (int f = 0; f < 8; ++f)
        mat->diffuseColor.set1Value(f, faceColors[f][0], faceColors[f][1], faceColors[f][2]);
    mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
    mat->shininess.setValue(0.4f);
    root->addChild(mat);

    SoMaterialBinding* mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_FACE);
    root->addChild(mb);

    SoIndexedFaceSet* ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0, 32, idx);
    root->addChild(ifs);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 18. Manipulators demo
// =========================================================================
SoSeparator* createManips(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // Left side: SoTrackballManip on a sphere
    SoSeparator* left = new SoSeparator;
    SoTranslation* lt = new SoTranslation;
    lt->translation.setValue(-2.5f, 0.0f, 0.0f);
    left->addChild(lt);
    SoTrackballManip* tbm = new SoTrackballManip;
    left->addChild(tbm);
    SoMaterial* lmat = new SoMaterial;
    lmat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    left->addChild(lmat);
    left->addChild(new SoSphere);
    root->addChild(left);

    // Right side: SoTabBoxManip on a cube
    SoSeparator* right = new SoSeparator;
    SoTranslation* rt = new SoTranslation;
    rt->translation.setValue(2.5f, 0.0f, 0.0f);
    right->addChild(rt);
    SoTabBoxManip* tabm = new SoTabBoxManip;
    right->addChild(tabm);
    SoMaterial* rmat = new SoMaterial;
    rmat->diffuseColor.setValue(0.2f, 0.6f, 0.9f);
    right->addChild(rmat);
    right->addChild(new SoCube);
    root->addChild(right);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 19. Scene (2×2 primitive grid — from render_scene)
// =========================================================================
static void addSceneGridObject(SoSeparator *root,
                             float r, float g, float b,
                             float tx, float ty,
                             SoNode *shape)
{
    SoSeparator *sep = new SoSeparator;
    SoTranslation *t = new SoTranslation;
    t->translation.setValue(tx, ty, 0);
    sep->addChild(t);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor .setValue(r, g, b);
    mat->specularColor.setValue(0.4f, 0.4f, 0.4f);
    mat->shininess    .setValue(0.3f);
    sep->addChild(mat);
    sep->addChild(shape);
    root->addChild(sep);
}

SoSeparator* createScene(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    const float s = 2.0f;
    addSceneGridObject(root, 0.85f, 0.15f, 0.15f, -s * 0.5f,  s * 0.5f, new SoSphere);
    addSceneGridObject(root, 0.15f, 0.75f, 0.15f,  s * 0.5f,  s * 0.5f, new SoCube);
    addSceneGridObject(root, 0.15f, 0.35f, 0.90f, -s * 0.5f, -s * 0.5f, new SoCone);
    addSceneGridObject(root, 0.90f, 0.75f, 0.15f,  s * 0.5f, -s * 0.5f, new SoCylinder);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.2f);
    return root;
}

// =========================================================================
// 20. FaceSet — emissive green quad in lower-left quadrant
// =========================================================================
SoSeparator* createFaceSet(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoSeparator* faceGrp = new SoSeparator;

    SoMaterial* mat = new SoMaterial;
    mat->emissiveColor.setValue(0.0f, 1.0f, 0.0f);
    mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
    faceGrp->addChild(mat);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 0.0f,  0.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  0.0f, 0.0f));
    faceGrp->addChild(coords);

    SoFaceSet* faces = new SoFaceSet;
    faces->numVertices.set1Value(0, 4);
    faceGrp->addChild(faces);

    root->addChild(faceGrp);
    return root;
}

// =========================================================================
// 21. LineSet — red horizontal line across the viewport
// =========================================================================
SoSeparator* createLineSet(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoSeparator* lineGrp = new SoSeparator;

    SoDrawStyle* ds = new SoDrawStyle;
    ds->lineWidth.setValue(3.0f);
    lineGrp->addChild(ds);

    SoBaseColor* bc = new SoBaseColor;
    bc->rgb.setValue(SbColor(1.0f, 0.0f, 0.0f));
    lineGrp->addChild(bc);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.9f, 0.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.9f, 0.0f, 0.0f));
    lineGrp->addChild(coords);

    SoLineSet* ls = new SoLineSet;
    ls->numVertices.set1Value(0, 2);
    lineGrp->addChild(ls);

    root->addChild(lineGrp);
    return root;
}

// =========================================================================
// 22. IndexedLineSet — green horizontal, red diagonal, blue V
// =========================================================================
SoSeparator* createIndexedLineSet(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoDrawStyle* ds = new SoDrawStyle;
    ds->lineWidth.setValue(3.0f);
    root->addChild(ds);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.9f,  0.5f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.9f,  0.5f, 0.0f));
    coords->point.set1Value(2, SbVec3f(-0.7f, -0.7f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.7f,  0.3f, 0.0f));
    coords->point.set1Value(4, SbVec3f(-0.6f, -0.1f, 0.0f));
    coords->point.set1Value(5, SbVec3f( 0.0f, -0.8f, 0.0f));
    coords->point.set1Value(6, SbVec3f( 0.6f, -0.1f, 0.0f));
    root->addChild(coords);

    // Green horizontal
    SoSeparator* sep1 = new SoSeparator;
    SoBaseColor* bc1 = new SoBaseColor;
    bc1->rgb.setValue(SbColor(0.0f, 1.0f, 0.0f));
    sep1->addChild(bc1);
    SoIndexedLineSet* ils1 = new SoIndexedLineSet;
    ils1->coordIndex.set1Value(0, 0);
    ils1->coordIndex.set1Value(1, 1);
    ils1->coordIndex.set1Value(2, -1);
    sep1->addChild(ils1);
    root->addChild(sep1);

    // Red diagonal
    SoSeparator* sep2 = new SoSeparator;
    SoBaseColor* bc2 = new SoBaseColor;
    bc2->rgb.setValue(SbColor(1.0f, 0.0f, 0.0f));
    sep2->addChild(bc2);
    SoIndexedLineSet* ils2 = new SoIndexedLineSet;
    ils2->coordIndex.set1Value(0, 2);
    ils2->coordIndex.set1Value(1, 3);
    ils2->coordIndex.set1Value(2, -1);
    sep2->addChild(ils2);
    root->addChild(sep2);

    // Blue V
    SoSeparator* sep3 = new SoSeparator;
    SoBaseColor* bc3 = new SoBaseColor;
    bc3->rgb.setValue(SbColor(0.2f, 0.2f, 1.0f));
    sep3->addChild(bc3);
    SoIndexedLineSet* ils3 = new SoIndexedLineSet;
    ils3->coordIndex.set1Value(0, 4);
    ils3->coordIndex.set1Value(1, 5);
    ils3->coordIndex.set1Value(2, -1);
    ils3->coordIndex.set1Value(3, 5);
    ils3->coordIndex.set1Value(4, 6);
    ils3->coordIndex.set1Value(5, -1);
    sep3->addChild(ils3);
    root->addChild(sep3);

    return root;
}

// =========================================================================
// 23. PointSet — four coloured points in four quadrants
// =========================================================================
SoSeparator* createPointSet(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoDrawStyle* ds = new SoDrawStyle;
    ds->pointSize.setValue(20.0f);
    root->addChild(ds);

    SoMaterialBinding* mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX);
    root->addChild(mb);

    SoPackedColor* pc = new SoPackedColor;
    pc->orderedRGBA.set1Value(0, 0xFF0000FFu);
    pc->orderedRGBA.set1Value(1, 0x00FF00FFu);
    pc->orderedRGBA.set1Value(2, 0x0000FFFFu);
    pc->orderedRGBA.set1Value(3, 0xFFFFFFFFu);
    root->addChild(pc);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.5f,  0.5f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.5f,  0.5f, 0.0f));
    coords->point.set1Value(2, SbVec3f(-0.5f, -0.5f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.5f, -0.5f, 0.0f));
    root->addChild(coords);

    root->addChild(new SoPointSet);
    return root;
}

// =========================================================================
// 24. TriangleStripSet — emissive blue strip quad in lower half
// =========================================================================
SoSeparator* createTriangleStripSet(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0.0f, 0.0f, 1.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoSeparator* grp = new SoSeparator;

    SoMaterial* mat = new SoMaterial;
    mat->emissiveColor.setValue(0.0f, 0.0f, 1.0f);
    mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
    grp->addChild(mat);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-0.8f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.8f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f(-0.8f,  0.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.8f,  0.0f, 0.0f));
    grp->addChild(coords);

    SoTriangleStripSet* strips = new SoTriangleStripSet;
    strips->numVertices.set1Value(0, 4);
    grp->addChild(strips);

    root->addChild(grp);
    return root;
}

// =========================================================================
// 25. QuadMesh — 5×5 colour-gradient grid
// =========================================================================
SoSeparator* createQuadMesh(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0.0f, -0.5f, -1.0f);
    root->addChild(light);

    static const int NCOLS = 5;
    static const int NROWS = 5;

    SoMaterialBinding* mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX);
    root->addChild(mb);

    static const float colColors[NCOLS][3] = {
        { 0.9f, 0.1f, 0.1f },
        { 0.9f, 0.5f, 0.1f },
        { 0.9f, 0.9f, 0.1f },
        { 0.1f, 0.8f, 0.1f },
        { 0.1f, 0.2f, 0.9f },
    };

    SoMaterial* mat = new SoMaterial;
    int mi = 0;
    for (int r = 0; r < NROWS; ++r)
        for (int c = 0; c < NCOLS; ++c)
            mat->diffuseColor.set1Value(mi++,
                SbColor(colColors[c][0], colColors[c][1], colColors[c][2]));
    root->addChild(mat);

    float xStep = 2.0f / (NCOLS - 1);
    float yStep = 2.0f / (NROWS - 1);
    SoCoordinate3* coords = new SoCoordinate3;
    int vi = 0;
    for (int r = 0; r < NROWS; ++r)
        for (int c = 0; c < NCOLS; ++c)
            coords->point.set1Value(vi++, SbVec3f(-1.0f + c * xStep,
                                                   -1.0f + r * yStep, 0.0f));
    root->addChild(coords);

    SoNormal* normals = new SoNormal;
    for (int i = 0; i < NCOLS * NROWS; ++i)
        normals->vector.set1Value(i, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(normals);
    SoNormalBinding* nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX);
    root->addChild(nb);

    SoQuadMesh* qm = new SoQuadMesh;
    qm->verticesPerRow   .setValue(NCOLS);
    qm->verticesPerColumn.setValue(NROWS);
    root->addChild(qm);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 26. VertexColors — per-vertex coloured quad
// =========================================================================
SoSeparator* createVertexColors(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 1);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction .setValue(0.0f, 0.0f, -1.0f);
    light->intensity .setValue(1.0f);
    root->addChild(light);

    SoMaterialBinding* mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX_INDEXED);
    root->addChild(mb);

    SoPackedColor* pc = new SoPackedColor;
    pc->orderedRGBA.set1Value(0, 0xFF0000FFu);
    pc->orderedRGBA.set1Value(1, 0x00FF00FFu);
    pc->orderedRGBA.set1Value(2, 0x0000FFFFu);
    pc->orderedRGBA.set1Value(3, 0xFFFF00FFu);
    root->addChild(pc);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoNormal* normals = new SoNormal;
    normals->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(normals);
    SoNormalBinding* nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    SoIndexedFaceSet* ifs = new SoIndexedFaceSet;
    ifs->coordIndex  .set1Value(0, 0);
    ifs->coordIndex  .set1Value(1, 1);
    ifs->coordIndex  .set1Value(2, 2);
    ifs->coordIndex  .set1Value(3, 3);
    ifs->coordIndex  .set1Value(4, -1);
    ifs->materialIndex.set1Value(0, 0);
    ifs->materialIndex.set1Value(1, 1);
    ifs->materialIndex.set1Value(2, 2);
    ifs->materialIndex.set1Value(3, 3);
    ifs->materialIndex.set1Value(4, -1);
    root->addChild(ifs);

    return root;
}

// =========================================================================
// 27. SwitchVisibility — two spheres, both switches on
// =========================================================================
SoSeparator* createSwitchVisibility(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 3);
    cam->nearDistance = 1.0f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0, 0, -1);
    root->addChild(light);

    // Red sphere (left)
    SoSwitch* redSw = new SoSwitch;
    redSw->whichChild.setValue(SO_SWITCH_ALL);
    {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(-0.5f, 0, 0);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->emissiveColor.setValue(1.0f, 0.0f, 0.0f);
        mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
        sep->addChild(mat);
        SoSphere* sph = new SoSphere;
        sph->radius = 0.3f;
        sep->addChild(sph);
        redSw->addChild(sep);
    }
    root->addChild(redSw);

    // Blue sphere (right)
    SoSwitch* blueSw = new SoSwitch;
    blueSw->whichChild.setValue(SO_SWITCH_ALL);
    {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(0.5f, 0, 0);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->emissiveColor.setValue(0.0f, 0.0f, 1.0f);
        mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
        sep->addChild(mat);
        SoSphere* sph = new SoSphere;
        sph->radius = 0.3f;
        sep->addChild(sph);
        blueSw->addChild(sep);
    }
    root->addChild(blueSw);

    return root;
}

// =========================================================================
// 28. SpherePosition — emissive sphere offset from centre
// =========================================================================
SoSeparator* createSpherePosition(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 3);
    cam->nearDistance = 1.0f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0, 0, -1);
    root->addChild(light);

    SoSeparator* sphGrp = new SoSeparator;
    SoMaterial* mat = new SoMaterial;
    mat->emissiveColor.setValue(1.0f, 0.4f, 0.4f);
    mat->diffuseColor .setValue(0, 0, 0);
    sphGrp->addChild(mat);

    SoTransform* xf = new SoTransform;
    xf->translation.setValue(0.3f, 0.2f, 0);
    sphGrp->addChild(xf);

    SoSphere* sph = new SoSphere;
    sph->radius = 0.2f;
    sphGrp->addChild(sph);

    root->addChild(sphGrp);
    return root;
}

// =========================================================================
// 29. CheckerTexture — checkerboard-textured cube
// =========================================================================
SoSeparator* createCheckerTexture(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor .setValue(1, 1, 1);
    mat->ambientColor .setValue(0.2f, 0.2f, 0.2f);
    root->addChild(mat);

    const int tw = 128, th = 128, cs = 32;
    std::vector<unsigned char> texData(tw * th * 3);
    for (int y = 0; y < th; ++y)
        for (int x = 0; x < tw; ++x) {
            unsigned char v = (((x / cs) + (y / cs)) % 2) ? 255 : 0;
            texData[(y * tw + x) * 3 + 0] = v;
            texData[(y * tw + x) * 3 + 1] = v;
            texData[(y * tw + x) * 3 + 2] = v;
        }

    SoTexture2* tex = new SoTexture2;
    tex->setImageData(tw, th, 3, texData.data());
    root->addChild(tex);
    root->addChild(new SoTextureCoordinateDefault);
    root->addChild(new SoCube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 30. ClipPlane — large sphere clipped at Y=0
// =========================================================================
SoSeparator* createClipPlane(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 5);
    cam->nearDistance = 1.0f;
    cam->farDistance  = 20.0f;
    cam->height       = 2.4f;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(0.0f, -1.0f, -1.0f);
    root->addChild(light);

    SoClipPlane* cp = new SoClipPlane;
    cp->plane.setValue(SbPlane(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f));
    root->addChild(cp);

    SoMaterial* mat = new SoMaterial;
    mat->emissiveColor.setValue(0.9f, 0.1f, 0.1f);
    mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
    root->addChild(mat);

    SoSphere* sph = new SoSphere;
    sph->radius = 1.0f;
    root->addChild(sph);

    return root;
}

// =========================================================================
// 31. ArrayMultipleCopy — 3×3 SoArray grid + 3 SoMultipleCopy cubes
// =========================================================================
SoSeparator* createArrayMultipleCopy(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    // 3×3 grid of blue spheres via SoArray
    {
        SoSeparator* arraySep = new SoSeparator;
        SoTranslation* offset = new SoTranslation;
        offset->translation.setValue(-2.5f, 1.5f, 0.0f);
        arraySep->addChild(offset);

        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor .setValue(0.2f, 0.4f, 0.9f);
        mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
        mat->shininess    .setValue(0.4f);
        arraySep->addChild(mat);

        SoArray* arr = new SoArray;
        arr->origin     .setValue(SoArray::FIRST);
        arr->numElements1.setValue(3);
        arr->numElements2.setValue(3);
        arr->numElements3.setValue(1);
        arr->separation1 .setValue(1.2f, 0.0f, 0.0f);
        arr->separation2 .setValue(0.0f, -1.2f, 0.0f);
        arr->separation3 .setValue(0.0f,  0.0f, 0.0f);

        SoSphere* sph = new SoSphere;
        sph->radius.setValue(0.4f);
        arr->addChild(sph);
        arraySep->addChild(arr);
        root->addChild(arraySep);
    }

    // 3 orange cubes via SoMultipleCopy
    {
        SoSeparator* mcSep = new SoSeparator;

        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor .setValue(0.9f, 0.5f, 0.1f);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess    .setValue(0.3f);
        mcSep->addChild(mat);

        SoMultipleCopy* mc = new SoMultipleCopy;
        static const float tx[3] = { -1.5f, 0.0f, 1.5f };
        for (int i = 0; i < 3; ++i) {
            SbMatrix m;
            m.setTranslate(SbVec3f(tx[i], -1.8f, 0.0f));
            mc->matrix.set1Value(i, m);
        }

        SoCube* cube = new SoCube;
        cube->width .setValue(0.7f);
        cube->height.setValue(0.7f);
        cube->depth .setValue(0.7f);
        mc->addChild(cube);
        mcSep->addChild(mc);
        root->addChild(mcSep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 32. Annotation — red sphere on top of a blue background sphere
// =========================================================================
SoSeparator* createAnnotation(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 5);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    cam->height       = 4.0f;
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    // Background sphere
    {
        SoSeparator* grp = new SoSeparator;
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(0.3f, 0.3f, 0.8f);
        grp->addChild(mat);
        SoTranslation* tr = new SoTranslation;
        tr->translation.setValue(0, 0, -2.0f);
        grp->addChild(tr);
        SoSphere* sph = new SoSphere;
        sph->radius = 0.5f;
        grp->addChild(sph);
        root->addChild(grp);
    }

    // Annotation node renders on top regardless of depth
    {
        SoAnnotation* ann = new SoAnnotation;
        SoMaterial* mat = new SoMaterial;
        mat->emissiveColor.setValue(1.0f, 0.2f, 0.2f);
        ann->addChild(mat);
        SoSphere* sph = new SoSphere;
        sph->radius = 0.4f;
        ann->addChild(sph);
        root->addChild(ann);
    }

    return root;
}

// =========================================================================
// 33. AsciiText — SoAsciiText "HELLO" with perspective camera
// =========================================================================
SoSeparator* createAsciiText(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.5f, -0.8f);
    root->addChild(light);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor .setValue(0.9f, 0.9f, 0.9f);
    mat->emissiveColor.setValue(0.5f, 0.5f, 0.5f);
    root->addChild(mat);

    SoAsciiText* text = new SoAsciiText;
    text->string.setValue("HELLO");
    text->justification.setValue(SoAsciiText::CENTER);
    root->addChild(text);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    // Pull back to leave comfortable margins around the text; adjust near/far
    // proportionally so the text stays within the view frustum.
    SbVec3f pos = cam->position.getValue();
    const float scale = 1.5f;
    cam->position.setValue(pos[0], pos[1], pos[2] * scale);
    cam->nearDistance.setValue(cam->nearDistance.getValue() * scale);
    cam->farDistance.setValue(cam->farDistance.getValue() * scale);
    return root;
}

// =========================================================================
// 34. ResetTransform — blue sphere at offset + red sphere reset to origin
// =========================================================================
SoSeparator* createResetTransform(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0, 0, 5);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    cam->height       = 4.0f;
    cam->aspectRatio  = (float)width / (float)height;
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    // Blue sphere translated right
    {
        SoSeparator* grp = new SoSeparator;
        SoTranslation* tr = new SoTranslation;
        tr->translation.setValue(1.5f, 0.0f, 0.0f);
        grp->addChild(tr);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(0.0f, 0.3f, 1.0f);
        grp->addChild(mat);
        SoSphere* sph = new SoSphere;
        sph->radius = 0.5f;
        grp->addChild(sph);
        root->addChild(grp);
    }

    // Same translation context cleared by SoResetTransform → red sphere at origin
    {
        SoSeparator* grp = new SoSeparator;
        SoTranslation* tr = new SoTranslation;
        tr->translation.setValue(1.5f, 0.0f, 0.0f);
        grp->addChild(tr);
        SoResetTransform* rst = new SoResetTransform;
        rst->whatToReset.setValue(SoResetTransform::TRANSFORM);
        grp->addChild(rst);
        SoMaterial* mat = new SoMaterial;
        mat->emissiveColor.setValue(1.0f, 0.1f, 0.1f);
        mat->diffuseColor .setValue(0.0f, 0.0f, 0.0f);
        grp->addChild(mat);
        SoSphere* sph = new SoSphere;
        sph->radius = 0.5f;
        grp->addChild(sph);
        root->addChild(grp);
    }

    return root;
}

// =========================================================================
// 35. ShapeHints — SOLID+CCW purple sphere (frame 1 config)
// =========================================================================
SoSeparator* createShapeHints(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->position    .setValue(0.0f, 0.0f, 4.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoShapeHints* hints = new SoShapeHints;
    hints->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    hints->shapeType     .setValue(SoShapeHints::SOLID);
    hints->faceType      .setValue(SoShapeHints::CONVEX);
    hints->creaseAngle   .setValue(0.5f);
    root->addChild(hints);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor .setValue(0.6f, 0.3f, 0.9f);
    mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
    mat->shininess    .setValue(0.4f);
    root->addChild(mat);

    root->addChild(new SoSphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 36. ImageNode — red/green checkerboard SoImage
// =========================================================================
SoSeparator* createImageNode(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position    .setValue(0.0f, 0.0f, 1.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.0f;
    root->addChild(cam);

    const int IMG_W = 32, IMG_H = 32;
    unsigned char pixels[IMG_W * IMG_H * 3];
    for (int row = 0; row < IMG_H; ++row)
        for (int col = 0; col < IMG_W; ++col) {
            unsigned char* p = pixels + (row * IMG_W + col) * 3;
            if ((row + col) % 2 == 0) { p[0] = 255; p[1] = 0;   p[2] = 0; }
            else                       { p[0] = 0;   p[1] = 255; p[2] = 0; }
        }

    SoImage* img = new SoImage;
    img->image.setValue(SbVec2s(IMG_W, IMG_H), 3, pixels);
    root->addChild(img);

    return root;
}

// =========================================================================
// 37. MarkerSet — five markers in a cross pattern
// =========================================================================
SoSeparator* createMarkerSet(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoMaterial* mat = new SoMaterial;
    mat->emissiveColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoCoordinate3* coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f( 0.0f,  0.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 0.5f,  0.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f(-0.5f,  0.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f( 0.0f,  0.5f, 0.0f));
    coords->point.set1Value(4, SbVec3f( 0.0f, -0.5f, 0.0f));
    root->addChild(coords);

    SoMarkerSet* markers = new SoMarkerSet;
    markers->markerIndex.set1Value(0, SoMarkerSet::CIRCLE_FILLED_5_5);
    markers->markerIndex.set1Value(1, SoMarkerSet::SQUARE_FILLED_5_5);
    markers->markerIndex.set1Value(2, SoMarkerSet::DIAMOND_FILLED_5_5);
    markers->markerIndex.set1Value(3, SoMarkerSet::CIRCLE_FILLED_7_7);
    markers->markerIndex.set1Value(4, SoMarkerSet::SQUARE_FILLED_7_7);
    root->addChild(markers);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 38. MaterialBinding — PER_FACE: red left quad, blue right quad
// =========================================================================
SoSeparator* createMaterialBinding(int width, int height)
{
    (void)width; (void)height;
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoOrthographicCamera* cam = new SoOrthographicCamera;
    cam->position  .setValue(0.0f, 0.0f, 5.0f);
    cam->height    .setValue(6.0f);
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    SoMaterialBinding* mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_FACE);
    root->addChild(mb);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.set1Value(0, SbColor(0.9f, 0.1f, 0.1f));
    mat->diffuseColor.set1Value(1, SbColor(0.1f, 0.1f, 0.9f));
    root->addChild(mat);

    static const float quadCoords[8][3] = {
        {-2.0f,-1.0f, 0.0f}, {-0.2f,-1.0f, 0.0f},
        {-0.2f, 1.0f, 0.0f}, {-2.0f, 1.0f, 0.0f},
        { 0.2f,-1.0f, 0.0f}, { 2.0f,-1.0f, 0.0f},
        { 2.0f, 1.0f, 0.0f}, { 0.2f, 1.0f, 0.0f}
    };
    SoCoordinate3* c3 = new SoCoordinate3;
    c3->point.setValues(0, 8, quadCoords);
    root->addChild(c3);

    static const SbVec3f faceNormals[2] = {
        SbVec3f(0,0,1), SbVec3f(0,0,1)
    };
    SoNormal* n = new SoNormal;
    n->vector.setValues(0, 2, faceNormals);
    root->addChild(n);
    SoNormalBinding* nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_FACE);
    root->addChild(nb);

    static const int quadVertCounts[2] = { 4, 4 };
    SoFaceSet* fs = new SoFaceSet;
    fs->numVertices.setValues(0, 2, quadVertCounts);
    root->addChild(fs);

    return root;
}

} // namespace Scenes
} // namespace ObolTest

// =========================================================================
// Additional includes for texture / visual / HUD factories (appended)
// =========================================================================
#include <Inventor/nodes/SoAlphaTest.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoBumpMap.h>
#include <Inventor/nodes/SoBumpMapCoordinate.h>
#include <Inventor/nodes/SoTexture2Transform.h>
#include <Inventor/nodes/SoTexture3.h>
#include <Inventor/nodes/SoTextureUnit.h>
#include <Inventor/nodes/SoEnvironment.h>
#include <Inventor/nodes/SoTextureCubeMap.h>
#include <Inventor/nodes/SoTextureCoordinateEnvironment.h>
#include <Inventor/nodes/SoSceneTexture2.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>
#include <Inventor/nodes/SoDepthBuffer.h>
#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoTextureScalePolicy.h>
#include <Inventor/nodes/SoShaderProgram.h>
#include <Inventor/nodes/SoVertexShader.h>
#include <Inventor/nodes/SoFragmentShader.h>
#include <Inventor/nodes/SoShaderParameter.h>
#include <Inventor/fields/SoSFImage3.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec4f.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ObolTest {
namespace Scenes {

// =========================================================================
// 39. AlphaTest — textured quad with SoAlphaTest in GREATER mode
// =========================================================================
//
// The canonical scene renders with SoAlphaTest::GREATER(0.5) to demonstrate
// the alpha test in action.  The 16×16 RGBA checkerboard texture (alpha=255
// at even-(x+y) texels, alpha=0 elsewhere) produces a characteristic pattern:
//   • Most rows show a fine red/black checkerboard (alternating pass/fail).
//   • A small number of rows are entirely black.  These occur when the
//     bilinear T-fraction between two vertically-adjacent phase-inverted
//     texel rows lands exactly at 0.5, making every x-position produce
//     alpha=0.5 which fails the strict GREATER(0.5) test.
// These all-black rows ARE the correct, expected rendering output for this
// scene; they are not artifacts.

// Build a 16×16 RGBA checkerboard: opaque red / transparent white
static void ts_buildAlphaTexture(SoTexture2 *tex)
{
    const int S  = 16;
    const int NC = 4;
    unsigned char buf[S * S * NC];
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            int idx = (y * S + x) * NC;
            if ((x + y) % 2 == 0) {
                buf[idx]   = 200; buf[idx+1] = 50; buf[idx+2] = 50;
                buf[idx+3] = 255; // opaque red
            } else {
                buf[idx]   = 255; buf[idx+1] = 255; buf[idx+2] = 255;
                buf[idx+3] = 0;   // fully transparent white
            }
        }
    }
    tex->image.setValue(SbVec2s(S, S), NC, buf);
    tex->wrapS.setValue(SoTexture2::REPEAT);
    tex->wrapT.setValue(SoTexture2::REPEAT);
    tex->model.setValue(SoTexture2::REPLACE);
}

SoSeparator* createAlphaTest(int width, int height)
{
    (void)width; (void)height;
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(2.2f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    // GREATER threshold: only fragments with alpha > 0.5 pass.
    SoAlphaTest *at = new SoAlphaTest;
    at->function.setValue(SoAlphaTest::GREATER);
    at->value.setValue(0.5f);
    root->addChild(at);

    SoTexture2 *tex = new SoTexture2;
    ts_buildAlphaTexture(tex);
    root->addChild(tex);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(4.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(4.0f, 4.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 4.0f));
    root->addChild(tc);

    SoNormal *nrm = new SoNormal;
    nrm->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(nrm);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    root->addChild(nb);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 4);
    root->addChild(fs);

    return root;
}

// =========================================================================
// 40. BackgroundGradient — 2×2 primitive grid (gradient set on renderer)
// =========================================================================
SoSeparator* createBackgroundGradient(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    struct PrimSpec { float r, g, b, tx, ty; SoNode *shape; };
    const float s = 2.5f;
    PrimSpec specs[] = {
        { 0.85f, 0.15f, 0.15f, -s*0.5f,  s*0.5f, new SoSphere   },
        { 0.15f, 0.75f, 0.15f,  s*0.5f,  s*0.5f, new SoCube     },
        { 0.15f, 0.35f, 0.90f, -s*0.5f, -s*0.5f, new SoCone     },
        { 0.90f, 0.75f, 0.15f,  s*0.5f, -s*0.5f, new SoCylinder },
    };
    for (int i = 0; i < 4; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(specs[i].tx, specs[i].ty, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(specs[i].r, specs[i].g, specs[i].b);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);
        sep->addChild(specs[i].shape);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.1f);

    return root;
}

// =========================================================================
// 41. BumpMap — sphere with SoBumpMap normal-map texture
// =========================================================================
static void ts_buildNormalMap(SoBumpMap *bump)
{
    const int S  = 32;
    const int NC = 4;
    unsigned char buf[S * S * NC];
    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            float phase = (float)y / (float)S * 2.0f * (float)M_PI * 4.0f;
            float ny = sinf(phase) * 0.5f;
            float nz = sqrtf(1.0f - ny * ny);
            int idx  = (y * S + x) * NC;
            buf[idx]   = 128;
            buf[idx+1] = (unsigned char)((ny * 0.5f + 0.5f) * 255.0f);
            buf[idx+2] = (unsigned char)((nz * 0.5f + 0.5f) * 255.0f);
            buf[idx+3] = 255;
        }
    }
    bump->image.setValue(SbVec2s(S, S), NC, buf);
}

SoSeparator* createBumpMap(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 4.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    light->intensity.setValue(1.0f);
    root->addChild(light);

    SoBumpMap *bump = new SoBumpMap;
    ts_buildNormalMap(bump);
    root->addChild(bump);

    root->addChild(new SoBumpMapCoordinate);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 1.0f);
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.6f);
    root->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius.setValue(1.2f);
    root->addChild(sph);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 42. MultiTexture — sphere with two SoTextureUnit texture units
// =========================================================================
static void ts_makeCheckerRGB(unsigned char *data, int w, int h, int cs,
                               unsigned char r0, unsigned char g0, unsigned char b0,
                               unsigned char r1, unsigned char g1, unsigned char b1)
{
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool cell0 = (((x / cs) + (y / cs)) & 1) == 0;
            unsigned char *p = data + (y * w + x) * 3;
            p[0] = cell0 ? r0 : r1;
            p[1] = cell0 ? g0 : g1;
            p[2] = cell0 ? b0 : b1;
        }
    }
}

SoSeparator* createMultiTexture(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    root->addChild(light);

    const int TEX_SIZE = 32;

    SoTextureUnit *tu0 = new SoTextureUnit;
    tu0->unit = 0;
    root->addChild(tu0);

    SoTexture2 *tex0 = new SoTexture2;
    {
        unsigned char data[TEX_SIZE * TEX_SIZE * 3];
        ts_makeCheckerRGB(data, TEX_SIZE, TEX_SIZE, 8,
                          220, 30, 30, 220, 220, 220);
        tex0->image.setValue(SbVec2s(TEX_SIZE, TEX_SIZE), 3, data);
        tex0->model = SoTexture2::MODULATE;
    }
    root->addChild(tex0);

    SoTextureUnit *tu1 = new SoTextureUnit;
    tu1->unit = 1;
    root->addChild(tu1);

    SoTexture2 *tex1 = new SoTexture2;
    {
        unsigned char data[TEX_SIZE * TEX_SIZE * 3];
        for (int i = 0; i < TEX_SIZE * TEX_SIZE; ++i) {
            data[i*3+0] = 30; data[i*3+1] = 30; data[i*3+2] = 180;
        }
        tex1->image.setValue(SbVec2s(TEX_SIZE, TEX_SIZE), 3, data);
        tex1->model = SoTexture2::MODULATE;
    }
    root->addChild(tex1);

    SoTextureUnit *tuReset = new SoTextureUnit;
    tuReset->unit = 0;
    root->addChild(tuReset);

    SoSphere *sphere = new SoSphere;
    sphere->radius.setValue(1.2f);
    root->addChild(sphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 43. Texture3 — cube with a procedural 8×8×8 SoTexture3
// =========================================================================
static void ts_buildTexture3(SoTexture3 *tex)
{
    const int S  = 8;
    const int NC = 4;
    unsigned char buf[S * S * S * NC];
    for (int z = 0; z < S; ++z) {
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                int idx = (z * S * S + y * S + x) * NC;
                if ((x + y + z) % 2 == 0) {
                    buf[idx]=200; buf[idx+1]=50; buf[idx+2]=50;
                } else {
                    buf[idx]=50; buf[idx+1]=50; buf[idx+2]=200;
                }
                buf[idx+3] = 255;
            }
        }
    }
    tex->images.setValue(SbVec3s(S, S, S), NC, buf);
    tex->wrapR.setValue(SoTexture3::REPEAT);
    tex->wrapS.setValue(SoTexture3::REPEAT);
    tex->wrapT.setValue(SoTexture3::REPEAT);
}

SoSeparator* createTexture3(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoTexture3 *tex3 = new SoTexture3;
    ts_buildTexture3(tex3);
    root->addChild(tex3);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoCube *cube = new SoCube;
    cube->width .setValue(2.0f);
    cube->height.setValue(2.0f);
    cube->depth .setValue(2.0f);
    root->addChild(cube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 44. TextureTransform — two textured quads, one with SoTexture2Transform
// =========================================================================
static void ts_buildChecker(SoTexture2 *tex)
{
    const int TILE = 4, SIZE = 32, NC = 3;
    unsigned char buf[SIZE * SIZE * NC];
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            int idx = (y * SIZE + x) * NC;
            if (((x / TILE) + (y / TILE)) % 2 == 0) {
                buf[idx]=200; buf[idx+1]=40; buf[idx+2]=40;
            } else {
                buf[idx]=255; buf[idx+1]=255; buf[idx+2]=255;
            }
        }
    }
    tex->image.setValue(SbVec2s(SIZE, SIZE), NC, buf);
    tex->wrapS.setValue(SoTexture2::REPEAT);
    tex->wrapT.setValue(SoTexture2::REPEAT);
}

static SoSeparator* ts_buildTexturedQuad(bool withTransform)
{
    SoSeparator *sep = new SoSeparator;

    SoTexture2 *tex = new SoTexture2;
    ts_buildChecker(tex);
    sep->addChild(tex);

    if (withTransform) {
        SoTexture2Transform *xf = new SoTexture2Transform;
        xf->scaleFactor.setValue(2.0f, 2.0f);
        xf->rotation.setValue(0.785398f); // 45 deg
        xf->translation.setValue(0.1f, 0.1f);
        sep->addChild(xf);
    }

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    sep->addChild(mat);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    sep->addChild(tc);

    SoNormal *nrm = new SoNormal;
    nrm->vector.set1Value(0, SbVec3f(0.0f, 0.0f, 1.0f));
    sep->addChild(nrm);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::OVERALL);
    sep->addChild(nb);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    sep->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 4);
    sep->addChild(fs);

    return sep;
}

SoSeparator* createTextureTransform(int width, int height)
{
    (void)width; (void)height;
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(2.5f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 20.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    {
        SoSeparator *leftSep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-1.3f, 0.0f, 0.0f);
        leftSep->addChild(t);
        leftSep->addChild(ts_buildTexturedQuad(false));
        root->addChild(leftSep);
    }
    {
        SoSeparator *rightSep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(1.3f, 0.0f, 0.0f);
        rightSep->addChild(t);
        rightSep->addChild(ts_buildTexturedQuad(true));
        root->addChild(rightSep);
    }

    return root;
}

// =========================================================================
// 45. Environment — sphere with SoEnvironment (no fog, high ambient)
// =========================================================================
SoSeparator* createEnvironment(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 4.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoEnvironment *env = new SoEnvironment;
    env->ambientIntensity.setValue(0.9f);
    env->ambientColor.setValue(1.0f, 1.0f, 1.0f);
    env->fogType.setValue(SoEnvironment::NONE);
    root->addChild(env);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    mat->ambientColor.setValue(0.8f, 0.2f, 0.2f);
    root->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius.setValue(1.0f);
    root->addChild(sph);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 46. Cubemap — sphere with SoTextureCubeMap (six solid-colour faces)
// =========================================================================
static void ts_fillFace(unsigned char *data, int w, int h,
                         unsigned char r, unsigned char g, unsigned char b)
{
    for (int i = 0; i < w * h; ++i) {
        data[i*4+0] = r; data[i*4+1] = g;
        data[i*4+2] = b; data[i*4+3] = 255;
    }
}

SoSeparator* createCubemap(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(light);

    const int FS = 64;
    SoTextureCubeMap *cubeMap = new SoTextureCubeMap;
    cubeMap->model = SoTextureCubeMap::REPLACE;

    unsigned char faceData[FS * FS * 4];
    SbVec2s faceSize(FS, FS);

    ts_fillFace(faceData, FS, FS, 255,   0,   0); cubeMap->imagePosX.setValue(faceSize, 4, faceData);
    ts_fillFace(faceData, FS, FS,   0, 255,   0); cubeMap->imageNegX.setValue(faceSize, 4, faceData);
    ts_fillFace(faceData, FS, FS,   0,   0, 255); cubeMap->imagePosY.setValue(faceSize, 4, faceData);
    ts_fillFace(faceData, FS, FS, 255, 255,   0); cubeMap->imageNegY.setValue(faceSize, 4, faceData);
    ts_fillFace(faceData, FS, FS,   0, 255, 255); cubeMap->imagePosZ.setValue(faceSize, 4, faceData);
    ts_fillFace(faceData, FS, FS, 255,   0, 255); cubeMap->imageNegZ.setValue(faceSize, 4, faceData);

    root->addChild(cubeMap);
    root->addChild(new SoTextureCoordinateEnvironment);

    SoSphere *sphere = new SoSphere;
    sphere->radius.setValue(1.2f);
    root->addChild(sphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 47. SceneTexture — quad with SoSceneTexture2 render-to-texture
// =========================================================================
static SoSeparator* ts_buildConeSubScene()
{
    SoSeparator *scene = new SoSeparator;

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    scene->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    scene->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.5f);
    scene->addChild(mat);

    SoCone *cone = new SoCone;
    cone->bottomRadius.setValue(0.6f);
    cone->height.setValue(1.2f);
    scene->addChild(cone);

    SbViewportRegion vp(128, 128);
    cam->viewAll(scene, vp);

    return scene;
}

SoSeparator* createSceneTexture(int width, int height)
{
    (void)width; (void)height;
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    SoSceneTexture2 *stex = new SoSceneTexture2;
    stex->size.setValue(SbVec2s(128, 128));
    stex->backgroundColor.setValue(0.0f, 0.0f, 0.2f, 1.0f);
    stex->type.setValue(SoSceneTexture2::RGBA8);
    stex->wrapS.setValue(SoSceneTexture2::CLAMP);
    stex->wrapT.setValue(SoSceneTexture2::CLAMP);
    stex->scene.setValue(ts_buildConeSubScene());
    root->addChild(stex);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(tc);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValue(4);
    root->addChild(fs);

    return root;
}

// =========================================================================
// 48. HUDOverlay — blue sphere with HUD status bar and side-menu buttons
// =========================================================================
static void ts_addMenuButton(SoHUDKit *hud, const char *label,
                              float y, float r, float g, float b)
{
    SoHUDButton *btn = new SoHUDButton;
    btn->position.setValue(10.0f, y);
    btn->size.setValue(100.0f, 28.0f);
    btn->string.setValue(label);
    btn->color.setValue(SbColor(r, g, b));
    btn->borderColor.setValue(SbColor(r * 0.65f, g * 0.65f, b * 0.65f));
    btn->fontSize.setValue(11.0f);
    hud->addWidget(btn);
}

SoSeparator* createHUDOverlay(int width, int height)
{
    (void)width; (void)height;
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 3.0f);
    cam->orientation.setValue(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f);
    cam->heightAngle.setValue(0.7854f);
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(100.0f);
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -1.0f, -0.8f);
    light->intensity.setValue(1.0f);
    root->addChild(light);

    {
        SoSeparator *sep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.15f, 0.35f, 0.85f);
        mat->specularColor.setValue(0.4f, 0.4f, 0.7f);
        mat->shininess.setValue(0.6f);
        sep->addChild(mat);
        SoSphere *sphere = new SoSphere;
        sphere->radius.setValue(0.5f);
        sep->addChild(sphere);
        root->addChild(sep);
    }

    SoHUDKit *hud = new SoHUDKit;

    SoHUDLabel *statusLabel = new SoHUDLabel;
    statusLabel->position.setValue(5.0f, 8.0f);
    statusLabel->string.set1Value(0, "Scene: Sphere  |  Radius: 0.50  |  Color: Blue");
    statusLabel->color.setValue(SbColor(0.9f, 0.85f, 0.3f));
    statusLabel->fontSize.setValue(12.0f);
    statusLabel->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(statusLabel);

    SoHUDLabel *headerLabel = new SoHUDLabel;
    headerLabel->position.setValue(10.0f, 568.0f);
    headerLabel->string.set1Value(0, "Controls");
    headerLabel->color.setValue(SbColor(0.9f, 0.9f, 0.9f));
    headerLabel->fontSize.setValue(12.0f);
    headerLabel->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(headerLabel);

    ts_addMenuButton(hud, "Larger",  530.0f, 0.55f, 0.88f, 0.55f);
    ts_addMenuButton(hud, "Smaller", 495.0f, 0.88f, 0.88f, 0.55f);
    ts_addMenuButton(hud, "Red",     460.0f, 1.00f, 0.35f, 0.35f);
    ts_addMenuButton(hud, "Blue",    425.0f, 0.35f, 0.55f, 1.00f);
    ts_addMenuButton(hud, "Green",   390.0f, 0.35f, 0.88f, 0.35f);

    root->addChild(hud);
    return root;
}

// =========================================================================
// 49. HUDNo3D — pure 2-D HUD scene without 3-D geometry
// =========================================================================
SoSeparator* createHUDNo3D(int width, int height)
{
    (void)width; (void)height;
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(100.0f);
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    SoHUDKit *hud = new SoHUDKit;

    SoHUDLabel *titleLabel = new SoHUDLabel;
    titleLabel->position.setValue(400.0f, 560.0f);
    titleLabel->string.set1Value(0, "HUD Display Test");
    titleLabel->color.setValue(SbColor(1.0f, 0.85f, 0.2f));
    titleLabel->fontSize.setValue(16.0f);
    titleLabel->justification.setValue(SoHUDLabel::CENTER);
    hud->addWidget(titleLabel);

    SoHUDLabel *subtitleLabel = new SoHUDLabel;
    subtitleLabel->position.setValue(400.0f, 532.0f);
    subtitleLabel->string.set1Value(0, "No 3D geometry present");
    subtitleLabel->color.setValue(SbColor(0.7f, 0.7f, 0.7f));
    subtitleLabel->fontSize.setValue(12.0f);
    subtitleLabel->justification.setValue(SoHUDLabel::CENTER);
    hud->addWidget(subtitleLabel);

    SoHUDLabel *leftHeader = new SoHUDLabel;
    leftHeader->position.setValue(10.0f, 490.0f);
    leftHeader->string.set1Value(0, "Main Menu");
    leftHeader->color.setValue(SbColor(0.9f, 0.9f, 0.9f));
    leftHeader->fontSize.setValue(13.0f);
    leftHeader->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(leftHeader);

    struct MenuEntry { const char *label; float r, g, b; };
    static const MenuEntry entries[] = {
        { "New Scene", 0.50f, 0.80f, 1.00f },
        { "Open...",   0.60f, 0.90f, 0.60f },
        { "Save",      0.90f, 0.85f, 0.40f },
        { "Settings",  0.80f, 0.60f, 0.90f },
        { "Exit",      1.00f, 0.40f, 0.40f },
    };
    const float btnX = 10.0f, btnW = 120.0f, btnH = 30.0f, btnStep = 40.0f;
    float btnY = 450.0f;
    for (int i = 0; i < 5; ++i) {
        SoHUDButton *btn = new SoHUDButton;
        btn->position.setValue(btnX, btnY);
        btn->size.setValue(btnW, btnH);
        btn->string.setValue(entries[i].label);
        btn->color.setValue(SbColor(entries[i].r, entries[i].g, entries[i].b));
        btn->borderColor.setValue(SbColor(
            entries[i].r * 0.65f, entries[i].g * 0.65f, entries[i].b * 0.65f));
        btn->fontSize.setValue(11.0f);
        hud->addWidget(btn);
        btnY -= btnStep;
    }

    SoHUDLabel *statusBar = new SoHUDLabel;
    statusBar->position.setValue(5.0f, 8.0f);
    statusBar->string.set1Value(0, "Ready  |  No scene loaded  |  HUD Only Mode");
    statusBar->color.setValue(SbColor(0.85f, 0.8f, 0.2f));
    statusBar->fontSize.setValue(12.0f);
    statusBar->justification.setValue(SoHUDLabel::LEFT);
    hud->addWidget(statusBar);

    root->addChild(hud);
    return root;
}

// =========================================================================
// 50. HUDInteraction — blue sphere with static HUD button layout
//     (no callbacks registered; visual layout only)
// =========================================================================
SoSeparator* createHUDInteraction(int width, int height)
{
    (void)width; (void)height;
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 3.5f);
    cam->orientation.setValue(SbVec3f(0.0f, 1.0f, 0.0f), 0.0f);
    cam->heightAngle.setValue(0.7854f);
    cam->nearDistance.setValue(0.1f);
    cam->farDistance.setValue(100.0f);
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -1.0f, -0.8f);
    root->addChild(light);

    {
        SoSeparator *sep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.15f, 0.35f, 0.85f);
        mat->specularColor.setValue(0.4f, 0.4f, 0.7f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);
        SoSphere *sphere = new SoSphere;
        sphere->radius.setValue(0.5f);
        sep->addChild(sphere);
        root->addChild(sep);
    }

    SoHUDKit *hud = new SoHUDKit;

    SoHUDLabel *statusLabel = new SoHUDLabel;
    statusLabel->position.setValue(5.0f, 8.0f);
    statusLabel->string.set1Value(0, "Scene: Sphere  |  Radius: 0.50  |  Color: Blue");
    statusLabel->color.setValue(SbColor(0.9f, 0.85f, 0.3f));
    statusLabel->fontSize.setValue(12.0f);
    hud->addWidget(statusLabel);

    SoHUDLabel *ctrlHeader = new SoHUDLabel;
    ctrlHeader->position.setValue(10.0f, 568.0f);
    ctrlHeader->string.set1Value(0, "Controls");
    ctrlHeader->color.setValue(SbColor(0.85f, 0.85f, 0.85f));
    ctrlHeader->fontSize.setValue(12.0f);
    hud->addWidget(ctrlHeader);

    struct BtnSpec { const char *label; float y; float r, g, b; };
    static const BtnSpec specs[] = {
        { "Larger",  530.0f, 0.55f, 0.88f, 0.55f },
        { "Smaller", 495.0f, 0.88f, 0.88f, 0.55f },
        { "Red",     460.0f, 1.00f, 0.35f, 0.35f },
        { "Blue",    425.0f, 0.35f, 0.55f, 1.00f },
        { "Green",   390.0f, 0.35f, 0.88f, 0.35f },
    };
    for (int i = 0; i < 5; ++i) {
        SoHUDButton *btn = new SoHUDButton;
        btn->position.setValue(10.0f, specs[i].y);
        btn->size.setValue(100.0f, 28.0f);
        btn->string.setValue(specs[i].label);
        btn->color.setValue(SbColor(specs[i].r, specs[i].g, specs[i].b));
        btn->borderColor.setValue(SbColor(
            specs[i].r * 0.65f, specs[i].g * 0.65f, specs[i].b * 0.65f));
        btn->fontSize.setValue(11.0f);
        hud->addWidget(btn);
    }

    root->addChild(hud);
    return root;
}

// =========================================================================
// 51. Text3Parts — SoText3 with FRONT, SIDES, BACK parts in one scene
// =========================================================================
SoSeparator* createText3Parts(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(2.0f, 2.0f, 15.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 80.0f;
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    // Use a small font so all three rows fit without overlapping.
    // Default font size is 10 units but the y-translations are only 3 units
    // apart, so a font size of 2 keeps each row within ~2 units tall and
    // prevents the red FRONT row from bleeding into the green ALL row in
    // the NanoRT panel (where overlapping coplanar triangles cause z-fighting).
    SoFont *fnt = new SoFont;
    fnt->size.setValue(2.0f);
    root->addChild(fnt);

    const int partsValues[3] = {
        SoText3::FRONT,
        SoText3::ALL,
        SoText3::BACK
    };
    float ys[3] = { 3.0f, 0.0f, -3.0f };
    float colors[3][3] = {
        { 0.9f, 0.3f, 0.3f },
        { 0.3f, 0.9f, 0.3f },
        { 0.3f, 0.3f, 0.9f }
    };

    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0.0f, ys[i], 0.0f);
        sep->addChild(tr);
        SoMaterial *m = new SoMaterial;
        m->diffuseColor.setValue(colors[i][0], colors[i][1], colors[i][2]);
        sep->addChild(m);
        SoText3 *t = new SoText3;
        t->string.setValue("AB");
        t->justification.setValue(SoText3::CENTER);
        t->parts.setValue(partsValues[i]);
        sep->addChild(t);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 52. DepthBuffer — near red cube + far blue sphere with SoDepthBuffer
// =========================================================================
SoSeparator* createDepthBuffer(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 30.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.7f, -0.6f);
    root->addChild(light);

    SoDepthBuffer *db = new SoDepthBuffer;
    db->test .setValue(TRUE);
    db->write.setValue(TRUE);
    db->function.setValue(SoDepthBuffer::LEQUAL);
    root->addChild(db);

    {
        SoSeparator *cubeSep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
        cubeSep->addChild(mat);
        SoCube *cube = new SoCube;
        cube->width .setValue(1.4f);
        cube->height.setValue(1.4f);
        cube->depth .setValue(1.4f);
        cubeSep->addChild(cube);
        root->addChild(cubeSep);
    }

    {
        SoSeparator *sphSep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(0.0f, 0.0f, -2.0f);
        sphSep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);
        sphSep->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius.setValue(0.8f);
        sphSep->addChild(sph);
        root->addChild(sphSep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 53. ProceduralShape — solid + wireframe truncated-cone side by side
// =========================================================================
static bool ts_proceduralShapeRegistered = false;

static void ts_coneBBox(const float *p, int n,
                         SbVec3f &mn, SbVec3f &mx, void *)
{
    float rb = (n > 0) ? p[0] : 1.0f;
    float rt = (n > 1) ? p[1] : 0.5f;
    float h  = (n > 2) ? p[2] : 2.0f;
    float r  = (rb > rt) ? rb : rt;
    mn.setValue(-r, -h*0.5f, -r);
    mx.setValue( r,  h*0.5f,  r);
}

static void ts_coneGeom(const float *p, int n,
                         SoProceduralTriangles *tris,
                         SoProceduralWireframe *wire,
                         void *)
{
    float rb    = (n > 0) ? p[0] : 1.0f;
    float rt    = (n > 1) ? p[1] : 0.5f;
    float h     = (n > 2) ? p[2] : 2.0f;
    int   sides = (n > 3) ? (int)p[3] : 16;
    if (sides < 3) sides = 3;

    const float yb = -h * 0.5f;
    const float yt =  h * 0.5f;

    if (tris) {
        tris->vertices.clear(); tris->normals.clear(); tris->indices.clear();
        for (int i = 0; i < sides; ++i) {
            float a = (float)(2.0 * M_PI * i / sides);
            tris->vertices.push_back(SbVec3f(rb * cosf(a), yb, rb * sinf(a)));
        }
        for (int i = 0; i < sides; ++i) {
            float a = (float)(2.0 * M_PI * i / sides);
            tris->vertices.push_back(SbVec3f(rt * cosf(a), yt, rt * sinf(a)));
        }
        float sl = (rb - rt) / h;
        for (int i = 0; i < sides * 2; ++i) {
            float a = (float)(2.0 * M_PI * (i % sides) / sides);
            SbVec3f nrm(cosf(a), sl, sinf(a)); nrm.normalize();
            tris->normals.push_back(nrm);
        }
        for (int i = 0; i < sides; ++i) {
            int i0 = i, i1 = (i+1) % sides, t0 = sides+i, t1 = sides+(i+1)%sides;
            tris->indices.push_back(i0); tris->indices.push_back(i1);
            tris->indices.push_back(t0);
            tris->indices.push_back(i1); tris->indices.push_back(t1);
            tris->indices.push_back(t0);
        }
    }
    if (wire) {
        wire->vertices.clear(); wire->segments.clear();
        for (int i = 0; i < sides; ++i) {
            float a = (float)(2.0 * M_PI * i / sides);
            wire->vertices.push_back(SbVec3f(rb * cosf(a), yb, rb * sinf(a)));
        }
        for (int i = 0; i < sides; ++i) {
            float a = (float)(2.0 * M_PI * i / sides);
            wire->vertices.push_back(SbVec3f(rt * cosf(a), yt, rt * sinf(a)));
        }
        for (int i = 0; i < sides; ++i) {
            wire->segments.push_back(i);
            wire->segments.push_back((i+1) % sides);
        }
        for (int i = 0; i < sides; ++i) {
            wire->segments.push_back(sides + i);
            wire->segments.push_back(sides + (i+1) % sides);
        }
        int step = (sides >= 12) ? sides / 8 : 1;
        for (int i = 0; i < sides; i += step) {
            wire->segments.push_back(i);
            wire->segments.push_back(sides + i);
        }
    }
}

static const char *kTSConeSchema = R"JSON({
  "type"  : "TruncatedCone_ts",
  "label" : "Truncated Cone (testlib)",
  "params": [
    { "name": "bottomRadius", "type": "float", "default": 1.0, "min": 0.001, "max": 100.0, "label": "Bottom Radius" },
    { "name": "topRadius",    "type": "float", "default": 0.5, "min": 0.0,   "max": 100.0, "label": "Top Radius" },
    { "name": "height",       "type": "float", "default": 2.0, "min": 0.001, "max": 100.0, "label": "Height" },
    { "name": "sides",        "type": "int",   "default": 16,  "min": 3,     "max": 128,   "label": "Sides" }
  ]
})JSON";

SoSeparator* createProceduralShape(int width, int height)
{
    if (!ts_proceduralShapeRegistered) {
        SoProceduralShape::registerShapeType("TruncatedCone_ts",
                                             kTSConeSchema,
                                             ts_coneBBox,
                                             ts_coneGeom);
        ts_proceduralShapeRegistered = true;
    }

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(light);

    // Left: solid
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-2.0f, 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.5f, 0.9f);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess.setValue(0.6f);
        sep->addChild(mat);
        SoProceduralShape *shape = new SoProceduralShape;
        shape->setShapeType("TruncatedCone_ts");
        sep->addChild(shape);
        root->addChild(sep);
    }

    // Right: wireframe
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(2.0f, 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.6f, 0.1f);
        sep->addChild(mat);
        SoDrawStyle *ds = new SoDrawStyle;
        ds->style.setValue(SoDrawStyle::LINES);
        sep->addChild(ds);
        SoProceduralShape *shape = new SoProceduralShape;
        shape->setShapeType("TruncatedCone_ts");
        float p[] = { 1.2f, 0.0f, 3.0f, 8.0f };
        shape->params.setValues(0, 4, p);
        sep->addChild(shape);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.2f);

    return root;
}

// =========================================================================
// 54. GLBigImage — textured quad with SoTextureScalePolicy::FRACTURE
// =========================================================================
static unsigned char* ts_makeCheckerboard(int w, int h)
{
    unsigned char *data = new unsigned char[w * h * 4];
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool red = (((x / 64) + (y / 64)) & 1) == 0;
            unsigned char *p = data + (y * w + x) * 4;
            p[0] = 255; p[1] = red ? 0 : 255; p[2] = red ? 0 : 255; p[3] = 255;
        }
    }
    return data;
}

SoSeparator* createGLBigImage(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoTextureScalePolicy *tsp = new SoTextureScalePolicy;
    tsp->policy = SoTextureScalePolicy::FRACTURE;
    root->addChild(tsp);

    SoTexture2 *tex = new SoTexture2;
    {
        const int TEX_W = 512, TEX_H = 512;
        unsigned char *data = ts_makeCheckerboard(TEX_W, TEX_H);
        tex->image.setValue(SbVec2s((short)TEX_W, (short)TEX_H), 4, data);
        delete[] data;
    }
    root->addChild(tex);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(tc);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.set1Value(0, 4);
    root->addChild(fs);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 55. ImageDeep — SoImage node with a 48×48 RGBA checkerboard
// =========================================================================
SoSeparator* createImageDeep(int width, int height)
{
    (void)width; (void)height;
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    SoImage *img = new SoImage;
    {
        const int iw = 48, ih = 48, nc = 4;
        std::vector<unsigned char> data(iw * ih * nc);
        for (int y = 0; y < ih; ++y) {
            for (int x = 0; x < iw; ++x) {
                bool checker = ((x / 4 + y / 4) % 2 == 0);
                unsigned char *p = data.data() + (y * iw + x) * nc;
                p[0] = checker ? 220 : 50;
                p[1] = 100;
                p[2] = checker ? 50 : 200;
                p[3] = 200;
            }
        }
        img->image.setValue(SbVec2s(iw, ih), nc, data.data());
    }
    root->addChild(img);
    return root;
}

// =========================================================================
// 56. ShaderProgram — sphere with a basic GLSL vertex + fragment program
// =========================================================================
SoSeparator* createShaderProgram(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    static const char *vert_src =
        "void main() {\n"
        "  gl_Position = ftransform();\n"
        "  gl_FrontColor = gl_Color;\n"
        "}\n";

    static const char *frag_src =
        "uniform vec3 uColor;\n"
        "void main() {\n"
        "  gl_FragColor = vec4(uColor, 1.0);\n"
        "}\n";

    SoShaderProgram *prog = new SoShaderProgram;

    SoVertexShader *vs = new SoVertexShader;
    vs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    vs->sourceProgram.setValue(vert_src);

    SoFragmentShader *fs = new SoFragmentShader;
    fs->sourceType.setValue(SoShaderObject::GLSL_PROGRAM);
    fs->sourceProgram.setValue(frag_src);

    SoShaderParameter3f *p3f = new SoShaderParameter3f;
    p3f->name.setValue("uColor");
    p3f->value.setValue(SbVec3f(0.2f, 0.8f, 0.4f));
    fs->parameter.addNode(p3f);

    prog->shaderObject.addNode(vs);
    prog->shaderObject.addNode(fs);
    root->addChild(prog);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.7f, 0.7f, 0.7f);
    root->addChild(mat);

    SoSphere *sphere = new SoSphere;
    sphere->radius.setValue(1.0f);
    root->addChild(sphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 57. SoRenderManager — camera + light + SoCube (scene only)
// =========================================================================
SoSeparator* createSoRenderManager(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(light);

    root->addChild(new SoCube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 58. GLFeatures — textured sphere (exercises SoGLImage paths)
// =========================================================================
SoSeparator* createGLFeatures(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.7f, -0.5f);
    root->addChild(light);

    const int TEX_SIZE = 64;
    SoTexture2 *tex = new SoTexture2;
    {
        unsigned char data[TEX_SIZE * TEX_SIZE * 3];
        for (int y = 0; y < TEX_SIZE; ++y) {
            for (int x = 0; x < TEX_SIZE; ++x) {
                bool red = (((x / 8) + (y / 8)) & 1) == 0;
                unsigned char *p = data + (y * TEX_SIZE + x) * 3;
                p[0] = red ? 220 : 240;
                p[1] = red ?   0 : 240;
                p[2] = red ?   0 : 240;
            }
        }
        tex->image.setValue(SbVec2s(TEX_SIZE, TEX_SIZE), 3, data);
    }
    tex->wrapS = SoTexture2::REPEAT;
    tex->wrapT = SoTexture2::REPEAT;
    root->addChild(tex);

    SoSphere *sphere = new SoSphere;
    sphere->radius.setValue(1.0f);
    root->addChild(sphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 59. QuadMeshDeep — 4×4 SoQuadMesh with PER_FACE material binding
// =========================================================================
SoSeparator* createQuadMeshDeep(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);

    root->addChild(new SoDirectionalLight);

    const int GRID_ROWS = 4, GRID_COLS = 4;
    const float CELL = 0.6f;

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_FACE);
    root->addChild(mb);

    const int NFACES = (GRID_ROWS - 1) * (GRID_COLS - 1);
    SoMaterial *mat = new SoMaterial;
    for (int i = 0; i < NFACES; ++i) {
        float r = (i % 3) == 0 ? 0.8f : 0.2f;
        float g = (i % 3) == 1 ? 0.8f : 0.2f;
        float b = (i % 3) == 2 ? 0.8f : 0.2f;
        mat->diffuseColor.set1Value(i, SbColor(r, g, b));
    }
    root->addChild(mat);

    SoNormal *norms = new SoNormal;
    for (int i = 0; i < GRID_ROWS * GRID_COLS; ++i)
        norms->vector.set1Value(i, SbVec3f(0.0f, 0.0f, 1.0f));
    root->addChild(norms);
    SoNormalBinding *nb = new SoNormalBinding;
    nb->value.setValue(SoNormalBinding::PER_VERTEX);
    root->addChild(nb);

    SoCoordinate3 *coords = new SoCoordinate3;
    int idx = 0;
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            float x = (c - (GRID_COLS - 1) * 0.5f) * CELL;
            float y = (r - (GRID_ROWS - 1) * 0.5f) * CELL;
            coords->point.set1Value(idx++, SbVec3f(x, y, 0.0f));
        }
    }
    root->addChild(coords);

    SoQuadMesh *qm = new SoQuadMesh;
    qm->verticesPerRow.setValue(GRID_COLS);
    qm->verticesPerColumn.setValue(GRID_ROWS);
    root->addChild(qm);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 60. Offscreen — red sphere (SoOffscreenRenderer API coverage scene)
// =========================================================================
SoSeparator* createOffscreen(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.5f, -0.8f);
    root->addChild(light);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.1f, 0.1f);
    mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
    mat->shininess.setValue(0.4f);
    root->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius.setValue(1.0f);
    root->addChild(sph);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 61. BBoxAction — three coloured spheres spread along X axis
// =========================================================================
SoSeparator* createBBoxAction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    float xs[3] = { -3.0f, 0.0f, 3.0f };
    float colors[3][3] = {
        { 0.8f, 0.3f, 0.3f },
        { 0.3f, 0.8f, 0.3f },
        { 0.3f, 0.3f, 0.8f }
    };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i][0], colors[i][1], colors[i][2]);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 62. SearchAction — hierarchical scene with named nodes of multiple types
// =========================================================================
SoSeparator* createSearchAction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();
    root->setName("root");

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // group1: red sphere + green cube
    SoSeparator *g1 = new SoSeparator;
    g1->setName("group1");

    SoMaterial *mat1 = new SoMaterial;
    mat1->setName("mat1");
    mat1->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
    g1->addChild(mat1);

    SoTranslation *tr1 = new SoTranslation;
    tr1->translation.setValue(-2.0f, 0.5f, 0.0f);
    g1->addChild(tr1);

    SoSphere *sph1 = new SoSphere;
    sph1->setName("sphere1");
    g1->addChild(sph1);

    SoTranslation *tr2 = new SoTranslation;
    tr2->translation.setValue(4.0f, 0.0f, 0.0f);
    g1->addChild(tr2);

    SoCube *cube = new SoCube;
    cube->setName("cube1");
    g1->addChild(cube);

    root->addChild(g1);

    // group2: blue sphere + orange cone
    SoSeparator *g2 = new SoSeparator;
    g2->setName("group2");

    SoMaterial *mat2 = new SoMaterial;
    mat2->setName("mat2");
    mat2->diffuseColor.setValue(0.2f, 0.3f, 0.9f);
    g2->addChild(mat2);

    SoTranslation *tr3 = new SoTranslation;
    tr3->translation.setValue(-2.0f, -2.5f, 0.0f);
    g2->addChild(tr3);

    SoSphere *sph2 = new SoSphere;
    sph2->setName("sphere2");
    g2->addChild(sph2);

    SoMaterial *mat3 = new SoMaterial;
    mat3->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    g2->addChild(mat3);

    SoTranslation *tr4 = new SoTranslation;
    tr4->translation.setValue(4.5f, 0.0f, 0.0f);
    g2->addChild(tr4);

    SoCone *cone = new SoCone;
    cone->setName("cone1");
    g2->addChild(cone);

    root->addChild(g2);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 63. CallbackAction — sphere + cube + cone for triangle callback coverage
// =========================================================================
SoSeparator* createCallbackAction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    float xs[3] = { -2.5f, 0.0f, 2.5f };
    float colrs[3][3] = {
        { 0.7f, 0.3f, 0.3f },
        { 0.3f, 0.7f, 0.3f },
        { 0.3f, 0.3f, 0.7f }
    };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(colrs[i][0], colrs[i][1], colrs[i][2]);
        sep->addChild(mat);
        SoNode *shape = nullptr;
        if (i == 0) shape = new SoSphere;
        else if (i == 1) shape = new SoCube;
        else shape = new SoCone;
        sep->addChild(shape);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 64. CallbackActionDeep — all primitive shape types in a 2×3 grid
// =========================================================================
SoSeparator* createCallbackActionDeep(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    const float DX = 2.5f, DY = 2.5f;
    struct ShapeItem { float x, y; float r, g, b; int type; };
    // type: 0=sphere, 1=cone, 2=cylinder, 3=cube
    static const ShapeItem items[] = {
        { -DX,  DY, 0.8f, 0.3f, 0.3f, 0 },
        {  0.f, DY, 0.3f, 0.8f, 0.3f, 1 },
        {  DX,  DY, 0.3f, 0.3f, 0.8f, 2 },
        { -DX, -DY, 0.8f, 0.7f, 0.2f, 3 },
        {  0.f, -DY, 0.6f, 0.3f, 0.8f, 0 },
        {  DX, -DY, 0.2f, 0.7f, 0.7f, 1 },
    };
    for (int i = 0; i < 6; ++i) {
        const ShapeItem &item = items[i];
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(item.x, item.y, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(item.r, item.g, item.b);
        sep->addChild(mat);
        SoNode *shape = nullptr;
        switch (item.type) {
            case 0: shape = new SoSphere;   break;
            case 1: shape = new SoCone;     break;
            case 2: shape = new SoCylinder; break;
            default: shape = new SoCube;    break;
        }
        sep->addChild(shape);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 65. CallbackNode — three SoCallback nodes interleaved with two shapes
// =========================================================================
SoSeparator* createCallbackNode(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // SoCallback nodes are traversal hooks; they have no visual effect in a
    // static render, but they are present in the graph to exercise the API
    root->addChild(new SoCallback);

    SoSeparator *sepA = new SoSeparator;
    SoTranslation *trA = new SoTranslation;
    trA->translation.setValue(-2.0f, 0.0f, 0.0f);
    sepA->addChild(trA);
    SoMaterial *matA = new SoMaterial;
    matA->diffuseColor.setValue(0.5f, 0.7f, 0.3f);
    sepA->addChild(matA);
    sepA->addChild(new SoCallback);
    sepA->addChild(new SoSphere);
    root->addChild(sepA);

    root->addChild(new SoCallback);

    SoSeparator *sepB = new SoSeparator;
    SoTranslation *trB = new SoTranslation;
    trB->translation.setValue(2.0f, 0.0f, 0.0f);
    sepB->addChild(trB);
    SoMaterial *matB = new SoMaterial;
    matB->diffuseColor.setValue(0.8f, 0.3f, 0.5f);
    sepB->addChild(matB);
    sepB->addChild(new SoCube);
    root->addChild(sepB);

    return root;
}

// =========================================================================
// 66. EventPropagation — SoEventCallback nodes in nested separators
// =========================================================================
SoSeparator* createEventPropagation(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Root-level event callback
    root->addChild(new SoEventCallback);

    // Inner separator: event callback + sphere
    SoSeparator *inner = new SoSeparator;
    inner->addChild(new SoEventCallback);
    SoTranslation *tr1 = new SoTranslation;
    tr1->translation.setValue(-2.0f, 0.0f, 0.0f);
    inner->addChild(tr1);
    SoMaterial *mat1 = new SoMaterial;
    mat1->diffuseColor.setValue(0.3f, 0.7f, 0.9f);
    inner->addChild(mat1);
    inner->addChild(new SoSphere);
    root->addChild(inner);

    // Outer separator: event callback + cube
    SoSeparator *outer = new SoSeparator;
    outer->addChild(new SoEventCallback);
    SoTranslation *tr2 = new SoTranslation;
    tr2->translation.setValue(2.0f, 0.0f, 0.0f);
    outer->addChild(tr2);
    SoMaterial *mat2 = new SoMaterial;
    mat2->diffuseColor.setValue(0.9f, 0.5f, 0.2f);
    outer->addChild(mat2);
    outer->addChild(new SoCube);
    root->addChild(outer);

    return root;
}

// =========================================================================
// 67. PathOperations — sphere (left) + cube (right) for SoPath tests
// =========================================================================
SoSeparator* createPathOperations(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoSeparator *sep1 = new SoSeparator;
    SoTranslation *tr1 = new SoTranslation;
    tr1->translation.setValue(-2.0f, 0.0f, 0.0f);
    sep1->addChild(tr1);
    SoMaterial *mat1 = new SoMaterial;
    mat1->diffuseColor.setValue(0.8f, 0.3f, 0.3f);
    sep1->addChild(mat1);
    sep1->addChild(new SoSphere);
    root->addChild(sep1);

    SoSeparator *sep2 = new SoSeparator;
    SoTranslation *tr2 = new SoTranslation;
    tr2->translation.setValue(2.0f, 0.0f, 0.0f);
    sep2->addChild(tr2);
    SoMaterial *mat2 = new SoMaterial;
    mat2->diffuseColor.setValue(0.3f, 0.3f, 0.8f);
    sep2->addChild(mat2);
    sep2->addChild(new SoCube);
    root->addChild(sep2);

    return root;
}

// =========================================================================
// 68. WriteReadAction — red sphere + blue cube (SoWriteAction input scene)
// =========================================================================
SoSeparator* createWriteReadAction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -0.5f, -0.8f);
    root->addChild(light);

    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(-1.2f, 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.3f, 0.2f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(1.2f, 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.2f, 0.5f, 0.9f);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 69. FieldConnections — sphere driven by SoComposeVec3f engine
// =========================================================================
SoSeparator* createFieldConnections(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoComposeVec3f *comp = new SoComposeVec3f;
    comp->ref();
    comp->x.setValue(0.9f);
    comp->y.setValue(0.4f);
    comp->z.setValue(0.1f);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.connectFrom(&comp->vector);
    root->addChild(mat);
    root->addChild(new SoSphere);

    // comp is kept alive by the field connection; release our explicit ref
    comp->unref();

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 70. SensorsRendering — static "good frame" sphere for sensor integration
// =========================================================================
SoSeparator* createSensorsRendering(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    // Colour represents the final state of a 5-frame sensor-driven animation
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.8f, 0.3f, 0.5f);
    mat->specularColor.setValue(0.4f, 0.4f, 0.4f);
    mat->shininess.setValue(0.3f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 71. RenderManagerFull — camera + light + cube for SoRenderManager tests
// =========================================================================
SoSeparator* createRenderManagerFull(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.4f, 0.7f, 0.9f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 72. SOGLBindings — 9-point grid with PER_VERTEX material (m1n0t0 variant)
// =========================================================================
SoSeparator* createSOGLBindings(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoDrawStyle *ds = new SoDrawStyle;
    ds->pointSize.setValue(8.0f);
    root->addChild(ds);

    SoMaterialBinding *mb = new SoMaterialBinding;
    mb->value.setValue(SoMaterialBinding::PER_VERTEX);
    root->addChild(mb);

    SoMaterial *mat = new SoMaterial;
    for (int i = 0; i < 9; ++i) {
        float r = (i % 3 == 0) ? 0.9f : 0.2f;
        float g = (i % 3 == 1) ? 0.9f : 0.2f;
        float b = (i % 3 == 2) ? 0.9f : 0.2f;
        mat->diffuseColor.set1Value(i, SbColor(r, g, b));
    }
    root->addChild(mat);

    static const SbVec3f pts[9] = {
        SbVec3f(-0.6f,  0.6f, 0.f), SbVec3f(0.f,  0.6f, 0.f), SbVec3f(0.6f,  0.6f, 0.f),
        SbVec3f(-0.6f,  0.0f, 0.f), SbVec3f(0.f,  0.0f, 0.f), SbVec3f(0.6f,  0.0f, 0.f),
        SbVec3f(-0.6f, -0.6f, 0.f), SbVec3f(0.f, -0.6f, 0.f), SbVec3f(0.6f, -0.6f, 0.f),
    };
    SoCoordinate3 *c3 = new SoCoordinate3;
    c3->point.setValues(0, 9, pts);
    root->addChild(c3);

    SoPointSet *ps = new SoPointSet;
    ps->numPoints.setValue(9);
    root->addChild(ps);

    (void)width; (void)height;
    return root;
}

// =========================================================================
// 73. GLRenderActionModes — two semi-transparent overlapping objects
// =========================================================================
SoSeparator* createGLRenderActionModes(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(-0.5f, 0.0f, -1.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.9f, 0.1f, 0.1f);
        mat->transparency.setValue(0.5f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0.5f, 0.0f, 1.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.1f, 0.1f, 0.9f);
        mat->transparency.setValue(0.3f);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 74. GLRenderDeep — three semi-transparent spheres side by side
// =========================================================================
SoSeparator* createGLRenderDeep(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    float xs[3] = { -1.5f, 0.0f, 1.5f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f - i*0.3f, 0.3f + i*0.2f, 0.5f);
        mat->transparency.setValue(0.5f);
        sep->addChild(mat);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 75. OffscreenAdvanced — camera + light + cube for SoOffscreenRenderer tests
// =========================================================================
SoSeparator* createOffscreenAdvanced(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 0.9f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 76. ViewVolumeOps — perspective camera + purple sphere
// =========================================================================
SoSeparator* createViewVolumeOps(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.3f, 0.8f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 77. LODPicking — three SoLOD nodes (sphere/cube/cone levels) side by side
// =========================================================================
SoSeparator* createLODPicking(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    float xs[3] = { -3.0f, 0.0f, 3.0f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);

        SoLOD *lod = new SoLOD;
        lod->range.set1Value(0,  5.0f);
        lod->range.set1Value(1, 15.0f);

        SoSeparator *nearSep = new SoSeparator;
        SoMaterial *m0 = new SoMaterial;
        m0->diffuseColor.setValue(0.2f, 0.8f, 0.2f);
        nearSep->addChild(m0);
        nearSep->addChild(new SoSphere);
        lod->addChild(nearSep);

        SoSeparator *midSep = new SoSeparator;
        SoMaterial *m1 = new SoMaterial;
        m1->diffuseColor.setValue(0.2f, 0.2f, 0.8f);
        midSep->addChild(m1);
        midSep->addChild(new SoCube);
        lod->addChild(midSep);

        SoSeparator *farSep = new SoSeparator;
        SoMaterial *m2 = new SoMaterial;
        m2->diffuseColor.setValue(0.8f, 0.2f, 0.2f);
        farSep->addChild(m2);
        farSep->addChild(new SoCone);
        lod->addChild(farSep);

        sep->addChild(lod);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 79. createCameraInteraction — sphere, cube, cone with perspective camera
// =========================================================================
SoSeparator* createCameraInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);
    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.5f, -1.0f);
    root->addChild(lt);

    // Sphere (left)
    SoSeparator *s = new SoSeparator;
    SoTransform *xfs = new SoTransform;
    xfs->translation.setValue(-1.5f, 0.0f, 0.0f);
    s->addChild(xfs);
    SoMaterial *ms = new SoMaterial;
    ms->diffuseColor.setValue(0.8f, 0.3f, 0.3f);
    s->addChild(ms);
    s->addChild(new SoSphere);
    root->addChild(s);

    // Cube (center)
    SoSeparator *c = new SoSeparator;
    SoMaterial *mc = new SoMaterial;
    mc->diffuseColor.setValue(0.3f, 0.7f, 0.3f);
    c->addChild(mc);
    c->addChild(new SoCube);
    root->addChild(c);

    // Cone (right)
    SoSeparator *co = new SoSeparator;
    SoTransform *xfco = new SoTransform;
    xfco->translation.setValue(1.5f, 0.0f, 0.0f);
    co->addChild(xfco);
    SoMaterial *mco = new SoMaterial;
    mco->diffuseColor.setValue(0.3f, 0.3f, 0.8f);
    co->addChild(mco);
    co->addChild(new SoCone);
    root->addChild(co);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 80. createSceneInteraction — 3-object dynamic scene (sphere, cube, cylinder)
// =========================================================================
SoSeparator* createSceneInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(10.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    float xs[3]  = { -3.0f, 0.0f, 3.0f };
    float rs[3]  = { 0.3f, 0.6f, 0.3f };
    float gs[3]  = { 0.6f, 0.5f, 0.7f };
    float bs[3]  = { 0.8f, 0.7f, 0.5f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf  = new SoTransform;
        xf->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(xf);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(rs[i], gs[i], bs[i]);
        sep->addChild(mat);
        if      (i == 0) sep->addChild(new SoSphere);
        else if (i == 1) sep->addChild(new SoCube);
        else             sep->addChild(new SoCylinder);
        root->addChild(sep);
    }
    return root;
}

// =========================================================================
// 81. createEngineInteraction — sphere positioned by SoComposeVec3f engine
// =========================================================================
SoSeparator* createEngineInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 0.3f);
    root->addChild(mat);

    SoTransform *xf = new SoTransform;
    root->addChild(xf);
    root->addChild(new SoSphere);

    // Engine drives translation: position = (1.5, 0.5, 0)
    SoComposeVec3f *compose = new SoComposeVec3f;
    compose->ref();
    compose->x.setValue(1.5f);
    compose->y.setValue(0.5f);
    compose->z.setValue(0.0f);
    xf->translation.connectFrom(&compose->vector);
    compose->unref();

    return root;
}

// =========================================================================
// 82. createEngineConverter — sphere with material driven via field conversion
// =========================================================================
SoSeparator* createEngineConverter(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    root->addChild(mat);

    // SoComposeVec3f output (SFVec3f) connected to SoMFColor — exercises
    // SoConvertAll automatic type-conversion engine.
    SoComposeVec3f *compose = new SoComposeVec3f;
    compose->ref();
    compose->x.setValue(0.9f);
    compose->y.setValue(0.3f);
    compose->z.setValue(0.1f);
    mat->diffuseColor.connectFrom(&compose->vector);
    compose->unref();

    root->addChild(new SoSphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 83. createEventCallbackInteraction — switch + sphere behind event callback
// =========================================================================
SoSeparator* createEventCallbackInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    root->addChild(new SoEventCallback);  // event callback node (no-op here)

    SoSwitch *sw = new SoSwitch;
    sw->whichChild.setValue(SO_SWITCH_ALL);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.8f, 0.3f);
    sw->addChild(mat);
    SoSphere *sph = new SoSphere;
    sph->radius = 0.8f;
    sw->addChild(sph);
    root->addChild(sw);

    return root;
}

// =========================================================================
// 84. createPickInteraction — blue sphere for SoRayPickAction testing
// =========================================================================
SoSeparator* createPickInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(lt);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.4f, 0.6f, 1.0f);
    root->addChild(mat);

    SoSphere *sph = new SoSphere;
    sph->radius = 0.8f;
    root->addChild(sph);

    return root;
}

// =========================================================================
// 85. createPickFilter — SoSelection + sphere + cube for pick-filter tests
// =========================================================================
SoSeparator* createPickFilter(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoSelection *sel = new SoSelection;
    sel->policy.setValue(SoSelection::SHIFT);
    root->addChild(sel);

    // Sphere (left)
    SoSeparator *sphSep = new SoSeparator;
    SoTransform *xf1 = new SoTransform;
    xf1->translation.setValue(-2.0f, 0.0f, 0.0f);
    sphSep->addChild(xf1);
    SoMaterial *m1 = new SoMaterial;
    m1->diffuseColor.setValue(0.8f, 0.3f, 0.3f);
    sphSep->addChild(m1);
    sphSep->addChild(new SoSphere);
    sel->addChild(sphSep);

    // Cube (right)
    SoSeparator *cubeSep = new SoSeparator;
    SoTransform *xf2 = new SoTransform;
    xf2->translation.setValue(2.0f, 0.0f, 0.0f);
    cubeSep->addChild(xf2);
    SoMaterial *m2 = new SoMaterial;
    m2->diffuseColor.setValue(0.3f, 0.3f, 0.8f);
    cubeSep->addChild(m2);
    cubeSep->addChild(new SoCube);
    sel->addChild(cubeSep);

    return root;
}

// =========================================================================
// 86. createSelectionInteraction — SoSelection (SHIFT) with sphere+cube+cone
// =========================================================================
SoSeparator* createSelectionInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    cam->height.setValue(8.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoSelection *sel = new SoSelection;
    sel->policy.setValue(SoSelection::SHIFT);
    root->addChild(sel);

    float xs[3]   = { -2.5f, 0.0f, 2.5f };
    float rs[3]   = { 0.7f, 0.3f, 0.3f };
    float gs[3]   = { 0.3f, 0.7f, 0.3f };
    float bs[3]   = { 0.3f, 0.3f, 0.7f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(xf);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(rs[i], gs[i], bs[i]);
        sep->addChild(mat);
        if      (i == 0) sep->addChild(new SoSphere);
        else if (i == 1) sep->addChild(new SoCube);
        else             sep->addChild(new SoCone);
        sel->addChild(sep);
    }
    return root;
}

// =========================================================================
// 87. createSensorInteraction — gray sphere for sensor-driven testing
// =========================================================================
SoSeparator* createSensorInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(4.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.5f, 0.5f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    return root;
}

// =========================================================================
// 88. createNodeKitInteraction — SoShapeKit with sphere
// =========================================================================
SoSeparator* createNodeKitInteraction(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 5.0f);
    cam->height.setValue(6.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoShapeKit *kit = new SoShapeKit;
    kit->setPart("shape", new SoSphere);
    SoMaterial *mat = static_cast<SoMaterial *>(kit->getPart("material", TRUE));
    if (mat) mat->diffuseColor.setValue(0.8f, 0.4f, 0.2f);
    root->addChild(kit);

    return root;
}

// =========================================================================
// 89. createManipSequences — sphere with SoCenterballManip attached
// =========================================================================
SoSeparator* createManipSequences(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-0.5f, -1.0f, -0.5f);
    root->addChild(lt);

    SoSeparator *shapeSep = new SoSeparator;
    shapeSep->addChild(new SoCenterballManip);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.4f, 0.8f);
    shapeSep->addChild(mat);
    shapeSep->addChild(new SoSphere);
    root->addChild(shapeSep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 90. createLightManips — 3 spheres + floor lit by SoDirectionalLightManip
// =========================================================================
SoSeparator* createLightManips(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 3.0f, 8.0f);
    cam->pointAt(SbVec3f(0.0f, 0.0f, 0.0f), SbVec3f(0.0f, 1.0f, 0.0f));
    root->addChild(cam);

    SoDirectionalLightManip *manip = new SoDirectionalLightManip;
    manip->direction.setValue(-0.5f, -1.0f, -0.5f);
    manip->color.setValue(1.0f, 1.0f, 0.9f);
    root->addChild(manip);

    // Three spheres
    float px[3] = { -2.0f, 0.0f, 2.0f };
    float rc[3] = { 0.8f, 0.3f, 0.3f };
    float gc[3] = { 0.3f, 0.8f, 0.3f };
    float bc[3] = { 0.3f, 0.3f, 0.8f };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTransform *xf = new SoTransform;
        xf->translation.setValue(px[i], 0.0f, 0.0f);
        sep->addChild(xf);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(rc[i], gc[i], bc[i]);
        mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);
        SoSphere *sph = new SoSphere;
        sph->radius = 0.7f;
        sep->addChild(sph);
        root->addChild(sep);
    }

    // Floor
    SoSeparator *floor = new SoSeparator;
    SoTransform *fxf = new SoTransform;
    fxf->translation.setValue(0.0f, -1.2f, 0.0f);
    floor->addChild(fxf);
    SoMaterial *fm = new SoMaterial;
    fm->diffuseColor.setValue(0.5f, 0.5f, 0.5f);
    floor->addChild(fm);
    SoCube *floorBox = new SoCube;
    floorBox->width  = 8.0f;
    floorBox->height = 0.1f;
    floorBox->depth  = 4.0f;
    floor->addChild(floorBox);
    root->addChild(floor);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 91. createSimpleDraggers — cube + SoTranslate1Dragger
// =========================================================================
SoSeparator* createSimpleDraggers(int width, int height)
{
    // Uses buildDraggerTestScene so the viewer and the render_simple_draggers
    // interaction test both start from the same scene setup.
    SoSeparator *root = buildDraggerTestScene(new SoTranslate1Dragger, width, height);
    root->ref();
    return root;
}

// =========================================================================
// 92-93. ARB8 shared geometry callbacks + schemas
// =========================================================================

static const int kArb8SceneFaces[6][4] = {
    {0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}
};
static const int kArb8SceneEdges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
};
static const float kArb8SceneDefault[24] = {
    -1,-1,-1, 1,-1,-1, 1,-1, 1,-1,-1, 1,
    -1, 1,-1, 1, 1,-1, 1, 1, 1,-1, 1, 1
};

static void arb8SceneBBox(const float* p, int n,
                           SbVec3f& mn, SbVec3f& mx, void*)
{
    if (n < 24) { mn.setValue(-1,-1,-1); mx.setValue(1,1,1); return; }
    float ax=p[0],ay=p[1],az=p[2],bx=p[0],by=p[1],bz=p[2];
    for (int i = 1; i < 8; ++i) {
        if (p[3*i  ]<ax) ax=p[3*i  ]; if (p[3*i+1]<ay) ay=p[3*i+1]; if (p[3*i+2]<az) az=p[3*i+2];
        if (p[3*i  ]>bx) bx=p[3*i  ]; if (p[3*i+1]>by) by=p[3*i+1]; if (p[3*i+2]>bz) bz=p[3*i+2];
    }
    mn.setValue(ax,ay,az); mx.setValue(bx,by,bz);
}

static void arb8SceneGeom(const float* pp, int n,
                           SoProceduralTriangles* tris,
                           SoProceduralWireframe* wire, void*)
{
    const float* p = (n >= 24) ? pp : kArb8SceneDefault;
    if (tris) {
        tris->vertices.clear(); tris->normals.clear(); tris->indices.clear();
        for (int f = 0; f < 6; ++f) {
            const int* fv = kArb8SceneFaces[f];
            SbVec3f v0(p[3*fv[0]],p[3*fv[0]+1],p[3*fv[0]+2]);
            SbVec3f v1(p[3*fv[1]],p[3*fv[1]+1],p[3*fv[1]+2]);
            SbVec3f v2(p[3*fv[2]],p[3*fv[2]+1],p[3*fv[2]+2]);
            SbVec3f v3(p[3*fv[3]],p[3*fv[3]+1],p[3*fv[3]+2]);
            // Use (v2-v0)×(v1-v0) cross-product order so that outward-facing
            // normals are produced for the face winding stored in kArb8SceneFaces.
            SbVec3f nm = (v2-v0).cross(v1-v0); nm.normalize();
            int b = (int)tris->vertices.size();
            tris->vertices.insert(tris->vertices.end(),{v0,v1,v2,v3});
            tris->normals .insert(tris->normals .end(),{nm,nm,nm,nm});
            tris->indices .insert(tris->indices .end(),{b,b+1,b+2,b,b+2,b+3});
        }
    }
    if (wire) {
        wire->vertices.clear(); wire->segments.clear();
        for (int i = 0; i < 8; ++i)
            wire->vertices.push_back(SbVec3f(p[3*i],p[3*i+1],p[3*i+2]));
        for (int e = 0; e < 12; ++e) {
            wire->segments.push_back(kArb8SceneEdges[e][0]);
            wire->segments.push_back(kArb8SceneEdges[e][1]);
        }
    }
}

// Full ARB8 schema with vertex/edge/face handles for createArb8Draggers
static const char* kArb8DaggersSchema = R"({
  "type": "ARB8_viewer",
  "label": "ARB8 Dragger Demo",
  "params": [
    {"name":"v0x","type":"float","default":-1.0},{"name":"v0y","type":"float","default":-1.0},{"name":"v0z","type":"float","default":-1.0},
    {"name":"v1x","type":"float","default": 1.0},{"name":"v1y","type":"float","default":-1.0},{"name":"v1z","type":"float","default":-1.0},
    {"name":"v2x","type":"float","default": 1.0},{"name":"v2y","type":"float","default":-1.0},{"name":"v2z","type":"float","default": 1.0},
    {"name":"v3x","type":"float","default":-1.0},{"name":"v3y","type":"float","default":-1.0},{"name":"v3z","type":"float","default": 1.0},
    {"name":"v4x","type":"float","default":-1.0},{"name":"v4y","type":"float","default": 1.0},{"name":"v4z","type":"float","default":-1.0},
    {"name":"v5x","type":"float","default": 1.0},{"name":"v5y","type":"float","default": 1.0},{"name":"v5z","type":"float","default":-1.0},
    {"name":"v6x","type":"float","default": 1.0},{"name":"v6y","type":"float","default": 1.0},{"name":"v6z","type":"float","default": 1.0},
    {"name":"v7x","type":"float","default":-1.0},{"name":"v7y","type":"float","default": 1.0},{"name":"v7z","type":"float","default": 1.0}
  ],
  "vertices": [
    {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},{"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
    {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},{"name":"v3","x":"v3x","y":"v3y","z":"v3z"},
    {"name":"v4","x":"v4x","y":"v4y","z":"v4z"},{"name":"v5","x":"v5x","y":"v5y","z":"v5z"},
    {"name":"v6","x":"v6x","y":"v6y","z":"v6z"},{"name":"v7","x":"v7x","y":"v7y","z":"v7z"}
  ],
  "faces": [
    {"name":"bottom","verts":["v0","v3","v2","v1"],"opposite":"top"   },
    {"name":"top",   "verts":["v4","v5","v6","v7"],"opposite":"bottom"},
    {"name":"front", "verts":["v0","v1","v5","v4"],"opposite":"back"  },
    {"name":"right", "verts":["v1","v2","v6","v5"],"opposite":"left"  },
    {"name":"back",  "verts":["v2","v3","v7","v6"],"opposite":"front" },
    {"name":"left",  "verts":["v3","v0","v4","v7"],"opposite":"right" }
  ],
  "handles": [
    {"name":"v0_h","vertex":"v0","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v1_h","vertex":"v1","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v2_h","vertex":"v2","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v3_h","vertex":"v3","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v4_h","vertex":"v4","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v5_h","vertex":"v5","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v6_h","vertex":"v6","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v7_h","vertex":"v7","dragType":"DRAG_NO_INTERSECT"},
    {"name":"e01_h","edge":["v0","v1"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e12_h","edge":["v1","v2"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e23_h","edge":["v2","v3"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e30_h","edge":["v3","v0"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e45_h","edge":["v4","v5"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e56_h","edge":["v5","v6"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e67_h","edge":["v6","v7"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e74_h","edge":["v7","v4"],"dragType":"DRAG_ON_PLANE"},
    {"name":"e04_h","edge":["v0","v4"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"e15_h","edge":["v1","v5"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"e26_h","edge":["v2","v6"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"e37_h","edge":["v3","v7"],"dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_bot_h","face":"bottom","dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_top_h","face":"top",   "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_frt_h","face":"front", "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_rgt_h","face":"right", "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_bak_h","face":"back",  "dragType":"DRAG_NO_INTERSECT"},
    {"name":"f_lft_h","face":"left",  "dragType":"DRAG_NO_INTERSECT"}
  ]
})";

// Minimal ARB8 schema for createArb8EditCycle (vertex handles + face handle)
static const char* kArb8EditCycleSchema = R"({
  "type": "ARB8_viewer_ec",
  "label": "ARB8 Edit Cycle Demo",
  "params": [
    {"name":"v0x","type":"float","default":-1.0},{"name":"v0y","type":"float","default":-1.0},{"name":"v0z","type":"float","default":-1.0},
    {"name":"v1x","type":"float","default": 1.0},{"name":"v1y","type":"float","default":-1.0},{"name":"v1z","type":"float","default":-1.0},
    {"name":"v2x","type":"float","default": 1.0},{"name":"v2y","type":"float","default":-1.0},{"name":"v2z","type":"float","default": 1.0},
    {"name":"v3x","type":"float","default":-1.0},{"name":"v3y","type":"float","default":-1.0},{"name":"v3z","type":"float","default": 1.0},
    {"name":"v4x","type":"float","default":-1.0},{"name":"v4y","type":"float","default": 1.0},{"name":"v4z","type":"float","default":-1.0},
    {"name":"v5x","type":"float","default": 1.0},{"name":"v5y","type":"float","default": 1.0},{"name":"v5z","type":"float","default":-1.0},
    {"name":"v6x","type":"float","default": 1.0},{"name":"v6y","type":"float","default": 1.0},{"name":"v6z","type":"float","default": 1.0},
    {"name":"v7x","type":"float","default":-1.0},{"name":"v7y","type":"float","default": 1.0},{"name":"v7z","type":"float","default": 1.0}
  ],
  "vertices": [
    {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},{"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
    {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},{"name":"v3","x":"v3x","y":"v3y","z":"v3z"},
    {"name":"v4","x":"v4x","y":"v4y","z":"v4z"},{"name":"v5","x":"v5x","y":"v5y","z":"v5z"},
    {"name":"v6","x":"v6x","y":"v6y","z":"v6z"},{"name":"v7","x":"v7x","y":"v7y","z":"v7z"}
  ],
  "faces": [
    {"name":"bottom","verts":["v0","v3","v2","v1"],"opposite":"top"},
    {"name":"top",   "verts":["v4","v5","v6","v7"],"opposite":"bottom"},
    {"name":"front", "verts":["v0","v1","v5","v4"]},
    {"name":"back",  "verts":["v3","v7","v6","v2"]},
    {"name":"left",  "verts":["v0","v4","v7","v3"]},
    {"name":"right", "verts":["v1","v2","v6","v5"]}
  ],
  "handles": [
    {"name":"v0_h","vertex":"v0","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v1_h","vertex":"v1","dragType":"DRAG_NO_INTERSECT"},
    {"name":"v4_h","vertex":"v4","dragType":"DRAG_NO_INTERSECT"},
    {"name":"top_h","face":"top","dragType":"DRAG_ALONG_AXIS"}
  ]
})";

static bool ts_arb8DaggersRegistered   = false;
static bool ts_arb8EditCycleRegistered = false;

// =========================================================================
// 92. createArb8Draggers — ARB8 SoProceduralShape with interactive handles
// =========================================================================
SoSeparator* createArb8Draggers(int width, int height)
{
    if (!ts_arb8DaggersRegistered) {
        SoProceduralShape::registerShapeType(
            "ARB8_viewer", kArb8DaggersSchema,
            arb8SceneBBox, arb8SceneGeom);
        ts_arb8DaggersRegistered = true;
    }

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(3.0f, 2.5f, 3.0f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(lt);

    // Solid semi-transparent body
    SoSeparator *solidSep = new SoSeparator;
    SoMaterial *matSolid = new SoMaterial;
    matSolid->diffuseColor.setValue(0.3f, 0.5f, 0.9f);
    matSolid->specularColor.setValue(0.4f, 0.4f, 0.4f);
    matSolid->shininess.setValue(0.5f);
    matSolid->transparency.setValue(0.3f);
    solidSep->addChild(matSolid);
    SoProceduralShape *shape = new SoProceduralShape;
    shape->setShapeType("ARB8_viewer");
    solidSep->addChild(shape);

    // Add interactive vertex/edge/face dragger handles
    SoSeparator *handles = shape->buildHandleDraggers();
    if (handles) solidSep->addChild(handles);

    root->addChild(solidSep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    cam->position.setValue(cam->position.getValue() * 1.3f);
    return root;
}

// =========================================================================
// 93. createArb8EditCycle — ARB8 SoProceduralShape edit-cycle visualization
// =========================================================================
SoSeparator* createArb8EditCycle(int width, int height)
{
    if (!ts_arb8EditCycleRegistered) {
        static const auto objCB = [](const char*, void*) -> SbBool {
            return TRUE;
        };
        SoProceduralShape::registerShapeType(
            "ARB8_viewer_ec", kArb8EditCycleSchema,
            arb8SceneBBox, arb8SceneGeom,
            nullptr, nullptr, nullptr,
            static_cast<SoProceduralObjectValidateCB>(objCB));
        ts_arb8EditCycleRegistered = true;
    }

    SoSeparator *root = new SoSeparator;
    root->ref();

    /* The camera is positioned along the diagonal from the cube so that
     * three faces are visible simultaneously.  With the corrected outward
     * face normals in arb8SceneGeom this direction is correctly lit by the
     * directional light (previously inward normals caused zero diffuse on
     * all faces visible from the diagonal). */
    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(2.5f, 2.0f, 2.5f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(lt);

    // Solid body
    SoSeparator *shapeSep = new SoSeparator;
    SoMaterial *matSolid = new SoMaterial;
    matSolid->diffuseColor.setValue(0.25f, 0.45f, 0.85f);
    matSolid->specularColor.setValue(0.4f, 0.4f, 0.4f);
    matSolid->shininess.setValue(0.5f);
    shapeSep->addChild(matSolid);
    SoProceduralShape *shape = new SoProceduralShape;
    shape->setShapeType("ARB8_viewer_ec");
    shapeSep->addChild(shape);
    root->addChild(shapeSep);

    /* Call viewAll on the solid geometry BEFORE adding the selection-display
     * overlay.  buildSelectionDisplay() inserts SoText2 labels that have
     * no well-defined 3-D bounding box; they would cause viewAll() to
     * produce an incorrect camera position (often pushing the camera far
     * past the scene's far-clipping plane). */
    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    const float backoff = 1.5f;
    cam->position.setValue(cam->position.getValue() * backoff);
    /* Extend the far-clipping plane so the full scene remains visible
     * after moving the camera back (matches setupCamera() in the test). */
    cam->farDistance.setValue(cam->farDistance.getValue() * backoff * 1.5f);

    // Golden selection-display overlay (labelled handle spheres) — added
    // AFTER viewAll so SoText2 labels do not skew the camera setup.
    SoSeparator *selDisp = shape->buildSelectionDisplay();
    if (selDisp) {
        SoSeparator *selSep = new SoSeparator;
        SoBaseColor *selCol = new SoBaseColor;
        selCol->rgb.setValue(1.0f, 0.8f, 0.0f);
        selSep->addChild(selCol);
        selSep->addChild(selDisp);
        root->addChild(selSep);
    }

    // Add interactive dragger handles
    SoSeparator *handles = shape->buildHandleDraggers();
    if (handles) root->addChild(handles);

    return root;
}

// =========================================================================
// 94. createExtSelection — SoExtSelection + 3 shapes (LASSO, FULL_BBOX)
// =========================================================================
SoSeparator* createExtSelection(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 10.0f);
    cam->height.setValue(10.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoExtSelection *extSel = new SoExtSelection;
    extSel->lassoType.setValue(SoExtSelection::LASSO);
    extSel->lassoMode.setValue(SoExtSelection::FULL_BBOX);
    root->addChild(extSel);

    float xs[3]  = { -3.0f, 0.0f, 3.0f };
    float rs[3]  = { 0.5f, 0.3f, 0.3f };
    float gs[3]  = { 0.3f, 0.5f, 0.7f };
    float bs[3]  = { 0.3f, 0.7f, 0.5f };
    SoNode *shapes[3] = { new SoSphere, new SoCube, new SoCone };
    for (int i = 0; i < 3; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(rs[i], gs[i], bs[i]);
        sep->addChild(mat);
        sep->addChild(shapes[i]);
        extSel->addChild(sep);
    }
    return root;
}

// =========================================================================
// 95. createExtSelectionEvents — SoExtSelection (RECTANGLE, PART_BBOX)
// =========================================================================
SoSeparator* createExtSelectionEvents(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 10.0f);
    cam->height.setValue(10.0f);
    root->addChild(cam);
    root->addChild(new SoDirectionalLight);

    SoExtSelection *extSel = new SoExtSelection;
    extSel->lassoType.setValue(SoExtSelection::RECTANGLE);
    extSel->lassoMode.setValue(SoExtSelection::PART_BBOX);
    root->addChild(extSel);

    // Sphere (upper-left)
    SoSeparator *sep1 = new SoSeparator;
    SoTranslation *t1 = new SoTranslation;
    t1->translation.setValue(-2.0f, 1.5f, 0.0f);
    sep1->addChild(t1);
    SoMaterial *m1 = new SoMaterial;
    m1->diffuseColor.setValue(0.8f, 0.3f, 0.3f);
    sep1->addChild(m1);
    sep1->addChild(new SoSphere);
    extSel->addChild(sep1);

    // Cube (right)
    SoSeparator *sep2 = new SoSeparator;
    SoTranslation *t2 = new SoTranslation;
    t2->translation.setValue(2.0f, 0.0f, 0.0f);
    sep2->addChild(t2);
    SoMaterial *m2 = new SoMaterial;
    m2->diffuseColor.setValue(0.3f, 0.8f, 0.3f);
    sep2->addChild(m2);
    sep2->addChild(new SoCube);
    extSel->addChild(sep2);

    // Cone (lower center)
    SoSeparator *sep3 = new SoSeparator;
    SoTranslation *t3 = new SoTranslation;
    t3->translation.setValue(0.0f, -2.0f, 0.0f);
    sep3->addChild(t3);
    SoMaterial *m3 = new SoMaterial;
    m3->diffuseColor.setValue(0.3f, 0.3f, 0.8f);
    sep3->addChild(m3);
    sep3->addChild(new SoCone);
    extSel->addChild(sep3);

    return root;
}

// =========================================================================
// 96. createRaypickShapes — SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder
// =========================================================================
SoSeparator* createRaypickShapes(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-0.5f, -0.8f, -0.6f);
    root->addChild(lt);

    const float s = 2.5f;

    // Top-left: red SoLineSet
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-s * 0.5f, s * 0.5f, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.85f, 0.15f, 0.15f);
        sep->addChild(mat);
        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-0.7f, 0.0f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.7f, 0.0f, 0.0f));
        sep->addChild(coords);
        SoLineSet *ls = new SoLineSet;
        ls->numVertices.set1Value(0, 2);
        sep->addChild(ls);
        root->addChild(sep);
    }
    // Top-right: green SoIndexedLineSet (diagonal)
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(s * 0.5f, s * 0.5f, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.15f, 0.75f, 0.15f);
        sep->addChild(mat);
        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(-0.6f, -0.6f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.6f,  0.6f, 0.0f));
        sep->addChild(coords);
        SoIndexedLineSet *ils = new SoIndexedLineSet;
        ils->coordIndex.set1Value(0, 0);
        ils->coordIndex.set1Value(1, 1);
        ils->coordIndex.set1Value(2, -1);
        sep->addChild(ils);
        root->addChild(sep);
    }
    // Bottom-left: blue SoPointSet (4-point cross)
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(-s * 0.5f, -s * 0.5f, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.15f, 0.35f, 0.90f);
        sep->addChild(mat);
        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f( 0.0f,  0.5f, 0.0f));
        coords->point.set1Value(1, SbVec3f( 0.0f, -0.5f, 0.0f));
        coords->point.set1Value(2, SbVec3f(-0.5f,  0.0f, 0.0f));
        coords->point.set1Value(3, SbVec3f( 0.5f,  0.0f, 0.0f));
        sep->addChild(coords);
        sep->addChild(new SoPointSet);
        root->addChild(sep);
    }
    // Bottom-right: gold SoCylinder (reference solid)
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(s * 0.5f, -s * 0.5f, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.90f, 0.75f, 0.15f);
        mat->specularColor.setValue(0.6f, 0.6f, 0.6f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);
        sep->addChild(new SoCylinder);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 97. createShadowAdvanced — SoShadowGroup + SoShadowSpotLight + sphere + ground
// =========================================================================
SoSeparator* createShadowAdvanced(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 4.0f, 7.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.5f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoShadowGroup *sg = new SoShadowGroup;
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.8f);
    sg->precision.setValue(0.5f);
    sg->quality.setValue(1.0f);
    sg->smoothBorder.setValue(0.2f);
    sg->visibilityRadius.setValue(15.0f);
    sg->visibilityNearRadius.setValue(0.1f);
    sg->epsilon.setValue(0.001f);
    sg->threshold.setValue(0.1f);
    sg->shadowCachingEnabled.setValue(FALSE);
    root->addChild(sg);

    SoShadowSpotLight *spot = new SoShadowSpotLight;
    spot->location.setValue(0.0f, 5.0f, 3.0f);
    spot->direction.setValue(0.0f, -1.0f, -0.5f);
    spot->cutOffAngle.setValue(0.6f);
    spot->dropOffRate.setValue(0.3f);
    spot->intensity.setValue(1.0f);
    spot->nearDistance.setValue(0.5f);
    spot->farDistance.setValue(20.0f);
    sg->addChild(spot);

    // Ground plane (SHADOWED)
    {
        SoSeparator *planeSep = new SoSeparator;
        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::SHADOWED);
        planeSep->addChild(ss);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.8f, 0.8f, 0.7f);
        planeSep->addChild(mat);
        static const float pts[4][3] = {
            {-4.0f,-1.5f,-4.0f}, {4.0f,-1.5f,-4.0f},
            {4.0f,-1.5f, 4.0f}, {-4.0f,-1.5f, 4.0f}
        };
        static const int nverts[] = { 4 };
        SoNormal *n = new SoNormal;
        n->vector.set1Value(0, SbVec3f(0.0f, 1.0f, 0.0f));
        planeSep->addChild(n);
        SoNormalBinding *nb = new SoNormalBinding;
        nb->value.setValue(SoNormalBinding::OVERALL);
        planeSep->addChild(nb);
        SoCoordinate3 *c3 = new SoCoordinate3;
        c3->point.setValues(0, 4, pts);
        planeSep->addChild(c3);
        SoFaceSet *fs = new SoFaceSet;
        fs->numVertices.setValues(0, 1, nverts);
        planeSep->addChild(fs);
        sg->addChild(planeSep);
    }

    // Sphere (CASTS_SHADOW_AND_SHADOWED)
    {
        SoSeparator *sphereSep = new SoSeparator;
        SoShadowStyle *ss = new SoShadowStyle;
        ss->style.setValue(SoShadowStyle::CASTS_SHADOW_AND_SHADOWED);
        sphereSep->addChild(ss);
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0.0f, 0.5f, 0.0f);
        sphereSep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.7f, 0.2f, 0.2f);
        sphereSep->addChild(mat);
        sphereSep->addChild(new SoSphere);
        sg->addChild(sphereSep);
    }

    return root;
}

// =========================================================================
// 98. createRTProxyShapes — SoLineSet, SoIndexedLineSet, SoPointSet, SoCylinder
// =========================================================================
SoSeparator* createRTProxyShapes(int width, int height)
{
    // Same quad layout as createRaypickShapes; both scenes share this geometry.
    return createRaypickShapes(width, height);
}

// =========================================================================
// 99. createNanoRT — four primitives + SoRaytracingParams for NanoRT tests
// =========================================================================
SoSeparator* createNanoRT(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoRaytracingParams *rtParams = new SoRaytracingParams;
    rtParams->shadowsEnabled.setValue(FALSE);
    root->addChild(rtParams);

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *dirLight = new SoDirectionalLight;
    dirLight->direction.setValue(-0.5f, -1.0f, -0.3f);
    root->addChild(dirLight);

    // Four primitives in a 2×2 grid (matching the nanort.cpp scene layout)
    const float s = 1.5f;
    struct PrimSpec { float r, g, b, x, y; SoNode *shape; };
    PrimSpec prims[4] = {
        { 0.85f, 0.15f, 0.15f, -s * 0.5f,  s * 0.5f, new SoSphere   },
        { 0.15f, 0.75f, 0.15f,  s * 0.5f,  s * 0.5f, new SoCube     },
        { 0.15f, 0.35f, 0.90f, -s * 0.5f, -s * 0.5f, new SoCone     },
        { 0.90f, 0.75f, 0.15f,  s * 0.5f, -s * 0.5f, new SoCylinder }
    };
    for (int i = 0; i < 4; ++i) {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *t = new SoTranslation;
        t->translation.setValue(prims[i].x, prims[i].y, 0.0f);
        sep->addChild(t);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(prims[i].r, prims[i].g, prims[i].b);
        sep->addChild(mat);
        sep->addChild(prims[i].shape);
        root->addChild(sep);
    }

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 100. createNanoRTShadow — ground + red sphere + SoRaytracingParams(shadows)
// =========================================================================
SoSeparator* createNanoRTShadow(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoRaytracingParams *rtParams = new SoRaytracingParams;
    rtParams->shadowsEnabled.setValue(TRUE);
    rtParams->ambientIntensity.setValue(0.2f);
    root->addChild(rtParams);

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 5.0f, 8.0f);
    cam->orientation.setValue(SbVec3f(1.0f, 0.0f, 0.0f), -0.5f);
    cam->nearDistance = 0.5f;
    cam->farDistance  = 50.0f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(-0.3f, -1.0f, 0.0f);
    light->intensity.setValue(1.0f);
    root->addChild(light);

    // Ground plane at y = -1.0
    {
        SoSeparator *plane = new SoSeparator;
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.7f, 0.7f, 0.7f);
        mat->specularColor.setValue(0.1f, 0.1f, 0.1f);
        mat->shininess.setValue(0.05f);
        plane->addChild(mat);
        SoNormal *n = new SoNormal;
        n->vector.set1Value(0, SbVec3f(0.0f, 1.0f, 0.0f));
        plane->addChild(n);
        SoNormalBinding *nb = new SoNormalBinding;
        nb->value.setValue(SoNormalBinding::OVERALL);
        plane->addChild(nb);
        static const float pts[4][3] = {
            {-4.0f,-1.0f,-4.0f}, {4.0f,-1.0f,-4.0f},
            {4.0f,-1.0f, 4.0f}, {-4.0f,-1.0f, 4.0f}
        };
        static const int idx[] = { 0, 1, 2, 3, -1 };
        SoCoordinate3 *c3 = new SoCoordinate3;
        c3->point.setValues(0, 4, pts);
        plane->addChild(c3);
        SoIndexedFaceSet *ifs = new SoIndexedFaceSet;
        ifs->coordIndex.setValues(0, 5, idx);
        plane->addChild(ifs);
        root->addChild(plane);
    }

    // Red sphere suspended above ground
    {
        SoSeparator *sep = new SoSeparator;
        SoTranslation *tr = new SoTranslation;
        tr->translation.setValue(0.0f, 0.5f, 0.0f);
        sep->addChild(tr);
        SoMaterial *mat = new SoMaterial;
        mat->diffuseColor.setValue(0.85f, 0.15f, 0.15f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
        root->addChild(sep);
    }

    return root;
}

// =========================================================================
// Motion callback: apply the dragger's accumulated transformation to the
// paired SoTransform so the reference geometry moves with the dragger.
static void draggerToGeomMotionCB(void* userData, SoDragger* dragger)
{
    SoTransform* xf = static_cast<SoTransform*>(userData);
    SbMatrix m = dragger->getMotionMatrix();
    SbVec3f t, s, center;
    SbRotation r, so;
    m.getTransform(t, r, s, so, center);
    xf->translation.setValue(t);
    xf->rotation.setValue(r);
    xf->scaleFactor.setValue(s);
    xf->scaleOrientation.setValue(so);
    xf->center.setValue(center);
}

// buildDraggerTestScene — camera + light + reference cube + given dragger
// =========================================================================
SoSeparator* buildDraggerTestScene(SoDragger* dragger, int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = addCameraAndLight(root);

    // Reference geometry: green cube driven by the dragger's motion matrix.
    SoSeparator *geom = new SoSeparator;
    SoTransform *cubeXform = new SoTransform;  // driven by dragger motion
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f, 0.7f, 0.5f);
    geom->addChild(cubeXform);
    geom->addChild(mat);
    geom->addChild(new SoCube);
    root->addChild(geom);

    root->addChild(dragger);

    // Wire the dragger so every motion update also moves the cube.
    dragger->addMotionCallback(draggerToGeomMotionCB, cubeXform);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);

    root->unrefNoDelete(); // transfer ownership to caller
    return root;
}

// =========================================================================
// buildManipTestBase — camera + light + purple sphere + plain SoTransform
// =========================================================================
SoSeparator* buildManipTestBase(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    cam->position.setValue(0.0f, 0.0f, 8.0f);
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-0.5f, -1.0f, -0.5f);
    root->addChild(lt);

    SoSeparator *shapeSep = new SoSeparator;
    SoTransform *xf = new SoTransform;
    shapeSep->addChild(xf);
    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.6f, 0.4f, 0.8f);
    shapeSep->addChild(mat);
    shapeSep->addChild(new SoSphere);
    root->addChild(shapeSep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);

    root->unrefNoDelete(); // transfer ownership to caller
    return root;
}

// =========================================================================
// 103. Viewport — blue sphere scene for SoViewport API tests
// =========================================================================
SoSeparator* createViewport(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.2f, 0.4f, 0.9f);   // blue
    root->addChild(mat);

    root->addChild(new SoSphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 104. ViewportScene — green sphere rendered via SoViewport (control image)
// =========================================================================
SoSeparator* createViewportScene(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(0.1f, 0.8f, 0.2f);   // green
    root->addChild(mat);

    root->addChild(new SoSphere);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 105. QuadViewport — LOD scene (sphere/cube/cone) for SoQuadViewport tests
// =========================================================================
SoSeparator* createQuadViewport(int width, int height)
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera *cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight *lt = new SoDirectionalLight;
    lt->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(lt);

    SoLOD *lod = new SoLOD;
    lod->range.set1Value(0,  5.0f);
    lod->range.set1Value(1, 12.0f);

    // HIGH detail: green sphere
    SoSeparator *hi = new SoSeparator;
    SoMaterial  *hiMat = new SoMaterial;
    hiMat->diffuseColor.setValue(0.1f, 0.8f, 0.1f);
    hi->addChild(hiMat);
    hi->addChild(new SoSphere);
    lod->addChild(hi);

    // MEDIUM detail: orange cube
    SoSeparator *med = new SoSeparator;
    SoMaterial  *medMat = new SoMaterial;
    medMat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    med->addChild(medMat);
    med->addChild(new SoCube);
    lod->addChild(med);

    // LOW detail: red cone
    SoSeparator *lo = new SoSeparator;
    SoMaterial  *loMat = new SoMaterial;
    loMat->diffuseColor.setValue(0.8f, 0.1f, 0.1f);
    lo->addChild(loMat);
    lo->addChild(new SoCone);
    lod->addChild(lo);

    root->addChild(lod);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    /* Place camera at ~8 units (medium LOD range: 5–12) → orange cube. */
    SbVec3f pos = cam->position.getValue();
    pos.normalize();
    cam->position.setValue(pos * 8.0f);
    return root;
}

// =========================================================================
// 106. QuadViewportLOD — LOD composite scene (control image source)
// =========================================================================
SoSeparator* createQuadViewportLOD(int width, int height)
{
    /* Return the same LOD scene as createQuadViewport; the control image
     * is generated from this single-image factory render. */
    return createQuadViewport(width, height);
}

// =========================================================================
// 107. SceneTextureMultiMgr — SoSceneTexture2 quad scene (multi-mgr test)
// =========================================================================
SoSeparator* createSceneTextureMultiMgr(int width, int height)
{
    (void)width; (void)height;

    SoSeparator *root = new SoSeparator;
    root->ref();

    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(0.0f, 0.0f, 2.0f);
    cam->nearDistance = 0.1f;
    cam->farDistance  = 10.0f;
    cam->height       = 2.2f;
    root->addChild(cam);

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(0.0f, 0.0f, -1.0f);
    root->addChild(light);

    SoSceneTexture2 *stex = new SoSceneTexture2;
    stex->size.setValue(SbVec2s(64, 64));
    stex->backgroundColor.setValue(0.0f, 0.0f, 0.2f, 1.0f);
    stex->type.setValue(SoSceneTexture2::RGBA8);
    stex->wrapS.setValue(SoSceneTexture2::CLAMP);
    stex->wrapT.setValue(SoSceneTexture2::CLAMP);
    stex->scene.setValue(ts_buildConeSubScene());
    root->addChild(stex);

    SoMaterial *mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);

    SoTextureCoordinate2 *tc = new SoTextureCoordinate2;
    tc->point.set1Value(0, SbVec2f(0.0f, 0.0f));
    tc->point.set1Value(1, SbVec2f(1.0f, 0.0f));
    tc->point.set1Value(2, SbVec2f(1.0f, 1.0f));
    tc->point.set1Value(3, SbVec2f(0.0f, 1.0f));
    root->addChild(tc);

    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
    coords->point.set1Value(1, SbVec3f( 1.0f, -1.0f, 0.0f));
    coords->point.set1Value(2, SbVec3f( 1.0f,  1.0f, 0.0f));
    coords->point.set1Value(3, SbVec3f(-1.0f,  1.0f, 0.0f));
    root->addChild(coords);

    SoFaceSet *fs = new SoFaceSet;
    fs->numVertices.setValue(4);
    root->addChild(fs);

    return root;
}

} // namespace Scenes
} // namespace ObolTest
