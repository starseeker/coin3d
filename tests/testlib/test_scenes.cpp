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
#include <Inventor/nodes/SoSceneRendererParams.h>
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
#include <cstdlib>

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
SoSeparator* createText3(int width, int height)
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
// 7b. Text demo — historical mixed SoText3/SoText2 scene
// =========================================================================
SoSeparator* createTextDemo(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

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

    SoSeparator* t2sep = new SoSeparator;
    SoTranslation* t2pos = new SoTranslation;
    t2pos->translation.setValue(0.0f, 0.5f, 0.0f);
    t2sep->addChild(t2pos);
    SoMaterial* t2mat = new SoMaterial;
    t2mat->diffuseColor.setValue(0.2f, 0.8f, 0.3f);
    t2sep->addChild(t2mat);
    SoFont* t2font = new SoFont;
    t2font->size.setValue(20.0f);
    t2sep->addChild(t2font);
    SoText2* text2 = new SoText2;
    text2->string.setValue("3D Test");
    t2sep->addChild(text2);
    root->addChild(t2sep);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    const SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos[0], pos[1], pos[2] * 1.8f);
    return root;
}

// =========================================================================
// 7c. Text2 (SoText2 — 2-D screen-space billboard text)
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

    // NanoRT hint: enable shadow rays.  SoSceneRendererParams is a no-op for
    // GL renderers; the SoShadowGroup below handles GL shadow maps.
    SoSceneRendererParams* rtParams = new SoSceneRendererParams;
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
    // normal directional light; shadow rays are controlled by SoSceneRendererParams.
    SoShadowGroup* sg = new SoShadowGroup;
    sg->isActive.setValue(TRUE);
    sg->intensity.setValue(0.7f);
    sg->precision.setValue(0.5f);
    sg->quality.setValue(1.0f);
    // The bundled OSMesa GLSL implementation currently rejects the legacy
    // gl_FrontLightModelProduct/gl_FrontMaterial struct members emitted by
    // the shadow shader.  Keep the scene in the software-renderer lane as a
    // meaningful geometry/fallback test; the NanoRT shadow test covers
    // backend-independent shadow behavior there.
    const char * backend = std::getenv("OBOL_TEST_RENDER_BACKEND");
    if (backend && std::strcmp(backend, "swrast") == 0)
        sg->isActive.setValue(FALSE);
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

    // Keep the explicit shadow-test camera.  viewAll() traverses the
    // SoShadowGroup's auxiliary shadow-map scene while it is still being
    // assembled, which can produce a degenerate camera volume on software
    // rasterizers and leave the final image empty.
    cam->nearDistance = 0.1f;
    cam->farDistance = 50.0f;
    (void)width;
    (void)height;
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
