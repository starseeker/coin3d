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

/*!
  \class SoSceneCollector SoSceneCollector.h Inventor/SoSceneCollector.h
  \brief Utility class for collecting world-space scene data for non-GL rendering backends.

  See the header file for full documentation.
*/

#include <Inventor/SoSceneCollector.h>

// Obol scene graph
#include <Inventor/SbColor.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbFont.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/elements/SoCoordinateElement.h>
#include <Inventor/elements/SoLineWidthElement.h>
#include <Inventor/elements/SoPointSizeElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMatrixTransform.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// ==========================================================================
// File-scope callback infrastructure
// ==========================================================================
// The SoCallbackAction callbacks are file-scope static functions, not class
// members, so they don't require SoCallbackAction::Response to appear in the
// public header.  ScRaytracerCbData holds raw pointers to the collector's
// vectors plus the proxy viewport needed for proxy geometry sizing, giving
// the callbacks direct access without class-member visibility.

struct ScRaytracerCbData {
    std::vector<SoScTriangle> *    tris;
    std::vector<SoScLightInfo> *   lights;
    std::vector<SoScTextOverlay> * overlays;
    SbViewportRegion               proxyVp;

    /* Normal-matrix cache: recompute inverse-transpose only when the model
     * matrix changes between triangles.  All triangles of the same shape
     * share the same model matrix, so this avoids one expensive matrix
     * inverse per triangle (O(n) inverse calls → O(shapes) inverse calls). */
    float     lastMM[16];     /* flat row-major copy of last seen model matrix */
    SbMatrix  normalMat;      /* corresponding inverse-transpose */
    bool      normalMatValid; /* false until the first triangle is processed */

    ScRaytracerCbData() : normalMatValid(false) {}
};

// Forward-declare all file-scope callbacks so they can be registered in
// collectImpl() and collectOverlaysOnly() without caring about order.
static void
src_triangleCB(void * ud,
               SoCallbackAction * action,
               const SoPrimitiveVertex * v0,
               const SoPrimitiveVertex * v1,
               const SoPrimitiveVertex * v2);

static SoCallbackAction::Response
src_shapePruneCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_lineSetCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_indexedLineSetCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_pointSetCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_cylinderLineCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_text2CB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_hudLabelCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_hudButtonCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_directionalLightCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_pointLightCB(void * ud, SoCallbackAction * action, const SoNode * node);

static SoCallbackAction::Response
src_spotLightCB(void * ud, SoCallbackAction * action, const SoNode * node);

// Helper: collect triangles from a proxy separator using a sub-SoCallbackAction.
// Injects the current material and model matrix from the parent action.
static void
src_collectProxy(SoCallbackAction * action,
                 SoSeparator * proxy,
                 ScRaytracerCbData * cbdata);
// ==========================================================================
// Constructor / destructor
// ==========================================================================

SoSceneCollector::SoSceneCollector()
{
}

SoSceneCollector::~SoSceneCollector()
{
}

// ==========================================================================
// Public API
// ==========================================================================

void
SoSceneCollector::reset()
{
    tris_.clear();
    lights_.clear();
    overlays_.clear();
}

void
SoSceneCollector::collect(SoNode * root,
                                   const SbViewportRegion & vp)
{
    collectImpl(root, vp, vp);
}

void
SoSceneCollector::collect(SoNode * root,
                                   const SbViewportRegion & renderVp,
                                   const SbViewportRegion & displayVp)
{
    collectImpl(root, renderVp, displayVp);
}

void
SoSceneCollector::collectOverlaysOnly(SoNode * root,
                                               const SbViewportRegion & vp)
{
    overlays_.clear();

    ScRaytracerCbData cbdata;
    cbdata.tris     = &tris_;
    cbdata.lights   = &lights_;
    cbdata.overlays = &overlays_;
    cbdata.proxyVp  = vp;

    SoCallbackAction cba(vp);
    cba.addPreCallback(SoText2::getClassTypeId(),    src_text2CB,     &cbdata);
    cba.addPreCallback(SoHUDLabel::getClassTypeId(), src_hudLabelCB,  &cbdata);
    cba.addPreCallback(SoHUDButton::getClassTypeId(),src_hudButtonCB, &cbdata);
    cba.apply(root);
}

const std::vector<SoScTriangle> &
SoSceneCollector::getTriangles() const
{
    return tris_;
}

const std::vector<SoScLightInfo> &
SoSceneCollector::getLights() const
{
    return lights_;
}

const std::vector<SoScTextOverlay> &
SoSceneCollector::getOverlays() const
{
    return overlays_;
}

// ==========================================================================
// Cache management
// ==========================================================================

SbBool
SoSceneCollector::needsRebuild(SoNode * root, SoCamera * cam) const
{
    if (!root) return TRUE;
    if (root != cachedRoot_) return TRUE;

    const SbUniqueId camId = cam ? cam->getNodeId() : 0;

    // A "camera-only move" is inferred when: same root pointer, same camera
    // pointer, but the camera's node ID has changed.  In that case the root's
    // node ID will also have changed (camera field change propagates up), but
    // we should NOT rebuild the BVH — only a lightweight overlay re-collection
    // is needed.
    const bool cameraOnlyMoved =
        (cam != nullptr) &&
        (cachedCamPtr_ != nullptr) &&
        (cam == cachedCamPtr_) &&
        (camId != cachedCamId_);

    const bool rootChanged = (root->getNodeId() != cachedRootId_);

    if (rootChanged && !cameraOnlyMoved) return TRUE;
    return FALSE;
}

void
SoSceneCollector::updateCacheKeysAfterRebuild(SoNode *  root,
                                                        SoCamera * cam)
{
    cachedRoot_   = root;
    cachedRootId_ = root ? root->getNodeId() : 0;
    cachedCamPtr_ = cam;
    cachedCamId_  = cam ? cam->getNodeId() : 0;
}

void
SoSceneCollector::updateCameraId(SoCamera * cam, SoNode * root)
{
    cachedCamId_  = cam  ? cam->getNodeId()  : 0;
    if (root) cachedRootId_ = root->getNodeId();
}

void
SoSceneCollector::resetCache()
{
    cachedRoot_   = nullptr;
    cachedRootId_ = 0;
    cachedCamPtr_ = nullptr;
    cachedCamId_  = 0;
}

// ==========================================================================
// Overlay compositing
// ==========================================================================

void
SoSceneCollector::compositeOverlays(unsigned char * pixels,
                                              unsigned int width,
                                              unsigned int height,
                                              unsigned int nrcomponents) const
{
    for (const SoScTextOverlay & ov : overlays_) {
        const int src_x0 = std::max(0, -ov.x);
        const int src_y0 = std::max(0, -ov.y);
        const int dst_x0 = std::max(0,  ov.x);
        const int dst_y0 = std::max(0,  ov.y);
        const int draw_w = std::min(ov.w - src_x0,
                                    static_cast<int>(width)  - dst_x0);
        const int draw_h = std::min(ov.h - src_y0,
                                    static_cast<int>(height) - dst_y0);
        if (draw_w <= 0 || draw_h <= 0) continue;

        for (int row = 0; row < draw_h; ++row) {
            const int src_row = src_y0 + row;
            const unsigned char * src =
                ov.pixbuf.data() + (src_row * ov.w + src_x0) * 4;

            const int dst_row = dst_y0 + row;
            const size_t dst_base =
                static_cast<size_t>(dst_row) * width + dst_x0;

            for (int col = 0; col < draw_w; ++col) {
                const unsigned char r_src = src[0];
                const unsigned char g_src = src[1];
                const unsigned char b_src = src[2];
                const unsigned char a_src = src[3];
                src += 4;

                if (a_src == 0) continue;

                const float fa = a_src * (1.0f / 255.0f);
                const float fb = 1.0f - fa;
                const size_t idx = (dst_base + col) * nrcomponents;
                pixels[idx + 0] = static_cast<unsigned char>(
                    r_src * fa + pixels[idx + 0] * fb);
                pixels[idx + 1] = static_cast<unsigned char>(
                    g_src * fa + pixels[idx + 1] * fb);
                pixels[idx + 2] = static_cast<unsigned char>(
                    b_src * fa + pixels[idx + 2] * fb);
                if (nrcomponents == 4) pixels[idx + 3] = 255;
            }
        }
    }
}

// ==========================================================================
// Proxy geometry utilities (public static)
// ==========================================================================

float
SoSceneCollector::computeWorldSpaceRadius(SoCallbackAction * action,
                                                    float sizePx,
                                                    float viewportHeightPx)
{
    const SbViewVolume & vv =
        SoViewVolumeElement::get(action->getState());
    float worldHeight = vv.getHeight();

    const SbMatrix & mm = action->getModelMatrix();

    if (vv.getProjectionType() == SbViewVolume::PERSPECTIVE) {
        const float nearDist = vv.getNearDist();
        if (nearDist > 1e-6f) {
            const SbVec3f objPos(mm[3][0], mm[3][1], mm[3][2]);
            const float dist = (objPos - vv.getProjectionPoint()).length();
            const float refDist = (dist > nearDist) ? dist : nearDist;
            worldHeight = worldHeight * refDist / nearDist;
        }
    }

    float radius = sizePx * worldHeight / viewportHeightPx * 0.5f;

    /* The returned radius is used as a local-space dimension in proxy
     * geometry (e.g. createCylinderProxy).  src_collectProxy wraps every
     * proxy with the full current model matrix – including any Scale node –
     * which would inflate the cylinder radius by that scale factor.  Divide
     * by the approximate uniform scale (geometric mean of the three row-vector
     * lengths in the upper-left 3×3) so the net world-space radius matches
     * the pixel-size target.  For pure rotation/translation the scale is 1
     * and the division is a no-op. */
    const SbMat & mv = mm.getValue();
    const float sx = sqrtf(mv[0][0]*mv[0][0] + mv[0][1]*mv[0][1] + mv[0][2]*mv[0][2]);
    const float sy = sqrtf(mv[1][0]*mv[1][0] + mv[1][1]*mv[1][1] + mv[1][2]*mv[1][2]);
    const float sz = sqrtf(mv[2][0]*mv[2][0] + mv[2][1]*mv[2][1] + mv[2][2]*mv[2][2]);
    const float scaleProduct = sx * sy * sz;
    if (scaleProduct > 1e-12f)
        radius /= cbrtf(scaleProduct);

    return radius;
}

SoSeparator *
SoSceneCollector::createWireframeCylinderProxy(float cylRadius,
                                                         float tubeRadius,
                                                         int   numSegments)
{
    SoSeparator * proxy = new SoSeparator;
    proxy->ref();

    const float twoPi = 2.0f * static_cast<float>(M_PI);
    for (int i = 0; i < numSegments; ++i) {
        const float a0 = twoPi *  i      / numSegments;
        const float a1 = twoPi * (i + 1) / numSegments;
        const SbVec3f p0(cylRadius * cosf(a0), 0.0f, cylRadius * sinf(a0));
        const SbVec3f p1(cylRadius * cosf(a1), 0.0f, cylRadius * sinf(a1));
        const SbVec3f diff = p1 - p0;
        const float segLen = diff.length();
        if (segLen < 1e-6f) continue;

        SoSeparator * segSep = new SoSeparator;
        SoMatrixTransform * xf = new SoMatrixTransform;
        SbMatrix mat;
        mat.setTransform((p0 + p1) * 0.5f,
                         SbRotation(SbVec3f(0.0f, 1.0f, 0.0f), diff / segLen),
                         SbVec3f(1.0f, 1.0f, 1.0f));
        xf->matrix.setValue(mat);
        segSep->addChild(xf);
        SoCylinder * cyl = new SoCylinder;
        cyl->radius.setValue(tubeRadius);
        cyl->height.setValue(segLen);
        segSep->addChild(cyl);
        proxy->addChild(segSep);
    }

    proxy->unrefNoDelete();
    return proxy;
}

// ==========================================================================
// Internal helpers
// ==========================================================================

// File-scope proxy-collect helper (extracted from the class so all callbacks
// can call it without class-member visibility).
static void
src_collectProxy(SoCallbackAction * action,
                 SoSeparator * proxy,
                 ScRaytracerCbData * cbdata)
{
    // Capture current material from traversal state.
    SbColor ambient, diffuse, specular, emission;
    float   shininess, transparency;
    action->getMaterial(ambient, diffuse, specular, emission,
                        shininess, transparency, 0);

    SoMaterial * mat = new SoMaterial;
    mat->ambientColor .setValue(ambient);
    mat->diffuseColor .setValue(diffuse);
    mat->specularColor.setValue(specular);
    mat->emissiveColor.setValue(emission);
    mat->shininess    .setValue(shininess);
    mat->transparency .setValue(transparency);

    // Apply the current model matrix so the proxy lands in world space.
    SoMatrixTransform * mt = new SoMatrixTransform;
    mt->matrix.setValue(action->getModelMatrix());

    SoSeparator * wrapper = new SoSeparator;
    wrapper->ref();
    wrapper->addChild(mat);
    wrapper->addChild(mt);
    wrapper->addChild(proxy);

    // Run a fresh sub-action to collect triangles from the proxy.
    SoCallbackAction subCba(action->getViewportRegion());
    subCba.addTriangleCallback(SoShape::getClassTypeId(),
                               src_triangleCB, cbdata);
    subCba.apply(wrapper);

    wrapper->unref();
}

// collectProxy (private class method): thin wrapper for public API users who
// have a SoCallbackAction but not a ScRaytracerCbData.
void
SoSceneCollector::collectProxy(SoCallbackAction * action,
                                         SoSeparator * proxy)
{
    ScRaytracerCbData cbdata;
    cbdata.tris     = &tris_;
    cbdata.lights   = &lights_;
    cbdata.overlays = &overlays_;
    cbdata.proxyVp  = action->getViewportRegion();
    src_collectProxy(action, proxy, &cbdata);
}

void
SoSceneCollector::collectImpl(SoNode *              root,
                                        const SbViewportRegion & renderVp,
                                        const SbViewportRegion & proxyVp)
{
    ScRaytracerCbData cbdata;
    cbdata.tris     = &tris_;
    cbdata.lights   = &lights_;
    cbdata.overlays = &overlays_;
    cbdata.proxyVp  = proxyVp;

    SoCallbackAction cba(renderVp);

    // Triangle geometry collection
    cba.addTriangleCallback(SoShape::getClassTypeId(),
                             src_triangleCB, &cbdata);

    // Prune DrawStyle INVISIBLE shapes (picking-only geometry)
    cba.addPreCallback(SoShape::getClassTypeId(),
                       src_shapePruneCB, nullptr);

    // Proxy geometry: lines → cylinders
    cba.addPreCallback(SoLineSet::getClassTypeId(),
                       src_lineSetCB, &cbdata);
    cba.addPreCallback(SoIndexedLineSet::getClassTypeId(),
                       src_indexedLineSetCB, &cbdata);

    // Proxy geometry: points → spheres
    cba.addPreCallback(SoPointSet::getClassTypeId(),
                       src_pointSetCB, &cbdata);

    // Proxy geometry: wireframe cylinders → rings of tubes
    cba.addPreCallback(SoCylinder::getClassTypeId(),
                       src_cylinderLineCB, &cbdata);

    // Screen-space text / HUD overlays
    cba.addPreCallback(SoText2::getClassTypeId(),    src_text2CB,     &cbdata);
    cba.addPreCallback(SoHUDLabel::getClassTypeId(), src_hudLabelCB,  &cbdata);
    cba.addPreCallback(SoHUDButton::getClassTypeId(),src_hudButtonCB, &cbdata);

    // Lights
    cba.addPreCallback(SoDirectionalLight::getClassTypeId(),
                       src_directionalLightCB, &cbdata);
    cba.addPreCallback(SoPointLight::getClassTypeId(),
                       src_pointLightCB, &cbdata);
    cba.addPreCallback(SoSpotLight::getClassTypeId(),
                       src_spotLightCB, &cbdata);

    cba.apply(root);
}

// ==========================================================================
// Static callbacks
// ==========================================================================

void
src_triangleCB(void * ud,
               SoCallbackAction * action,
               const SoPrimitiveVertex * v0,
               const SoPrimitiveVertex * v1,
               const SoPrimitiveVertex * v2)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);

    const SbMatrix & mm = action->getModelMatrix();

    /* Recompute the normal matrix (inverse-transpose) only when the model
     * matrix has actually changed.  All triangles in the same shape share
     * an identical model matrix, so this reduces O(triangles) expensive
     * matrix inversions to O(shapes) inversions during scene collection. */
    const SbMat & mmv = mm.getValue();   /* const float[4][4] */
    if (!cbdata->normalMatValid ||
        std::memcmp(cbdata->lastMM, mmv, sizeof(float) * 16) != 0)
    {
        cbdata->normalMat       = mm.inverse().transpose();
        std::memcpy(cbdata->lastMM, mmv, sizeof(float) * 16);
        cbdata->normalMatValid  = true;
    }
    const SbMatrix & normalMat = cbdata->normalMat;

    SoScTriangle tri;
    const SoPrimitiveVertex * verts[3] = { v0, v1, v2 };
    for (int i = 0; i < 3; ++i) {
        SbVec3f wp, wn;
        mm.multVecMatrix(verts[i]->getPoint(),   wp);
        normalMat.multDirMatrix(verts[i]->getNormal(), wn);
        wn.normalize();
        tri.pos[i][0]  = wp[0]; tri.pos[i][1]  = wp[1]; tri.pos[i][2]  = wp[2];
        tri.norm[i][0] = wn[0]; tri.norm[i][1] = wn[1]; tri.norm[i][2] = wn[2];
    }

    SbColor ambient, diffuse, specular, emission;
    float shininess, transparency;
    action->getMaterial(ambient, diffuse, specular, emission,
                        shininess, transparency,
                        v0->getMaterialIndex());

    tri.mat.diffuse[0]  = diffuse[0];  tri.mat.diffuse[1]  = diffuse[1];  tri.mat.diffuse[2]  = diffuse[2];
    tri.mat.specular[0] = specular[0]; tri.mat.specular[1] = specular[1]; tri.mat.specular[2] = specular[2];
    tri.mat.ambient[0]  = ambient[0];  tri.mat.ambient[1]  = ambient[1];  tri.mat.ambient[2]  = ambient[2];
    tri.mat.emission[0] = emission[0]; tri.mat.emission[1] = emission[1]; tri.mat.emission[2] = emission[2];
    tri.mat.shininess   = shininess;

    cbdata->tris->push_back(tri);
}

SoCallbackAction::Response
src_shapePruneCB(void * /*ud*/,
                                         SoCallbackAction * action,
                                         const SoNode *)
{
    if (action->getDrawStyle() == SoDrawStyle::INVISIBLE)
        return SoCallbackAction::PRUNE;
    return SoCallbackAction::CONTINUE;
}

SoCallbackAction::Response
src_lineSetCB(void * ud,
                                      SoCallbackAction * action,
                                      const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoLineSet * ls = static_cast<const SoLineSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH    = static_cast<float>(
        cbdata->proxyVp.getViewportSizePixels()[1]);
    const float radius = SoSceneCollector::computeWorldSpaceRadius(action, lineW, vpH);

    SoSeparator * proxy = ls->createCylinderProxy(coords, radius);
    proxy->ref();
    src_collectProxy(action, proxy, cbdata);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

SoCallbackAction::Response
src_indexedLineSetCB(void * ud,
                                             SoCallbackAction * action,
                                             const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoIndexedLineSet * ils =
        static_cast<const SoIndexedLineSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH    = static_cast<float>(
        cbdata->proxyVp.getViewportSizePixels()[1]);
    const float radius = SoSceneCollector::computeWorldSpaceRadius(action, lineW, vpH);

    SoSeparator * proxy = ils->createCylinderProxy(coords, radius);
    proxy->ref();
    src_collectProxy(action, proxy, cbdata);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

SoCallbackAction::Response
src_pointSetCB(void * ud,
                                       SoCallbackAction * action,
                                       const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoPointSet * ps = static_cast<const SoPointSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    float ptSz = SoPointSizeElement::get(action->getState());
    if (ptSz <= 0.0f) ptSz = 1.0f;
    const float vpH    = static_cast<float>(
        cbdata->proxyVp.getViewportSizePixels()[1]);
    const float radius = SoSceneCollector::computeWorldSpaceRadius(action, ptSz, vpH);

    SoSeparator * proxy = ps->createSphereProxy(coords, radius);
    proxy->ref();
    src_collectProxy(action, proxy, cbdata);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

SoCallbackAction::Response
src_cylinderLineCB(void * ud,
                                           SoCallbackAction * action,
                                           const SoNode * node)
{
    if (action->getDrawStyle() != SoDrawStyle::LINES)
        return SoCallbackAction::CONTINUE;

    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoCylinder * cyl = static_cast<const SoCylinder *>(node);

    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH     = static_cast<float>(
        cbdata->proxyVp.getViewportSizePixels()[1]);
    const float tubeRad = SoSceneCollector::computeWorldSpaceRadius(action, lineW, vpH);
    const float cylRad  = cyl->radius.getValue();

    SoSeparator * proxy = SoSceneCollector::createWireframeCylinderProxy(cylRad, tubeRad);
    proxy->ref();
    src_collectProxy(action, proxy, cbdata);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

SoCallbackAction::Response
src_text2CB(void * ud,
                                    SoCallbackAction * action,
                                    const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoText2 * text = static_cast<const SoText2 *>(node);
    SoState * state = action->getState();

    SoScTextOverlay ov;
    if (!text->buildPixelBuffer(state, ov.pixbuf, ov.x, ov.y, ov.w, ov.h))
        return SoCallbackAction::PRUNE;

    cbdata->overlays->push_back(std::move(ov));
    return SoCallbackAction::PRUNE;
}

// Approximate line height factor: ascender + descender + leading,
// expressed as a multiplier of fontSize.
static const float kHUDLineHeightFactor = 1.3f;

SoCallbackAction::Response
src_hudLabelCB(void * ud,
                                       SoCallbackAction * /*action*/,
                                       const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoHUDLabel * label = static_cast<const SoHUDLabel *>(node);

    const int nlines = label->string.getNum();
    if (nlines == 0) return SoCallbackAction::PRUNE;

    const float fontSize = label->fontSize.getValue();
    if (fontSize <= 0.0f) return SoCallbackAction::PRUNE;

    SbFont font;
    font.setSize(fontSize);

    const SbColor & col = label->color.getValue();
    const unsigned char cr = static_cast<unsigned char>(col[0] * 255.0f);
    const unsigned char cg = static_cast<unsigned char>(col[1] * 255.0f);
    const unsigned char cb = static_cast<unsigned char>(col[2] * 255.0f);

    const int lineH = static_cast<int>(fontSize * kHUDLineHeightFactor) + 1;

    // Pass 1: compute per-line pixel widths
    std::vector<int> lineWidths(nlines, 0);
    int maxWidth = 0;
    for (int li = 0; li < nlines; ++li) {
        const char * p = label->string[li].getString();
        int x = 0;
        while (*p) {
            const unsigned char ch = static_cast<unsigned char>(*p++);
            SbVec2s sz, bearing;
            font.getGlyphBitmap(ch, sz, bearing);
            x += static_cast<int>(font.getGlyphAdvance(ch)[0]);
        }
        lineWidths[li] = x;
        if (x > maxWidth) maxWidth = x;
    }
    if (maxWidth <= 0) return SoCallbackAction::PRUNE;

    const int canvasW = maxWidth;
    const int canvasH = nlines * lineH;

    SoScTextOverlay ov;
    ov.w = canvasW;
    ov.h = canvasH;
    ov.pixbuf.assign(static_cast<size_t>(canvasW) * canvasH * 4, 0);

    const SbVec2f pos = label->position.getValue();
    ov.x = static_cast<int>(pos[0]);
    ov.y = static_cast<int>(pos[1]) -
           (canvasH - 1 - static_cast<int>(fontSize));

    const int just = label->justification.getValue();
    for (int li = 0; li < nlines; ++li) {
        const int lineTopInCanvas = li * lineH;

        int xstart = 0;
        if (just == SoHUDLabel::RIGHT)
            xstart = maxWidth - lineWidths[li];
        else if (just == SoHUDLabel::CENTER)
            xstart = (maxWidth - lineWidths[li]) / 2;

        const char * p = label->string[li].getString();
        int xpen = xstart;
        while (*p) {
            const unsigned char ch = static_cast<unsigned char>(*p++);
            SbVec2s sz, bearing;
            const unsigned char * bitmap = font.getGlyphBitmap(ch, sz, bearing);
            const SbVec2f adv = font.getGlyphAdvance(ch);

            if (bitmap && sz[0] > 0 && sz[1] > 0) {
                const int baseline = lineTopInCanvas +
                                     static_cast<int>(fontSize);
                const int dst_x = xpen + bearing[0];
                const int dst_y = baseline - bearing[1];

                for (int gy = 0; gy < sz[1]; ++gy) {
                    const int canvas_row = dst_y + gy;
                    if (canvas_row < 0 || canvas_row >= canvasH) continue;
                    const int gl_row = canvasH - 1 - canvas_row;

                    for (int gx = 0; gx < sz[0]; ++gx) {
                        const int canvas_col = dst_x + gx;
                        if (canvas_col < 0 || canvas_col >= canvasW) continue;
                        const unsigned char alpha =
                            bitmap[gy * sz[0] + gx];
                        if (alpha == 0) continue;
                        const size_t idx =
                            (static_cast<size_t>(gl_row) * canvasW +
                             canvas_col) * 4;
                        ov.pixbuf[idx + 0] = cr;
                        ov.pixbuf[idx + 1] = cg;
                        ov.pixbuf[idx + 2] = cb;
                        ov.pixbuf[idx + 3] = alpha;
                    }
                }
            }
            xpen += static_cast<int>(adv[0]);
        }
    }

    cbdata->overlays->push_back(std::move(ov));
    return SoCallbackAction::PRUNE;
}

SoCallbackAction::Response
src_hudButtonCB(void * ud,
                                        SoCallbackAction * /*action*/,
                                        const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoHUDButton * btn = static_cast<const SoHUDButton *>(node);

    const SbVec2f pos  = btn->position.getValue();
    const SbVec2f size = btn->size.getValue();
    const int btnW = static_cast<int>(size[0]);
    const int btnH = static_cast<int>(size[1]);
    if (btnW <= 0 || btnH <= 0) return SoCallbackAction::PRUNE;

    SoScTextOverlay ov;
    ov.w = btnW;
    ov.h = btnH;
    ov.x = static_cast<int>(pos[0]);
    ov.y = static_cast<int>(pos[1]);
    ov.pixbuf.assign(static_cast<size_t>(btnW) * btnH * 4, 0);

    // Draw 1-pixel border rectangle using borderColor
    const SbColor & bcol = btn->borderColor.getValue();
    const unsigned char br = static_cast<unsigned char>(bcol[0] * 255.0f);
    const unsigned char bg = static_cast<unsigned char>(bcol[1] * 255.0f);
    const unsigned char bb = static_cast<unsigned char>(bcol[2] * 255.0f);

    auto drawBorderPixel = [&](int col, int glRow) {
        if (col < 0 || col >= btnW || glRow < 0 || glRow >= btnH) return;
        const size_t idx = (static_cast<size_t>(glRow) * btnW + col) * 4;
        ov.pixbuf[idx + 0] = br;
        ov.pixbuf[idx + 1] = bg;
        ov.pixbuf[idx + 2] = bb;
        ov.pixbuf[idx + 3] = 255;
    };
    for (int c = 0; c < btnW; ++c) {
        drawBorderPixel(c, 0);
        drawBorderPixel(c, btnH - 1);
    }
    for (int r = 1; r < btnH - 1; ++r) {
        drawBorderPixel(0,        r);
        drawBorderPixel(btnW - 1, r);
    }

    // Draw centered text label
    const SbString & str  = btn->string.getValue();
    const float fontSize  = btn->fontSize.getValue();
    if (str.getLength() > 0 && fontSize > 0.0f) {
        SbFont font;
        font.setSize(fontSize);

        const SbColor & tcol = btn->color.getValue();
        const unsigned char cr = static_cast<unsigned char>(tcol[0] * 255.0f);
        const unsigned char cg = static_cast<unsigned char>(tcol[1] * 255.0f);
        const unsigned char cb_c = static_cast<unsigned char>(tcol[2] * 255.0f);

        int strWidth = 0;
        {
            const char * p = str.getString();
            while (*p) {
                const unsigned char ch2 = static_cast<unsigned char>(*p++);
                SbVec2s sz2, bearing2;
                font.getGlyphBitmap(ch2, sz2, bearing2);
                strWidth +=
                    static_cast<int>(font.getGlyphAdvance(ch2)[0]);
            }
        }

        const int lineH   = static_cast<int>(fontSize * kHUDLineHeightFactor) + 1;
        const int textX0  = (btnW - strWidth) / 2;
        const int textY0  = (btnH - lineH)   / 2;

        const char * p = str.getString();
        int xpen = textX0;
        while (*p) {
            const unsigned char ch = static_cast<unsigned char>(*p++);
            SbVec2s sz, bearing;
            const unsigned char * bitmap = font.getGlyphBitmap(ch, sz, bearing);
            const SbVec2f adv = font.getGlyphAdvance(ch);

            if (bitmap && sz[0] > 0 && sz[1] > 0) {
                const int baseline  = lineH - static_cast<int>(fontSize);
                const int glyph_top = baseline - bearing[1];

                for (int gy = 0; gy < sz[1]; ++gy) {
                    const int text_td_row = glyph_top + gy;
                    const int btn_gl_row  = textY0 + (lineH - 1 - text_td_row);
                    if (btn_gl_row < 0 || btn_gl_row >= btnH) continue;

                    for (int gx = 0; gx < sz[0]; ++gx) {
                        const int btn_col = xpen + bearing[0] + gx;
                        if (btn_col < 0 || btn_col >= btnW) continue;
                        const unsigned char alpha =
                            bitmap[gy * sz[0] + gx];
                        if (alpha == 0) continue;
                        const size_t idx =
                            (static_cast<size_t>(btn_gl_row) * btnW +
                             btn_col) * 4;
                        ov.pixbuf[idx + 0] = cr;
                        ov.pixbuf[idx + 1] = cg;
                        ov.pixbuf[idx + 2] = cb_c;
                        ov.pixbuf[idx + 3] = alpha;
                    }
                }
            }
            xpen += static_cast<int>(adv[0]);
        }
    }

    cbdata->overlays->push_back(std::move(ov));
    return SoCallbackAction::PRUNE;
}

SoCallbackAction::Response
src_directionalLightCB(void * ud,
                       SoCallbackAction * action,
                       const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoDirectionalLight * dl =
        static_cast<const SoDirectionalLight *>(node);
    if (!dl->on.getValue())
        return SoCallbackAction::CONTINUE;

    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wDir;
    mm.multDirMatrix(dl->direction.getValue(), wDir);
    float wlen = wDir.length();
    if (wlen > 1e-6f) wDir /= wlen;

    SoScLightInfo li;
    li.type      = SO_SC_DIRECTIONAL;
    li.dir[0]    = wDir[0]; li.dir[1] = wDir[1]; li.dir[2] = wDir[2];
    li.pos[0]    = li.pos[1] = li.pos[2] = 0.0f;
    const SbColor & c = dl->color.getValue();
    li.rgb[0]    = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity    = dl->intensity.getValue();
    li.cutOffAngle  = 0.0f;
    li.dropOffRate  = 0.0f;
    cbdata->lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}

SoCallbackAction::Response
src_pointLightCB(void * ud,
                 SoCallbackAction * action,
                 const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoPointLight * pl =
        static_cast<const SoPointLight *>(node);
    if (!pl->on.getValue())
        return SoCallbackAction::CONTINUE;

    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wPos;
    mm.multVecMatrix(pl->location.getValue(), wPos);

    SoScLightInfo li;
    li.type      = SO_SC_POINT;
    li.dir[0]    = li.dir[1] = li.dir[2] = 0.0f;
    li.pos[0]    = wPos[0]; li.pos[1] = wPos[1]; li.pos[2] = wPos[2];
    const SbColor & c = pl->color.getValue();
    li.rgb[0]    = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity    = pl->intensity.getValue();
    li.cutOffAngle  = 0.0f;
    li.dropOffRate  = 0.0f;
    cbdata->lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}

SoCallbackAction::Response
src_spotLightCB(void * ud,
                SoCallbackAction * action,
                const SoNode * node)
{
    ScRaytracerCbData * cbdata = static_cast<ScRaytracerCbData *>(ud);
    const SoSpotLight * sl =
        static_cast<const SoSpotLight *>(node);
    if (!sl->on.getValue())
        return SoCallbackAction::CONTINUE;

    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wPos, wDir;
    mm.multVecMatrix(sl->location.getValue(),  wPos);
    mm.multDirMatrix(sl->direction.getValue(), wDir);
    float wlen = wDir.length();
    if (wlen > 1e-6f) wDir /= wlen;

    SoScLightInfo li;
    li.type         = SO_SC_SPOT;
    li.dir[0]       = wDir[0]; li.dir[1] = wDir[1]; li.dir[2] = wDir[2];
    li.pos[0]       = wPos[0]; li.pos[1] = wPos[1]; li.pos[2] = wPos[2];
    const SbColor & c = sl->color.getValue();
    li.rgb[0]       = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity    = sl->intensity.getValue();
    li.cutOffAngle  = sl->cutOffAngle.getValue();
    li.dropOffRate  = sl->dropOffRate.getValue();
    cbdata->lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}
