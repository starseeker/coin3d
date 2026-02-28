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

#ifdef OBOL_OSMESA_BUILD
#  include <OSMesa/gl.h>
#else
#  ifdef __unix__
#    include <GL/gl.h>
#  else
#    include <OpenGL/gl.h>
#  endif
#endif

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

// Gradient background rendered via a callback node.
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
        glColor3f(0.05f, 0.05f, 0.20f); glVertex2i(0, 0);
        glColor3f(0.05f, 0.05f, 0.20f); glVertex2i(w, 0);
        // top: lighter blue
        glColor3f(0.20f, 0.35f, 0.60f); glVertex2i(w, h);
        glColor3f(0.20f, 0.35f, 0.60f); glVertex2i(0, h);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
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

    struct MatSpec { float dr,dg,db; float sr,sg,sb; float shine; float ambient; };
    const MatSpec specs[] = {
        { 0.8f,0.1f,0.1f,  0.8f,0.8f,0.8f,  0.9f, 0.2f }, // shiny red
        { 0.1f,0.7f,0.1f,  0.0f,0.0f,0.0f,  0.0f, 0.3f }, // matte green
        { 0.1f,0.2f,0.9f,  0.9f,0.9f,0.9f,  0.5f, 0.1f }, // semi-shiny blue
        { 0.8f,0.6f,0.1f,  0.3f,0.3f,0.1f,  0.2f, 0.4f }, // gold-ish
    };
    const float xs[] = {-4.5f, -1.5f, 1.5f, 4.5f};
    for (int i = 0; i < 4; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(xs[i], 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(specs[i].dr, specs[i].dg, specs[i].db);
        mat->specularColor.setValue(specs[i].sr, specs[i].sg, specs[i].sb);
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

    // NanoRT hint: enable shadow rays so the NanoRT panel shows the sphere's
    // shadow on the floor.  Ignored by GL renderers.
    SoRaytracingParams* rtParams = new SoRaytracingParams;
    rtParams->shadowsEnabled.setValue(TRUE);
    rtParams->ambientIntensity.setValue(0.15f);
    root->addChild(rtParams);

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    // Directional light from upper-left (warm white)
    SoDirectionalLight* dlight = new SoDirectionalLight;
    dlight->direction.setValue(-1.0f, -1.0f, -0.5f);
    dlight->color.setValue(1.0f, 0.9f, 0.8f);
    dlight->intensity.setValue(0.7f);
    root->addChild(dlight);

    // Point light from the right (cool blue accent)
    SoPointLight* plight = new SoPointLight;
    plight->location.setValue(5.0f, 2.0f, 5.0f);
    plight->color.setValue(0.3f, 0.6f, 1.0f);
    plight->intensity.setValue(0.8f);
    root->addChild(plight);

    // Spot light with a tight cone aimed at the sphere (white)
    SoSpotLight* slight = new SoSpotLight;
    slight->location.setValue(-4.0f, 4.0f, 3.0f);
    slight->direction.setValue(0.6f, -0.7f, -0.5f);
    slight->cutOffAngle.setValue(0.4f);
    slight->dropOffRate.setValue(0.5f);
    slight->color.setValue(1.0f, 1.0f, 0.9f);
    slight->intensity.setValue(0.9f);
    root->addChild(slight);

    // Central white sphere
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.9f, 0.9f);
    mat->specularColor.setValue(1.0f, 1.0f, 1.0f);
    mat->shininess.setValue(0.8f);
    root->addChild(mat);
    root->addChild(new SoSphere);

    // Floor plane (cube, flat) to receive shadows in the NanoRT panel
    SoSeparator* floor = new SoSeparator;
    SoTranslation* ft = new SoTranslation;
    ft->translation.setValue(0.0f, -1.5f, 0.0f);
    floor->addChild(ft);
    SoScale* fs = new SoScale;
    fs->scaleFactor.setValue(6.0f, 0.1f, 6.0f);
    floor->addChild(fs);
    SoMaterial* fm = new SoMaterial;
    fm->diffuseColor.setValue(0.55f, 0.55f, 0.55f);
    floor->addChild(fm);
    floor->addChild(new SoCube);
    root->addChild(floor);

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

    const float colors[6][3] = {
        {0.9f,0.2f,0.2f}, {0.2f,0.8f,0.2f}, {0.2f,0.2f,0.9f},
        {0.9f,0.7f,0.1f}, {0.7f,0.2f,0.8f}, {0.1f,0.8f,0.8f},
    };

    // Row 1: Translations
    float xpos[3] = {-4.0f, 0.0f, 4.0f};
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue(xpos[i], 2.5f, 0.0f);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i][0],colors[i][1],colors[i][2]);
        sep->addChild(mat);
        sep->addChild(new SoCube);
        root->addChild(sep);
    }
    // Row 2: Rotations
    SbVec3f rotAxes[3] = {{1,0,0},{0,1,0},{0,0,1}};
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTransform* xf = new SoTransform;
        xf->translation.setValue(xpos[i], -2.5f, 0.0f);
        xf->rotation.setValue(rotAxes[i], float(M_PI / 4.0));
        sep->addChild(xf);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i+3][0],colors[i+3][1],colors[i+3][2]);
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
    // A simple scene with perspective camera and a row of spheres.
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->heightAngle.setValue(float(M_PI / 4.0));
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -1.0f, -0.7f);
    root->addChild(light);

    const float colors[5][3] = {
        {0.9f,0.1f,0.1f},{0.1f,0.9f,0.1f},{0.1f,0.1f,0.9f},
        {0.9f,0.9f,0.1f},{0.1f,0.9f,0.9f}
    };
    for (int i = 0; i < 5; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation;
        t->translation.setValue((i - 2) * 2.5f, 0.0f, 0.0f);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(colors[i][0],colors[i][1],colors[i][2]);
        mat->specularColor.setValue(0.6f,0.6f,0.6f);
        mat->shininess.setValue(0.5f);
        sep->addChild(mat);
        sep->addChild(new SoSphere);
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

    SoTexture2* tex = new SoTexture2;
    buildCheckerTexture(tex);
    root->addChild(tex);

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(1.0f, 1.0f, 1.0f);
    root->addChild(mat);
    root->addChild(new SoCube);

    SbViewportRegion vp(width, height);
    cam->viewAll(root, vp);
    return root;
}

// =========================================================================
// 7. Text
// =========================================================================
SoSeparator* createText(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera* cam = addCameraAndLight(root);

    // SoText3 label (3-D geometry)
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

    // SoText2 label (2-D billboard, screen-space)
    // Anchor placed within the camera frustum that viewAll sets for SoText3.
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
    // Pull back along Z so the SoText2 anchor (below SoText3) stays visible.
    SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos[0], pos[1], pos[2] * 1.8f);
    return root;
}

// =========================================================================
// 8. Gradient background
// =========================================================================
SoSeparator* createGradient(int width, int height)
{
    SoSeparator* root = new SoSeparator;
    root->ref();

    // Gradient background drawn first (before depth-tested geometry)
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

    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    mat->specularColor.setValue(0.8f, 0.8f, 0.8f);
    mat->shininess.setValue(0.5f);
    root->addChild(mat);

    static const float pts[4][3] = {
        { 0.0f,  1.2f,  0.0f },
        {-1.0f, -0.8f,  0.9f },
        { 1.0f, -0.8f,  0.9f },
        { 0.0f, -0.8f, -1.2f }
    };
    static const int32_t idx[] = {
        0, 1, 2, -1,
        0, 2, 3, -1,
        0, 3, 1, -1,
        1, 3, 2, -1
    };

    SoCoordinate3* co = new SoCoordinate3;
    co->point.setValues(0, 4, pts);
    root->addChild(co);

    SoShapeHints* sh = new SoShapeHints;
    sh->vertexOrdering.setValue(SoShapeHints::COUNTERCLOCKWISE);
    sh->shapeType.setValue(SoShapeHints::SOLID);
    root->addChild(sh);

    SoIndexedFaceSet* ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0, 16, idx);
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

} // namespace Scenes
} // namespace ObolTest
