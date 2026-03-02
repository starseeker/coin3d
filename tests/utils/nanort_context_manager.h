/*
 * nanort_context_manager.h
 *
 * SoNanoRTContextManager – a SoDB::ContextManager specialization that uses
 * nanort for CPU ray-triangle intersection instead of OpenGL.
 *
 * This is the reference implementation of the generalized context-manager
 * rendering path introduced in SoDB::ContextManager::renderScene().  By
 * providing this as the context manager at SoDB::init() time, the entire
 * SoOffscreenRenderer pipeline (render() → getBuffer() → writeToRGB()) works
 * without any OpenGL context.
 *
 * Usage:
 *
 *   #include "nanort_context_manager.h"
 *
 *   static SoNanoRTContextManager nrt_mgr;
 *   SoDB::init(&nrt_mgr);
 *   SoNodeKit::init();
 *   SoInteraction::init();
 *
 *   // ... build scene graph ...
 *
 *   SbViewportRegion vp(800, 600);
 *   SoOffscreenRenderer renderer(vp);
 *   renderer.setComponents(SoOffscreenRenderer::RGB);
 *   renderer.render(root);          // calls nrt_mgr.renderScene() internally
 *   renderer.writeToRGB("out.rgb"); // writes the raytraced pixels
 *
 * Obol APIs used internally:
 *   - SoCallbackAction  – extract triangles, normals, materials from scene graph
 *   - SoSearchAction    – find camera, lights, SoRaytracingParams in scene graph
 *   - SbViewportRegion  – viewport for camera setup
 *   - SoCamera          – get view volume for ray generation
 *   - SbViewVolume      – projectPointToLine() for per-pixel ray directions
 *   - SbMatrix          – transform vertices/normals/positions to world space
 *   - SoRaytracingParams – rendering hints (shadows, reflections, AA, ambient)
 *
 * Rendering features implemented:
 *   - Perspective and orthographic cameras (via SoCamera / SbViewVolume)
 *   - Directional lights (SoDirectionalLight) – Phong diffuse + specular
 *   - Point lights (SoPointLight) – positional with inverse-square attenuation
 *   - Spot lights (SoSpotLight) – cone-limited positional lights
 *   - SoMaterial: diffuse, specular, emissive, ambient, shininess
 *   - All triangle-generating shapes (SoSphere, SoCube, SoCone, SoCylinder,
 *     SoFaceSet, SoIndexedFaceSet, SoVertexProperty, …)
 *   - Any shape-local or per-face material bindings
 *   - All rigid-body transforms (SoTranslation, SoRotation, SoScale, …)
 *   - SoSeparator / SoGroup scene-graph structure
 *   - SoRaytracingParams: hard shadows, specular reflections, AA super-sampling,
 *     configurable ambient fill intensity
 *
 * Rendering features NOT supported (fall back to rendering nothing extra):
 *   - Textures (texture images not applied)
 *   - SoGLRenderAction-specific effects (shadow maps, FBOs, GLSL shaders)
 *   - SoPath rendering (only SoNode* root is handled; SoPath falls through
 *     to GL unless an SoNode is available)
 *
 * Rendering features with proxy-geometry fallback:
 *   - SoLineSet, SoIndexedLineSet – rendered as thin cylinders via
 *     SoLineSet::createCylinderProxy() / SoIndexedLineSet::createCylinderProxy()
 *   - SoPointSet – rendered as small spheres via SoPointSet::createSphereProxy()
 *   - SoCylinder with DrawStyle LINES – rendered as a ring of thin facetized
 *     cylinders (nrtCreateRingProxy) to match the OpenGL wireframe ring
 *     appearance used by rotational draggers / manipulators
 *   - Shapes with DrawStyle INVISIBLE – pruned from the nanort scene so that
 *     picking-only geometry (e.g. the sphere inside SoRotateSphericalDragger)
 *     is not rendered as solid geometry
 *   - SoText3 – uses generatePrimitives() which produces extruded triangle
 *     geometry directly; no extra work needed
 *   - SoText2 – screen-aligned text rendered as one screen-aligned quad per
 *     visible glyph via SoText2::buildGlyphQuads(); each quad matches the
 *     pixel footprint of the glyph bitmap at the text anchor depth, with the
 *     per-line justification offsets applied; whitespace chars are skipped
 *   - SoHUDLabel – HUD overlay text labels (inside SoHUDKit) are rasterized
 *     via SbFont/stb_truetype and alpha-composited onto the final framebuffer,
 *     giving the same HUD text appearance without any OpenGL context
 *   - SoHUDButton – HUD button border rectangle (1-pixel GL_LINE_LOOP equivalent)
 *     is drawn directly into the image buffer using borderColor, and the button
 *     label text is rasterized via SbFont and composited onto the framebuffer
 *
 * Dependencies:
 *   - nanort.h (external/nanort/nanort.h)
 *   - stb_image_write.h is NOT needed here (pixel writing is done by Coin)
 */

#ifndef OBOL_NANORT_CONTEXT_MANAGER_H
#define OBOL_NANORT_CONTEXT_MANAGER_H

// ---- Obol/Coin includes ---------------------------------------------------
#include <Inventor/SoDB.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbViewVolume.h>
#include <Inventor/SbVec2f.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbColor.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSpotLight.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/SbRotation.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/annex/HUD/nodes/SoHUDLabel.h>
#include <Inventor/annex/HUD/nodes/SoHUDButton.h>
#include <Inventor/SbFont.h>
#include <Inventor/nodes/SoMatrixTransform.h>
#include <Inventor/nodes/SoRaytracingParams.h>
#include <Inventor/elements/SoCoordinateElement.h>
#include <Inventor/elements/SoLineWidthElement.h>
#include <Inventor/elements/SoPointSizeElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/SoPath.h>

// ---- nanort --------------------------------------------------------------
#include "nanort.h"

// ---- Standard library ----------------------------------------------------
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <algorithm>

// ==========================================================================
// Internal scene-data structures
// ==========================================================================

struct NrtMaterial {
    float diffuse[3];   // RGB [0,1]
    float specular[3];  // RGB [0,1]
    float ambient[3];   // RGB [0,1]
    float emission[3];  // RGB [0,1]
    float shininess;    // [0,1] – multiply by 128 for Phong exponent
};

struct NrtTriangle {
    float pos[3][3];    // world-space positions  [vertex 0/1/2][x/y/z]
    float norm[3][3];   // world-space normals    [vertex 0/1/2][x/y/z]
    NrtMaterial mat;
};

struct NrtSceneCollector {
    std::vector<NrtTriangle> tris;
};

// ==========================================================================
// Triangle callback – called by SoCallbackAction for every triangle
// ==========================================================================

static void nrtTriangleCB(void * userdata,
                          SoCallbackAction * action,
                          const SoPrimitiveVertex * v0,
                          const SoPrimitiveVertex * v1,
                          const SoPrimitiveVertex * v2)
{
    NrtSceneCollector * col = static_cast<NrtSceneCollector *>(userdata);

    const SbMatrix & mm = action->getModelMatrix();
    SbMatrix normalMat   = mm.inverse().transpose();

    NrtTriangle tri;
    const SoPrimitiveVertex * verts[3] = { v0, v1, v2 };
    for (int i = 0; i < 3; ++i) {
        SbVec3f wp, wn;
        mm.multVecMatrix(verts[i]->getPoint(),   wp);
        normalMat.multDirMatrix(verts[i]->getNormal(), wn);
        wn.normalize();
        tri.pos[i][0]  = wp[0];  tri.pos[i][1]  = wp[1];  tri.pos[i][2]  = wp[2];
        tri.norm[i][0] = wn[0];  tri.norm[i][1] = wn[1];  tri.norm[i][2] = wn[2];
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

    col->tris.push_back(tri);
}

// ==========================================================================
// Proxy-geometry callbacks – fired for shape types that produce no triangles
// ==========================================================================
// These pre-callbacks intercept SoLineSet, SoIndexedLineSet and SoPointSet
// nodes during SoCallbackAction traversal and substitute rendered proxy
// geometry (cylinders for lines, spheres for points) so that ray-tracing
// backends can visualise them.

// Per-SoText2-node pixel overlay collected during scene traversal.
// After ray-tracing, these are composited directly onto the framebuffer.
struct NrtTextOverlay {
    std::vector<unsigned char> pixbuf; // RGBA, bottom-to-top rows
    int x, y, w, h;                   // viewport-space bottom-left + size
};

struct NrtProxyData {
    NrtSceneCollector *          collector;
    SbViewportRegion             vp;
    std::vector<NrtTextOverlay>  textOverlays; // collected SoText2 bitmaps
};

// Helper: wrap \a proxy in a separator that injects the current material and
// model matrix from \a action, then collect its triangles into \a collector.
static void nrtCollectProxy(SoCallbackAction * action,
                            NrtProxyData * data,
                            SoSeparator * proxy)
{
    // Transfer the material currently on the traversal state.
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

    // Apply the current model matrix so the proxy ends up in world space.
    SoMatrixTransform * mt = new SoMatrixTransform;
    mt->matrix.setValue(action->getModelMatrix());

    SoSeparator * wrapper = new SoSeparator;
    wrapper->ref();
    wrapper->addChild(mat);
    wrapper->addChild(mt);
    wrapper->addChild(proxy);

    SoCallbackAction subCba(data->vp);
    subCba.addTriangleCallback(SoShape::getClassTypeId(),
                               nrtTriangleCB, data->collector);
    subCba.apply(wrapper);

    wrapper->unref();
}

// Compute the world-space radius for a line or point given the current state.
static float nrtLineWorldRadius(SoCallbackAction * action,
                                float sizePx,
                                float viewportHeightPx)
{
    const SbViewVolume & vv =
        SoViewVolumeElement::get(action->getState());
    float worldHeight = vv.getHeight();
    // For perspective cameras vv.getHeight() is the frustum height at the
    // near plane, which is typically very small.  Scale to the viewing
    // distance of the current object so that the proxy radius corresponds
    // to sizePx pixels at that depth.
    if (vv.getProjectionType() == SbViewVolume::PERSPECTIVE) {
        const float nearDist = vv.getNearDist();
        if (nearDist > 1e-6f) {
            // Estimate the object's world-space position from the model
            // matrix translation column, then measure its distance from
            // the camera projection point.
            const SbMatrix & mm = action->getModelMatrix();
            const SbVec3f objPos(mm[3][0], mm[3][1], mm[3][2]);
            const float dist = (objPos - vv.getProjectionPoint()).length();
            const float refDist = (dist > nearDist) ? dist : nearDist;
            worldHeight = worldHeight * refDist / nearDist;
        }
    }
    return sizePx * worldHeight / viewportHeightPx * 0.5f;
}

static SoCallbackAction::Response
nrtLineSetPreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoLineSet * ls = static_cast<const SoLineSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    // SoLineWidthElement default is 0 (meaning "use GL default of 1px")
    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH    = static_cast<float>(
        data->vp.getViewportSizePixels()[1]);
    const float radius = nrtLineWorldRadius(action, lineW, vpH);

    SoSeparator * proxy = ls->createCylinderProxy(coords, radius);
    proxy->ref();
    nrtCollectProxy(action, data, proxy);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

static SoCallbackAction::Response
nrtIndexedLineSetPreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoIndexedLineSet * ils =
        static_cast<const SoIndexedLineSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH    = static_cast<float>(
        data->vp.getViewportSizePixels()[1]);
    const float radius = nrtLineWorldRadius(action, lineW, vpH);

    SoSeparator * proxy = ils->createCylinderProxy(coords, radius);
    proxy->ref();
    nrtCollectProxy(action, data, proxy);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

static SoCallbackAction::Response
nrtPointSetPreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoPointSet * ps = static_cast<const SoPointSet *>(node);

    const SoCoordinateElement * coords =
        SoCoordinateElement::getInstance(action->getState());
    if (!coords || coords->getNum() == 0)
        return SoCallbackAction::CONTINUE;

    // SoPointSizeElement default is 0 (meaning "use GL default of 1px")
    float ptSz = SoPointSizeElement::get(action->getState());
    if (ptSz <= 0.0f) ptSz = 1.0f;
    const float vpH    = static_cast<float>(
        data->vp.getViewportSizePixels()[1]);
    const float radius = nrtLineWorldRadius(action, ptSz, vpH);

    SoSeparator * proxy = ps->createSphereProxy(coords, radius);
    proxy->ref();
    nrtCollectProxy(action, data, proxy);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

// ==========================================================================
// SoShape pre-callback: skip invisible shapes
// ==========================================================================
// Shapes marked with DrawStyle { style INVISIBLE } (e.g. the picking sphere
// in SoRotateSphericalDragger) must not contribute triangles to the nanort
// scene.  This pre-callback prunes any SoShape whose current draw style is
// INVISIBLE so that nrtTriangleCB is never invoked for it.
static SoCallbackAction::Response
nrtShapePreCB(void * /*ud*/, SoCallbackAction * action, const SoNode *)
{
    if (action->getDrawStyle() == SoDrawStyle::INVISIBLE)
        return SoCallbackAction::PRUNE;
    return SoCallbackAction::CONTINUE;
}

// ==========================================================================
// SoCylinder pre-callback: ring proxy for DrawStyle LINES cylinders
// ==========================================================================
// In OpenGL, SoCylinder with DrawStyle { style LINES } renders as a wireframe
// ring (circular outline).  nanort has no concept of wireframe rendering, so
// instead we build a series of thin cylindrical tube segments arranged around
// the circumference to produce the same ring appearance.  This matches the
// visual style of rotational manipulators (SoRotateSphericalDragger,
// SoTrackballDragger, SoCenterballDragger) in the OpenGL panel.
static SoSeparator *
nrtCreateRingProxy(float cylRadius, float tubeRadius, int numSegments = 32)
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

static SoCallbackAction::Response
nrtCylinderPreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    if (action->getDrawStyle() != SoDrawStyle::LINES)
        return SoCallbackAction::CONTINUE;

    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoCylinder * cyl = static_cast<const SoCylinder *>(node);

    float lineW = SoLineWidthElement::get(action->getState());
    if (lineW <= 0.0f) lineW = 1.0f;
    const float vpH      = static_cast<float>(data->vp.getViewportSizePixels()[1]);
    const float tubeRad  = nrtLineWorldRadius(action, lineW, vpH);
    const float cylRad   = cyl->radius.getValue();

    SoSeparator * proxy = nrtCreateRingProxy(cylRad, tubeRad);
    proxy->ref();
    nrtCollectProxy(action, data, proxy);
    proxy->unref();

    return SoCallbackAction::PRUNE;
}

// ==========================================================================
// SoText2 pre-callback: per-glyph billboard quads
// ==========================================================================
// SoText2 renders screen-aligned text via GL rasterisation, so its
// generatePrimitives() emits nothing for ray-tracing backends.  This
// pre-callback intercepts each SoText2 node and builds a crisp RGBA pixel
// buffer via SoText2::buildPixelBuffer().  The overlay is stored for
// compositing directly onto the framebuffer after the ray-trace pass,
// which gives letter-accurate glyph shapes (not bounding-box blocks).
static SoCallbackAction::Response
nrtText2PreCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoText2 * text = static_cast<const SoText2 *>(node);
    SoState * state = action->getState();

    NrtTextOverlay ov;
    if (!text->buildPixelBuffer(state, ov.pixbuf, ov.x, ov.y, ov.w, ov.h))
        return SoCallbackAction::PRUNE;

    data->textOverlays.push_back(std::move(ov));
    return SoCallbackAction::PRUNE;  // generatePrimitives() produces nothing
}

// ==========================================================================
// SoHUDLabel pre-callback: HUD overlay text via SbFont/stb_truetype
// ==========================================================================
// SoHUDLabel renders text at a fixed pixel position using the pixel-space
// orthographic projection set up by SoHUDKit.  During SoCallbackAction
// traversal there is no GL state to call GLRender, so this pre-callback
// reads the label's fields directly and rasterizes the text using SbFont
// (the same stb_truetype path used by stt_reference).  The resulting RGBA
// pixel buffer is stored as an NrtTextOverlay for compositing after the
// ray-trace pass.

// Approximate line height factor: ascender + descender + leading, expressed
// as a multiplier of fontSize.  Consistent with stb_truetype's default metrics.
static const float kNrtHUDLineHeightFactor = 1.3f;

static SoCallbackAction::Response
nrtHUDLabelPreCB(void * ud, SoCallbackAction * /*action*/, const SoNode * node)
{
    NrtProxyData * data  = static_cast<NrtProxyData *>(ud);
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

    // Approximate line height (ascender + descender + a small leading).
    // stb_truetype scales with fontSize; see kNrtHUDLineHeightFactor.
    const int lineH = static_cast<int>(fontSize * kNrtHUDLineHeightFactor) + 1;

    // ------------------------------------------------------------------
    // Pass 1: compute per-line pixel widths and overall canvas dimensions.
    // ------------------------------------------------------------------
    std::vector<int> lineWidths(nlines, 0);
    int maxWidth = 0;
    for (int li = 0; li < nlines; ++li) {
        const char * p = label->string[li].getString();
        int x = 0;
        while (*p) {
            const unsigned char ch = static_cast<unsigned char>(*p++);
            SbVec2s sz, bearing;
            font.getGlyphBitmap(ch, sz, bearing);
            const SbVec2f adv = font.getGlyphAdvance(ch);
            x += static_cast<int>(adv[0]);
        }
        lineWidths[li] = x;
        if (x > maxWidth) maxWidth = x;
    }
    if (maxWidth <= 0) return SoCallbackAction::PRUNE;

    const int canvasW = maxWidth;
    const int canvasH = nlines * lineH;

    // ------------------------------------------------------------------
    // Allocate RGBA canvas (transparent black), stored bottom-to-top to
    // match the GL/nanort framebuffer convention.
    // ------------------------------------------------------------------
    NrtTextOverlay ov;
    ov.w = canvasW;
    ov.h = canvasH;
    ov.pixbuf.assign(static_cast<size_t>(canvasW) * canvasH * 4, 0);

    // Anchor position: label->position gives the baseline of the first line
    // in viewport pixels from the lower-left.  The canvas is placed so that
    // the first line's baseline (at canvas GL row canvasH - 1 - (int)fontSize)
    // lands exactly at pos[1] in the framebuffer.
    const SbVec2f pos = label->position.getValue();
    ov.x = static_cast<int>(pos[0]);
    ov.y = static_cast<int>(pos[1]) - (canvasH - 1 - static_cast<int>(fontSize));

    // ------------------------------------------------------------------
    // Pass 2: rasterize each line into the canvas.
    // Line 0 (string[0]) is at the top of the canvas, line (nlines-1) at
    // the bottom.  Within the canvas, row 0 is the GL bottom row.
    // ------------------------------------------------------------------
    const int just = label->justification.getValue();
    for (int li = 0; li < nlines; ++li) {
        // Top of this line in top-down canvas coordinates.
        // Line 0 starts at row 0 (top); each subsequent line is lineH lower.
        const int lineTopInCanvas = li * lineH;

        // Horizontal start offset for justification.
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
                // bearing[1] = pixels above baseline (positive = up).
                // The baseline sits fontSize rows below the top of each line
                // area, leaving fontSize rows for ascenders and
                // (lineH - fontSize - 1) rows for descenders/leading.
                const int baseline = lineTopInCanvas + static_cast<int>(fontSize);
                const int dst_x = xpen + bearing[0];
                const int dst_y = baseline - bearing[1];  // top of glyph in canvas

                for (int gy = 0; gy < sz[1]; ++gy) {
                    // Canvas row for this glyph row (top-down glyph, bottom-up canvas).
                    const int canvas_row = dst_y + gy;
                    if (canvas_row < 0 || canvas_row >= canvasH) continue;
                    // Flip to GL bottom-up convention.
                    const int gl_row = canvasH - 1 - canvas_row;

                    for (int gx = 0; gx < sz[0]; ++gx) {
                        const int canvas_col = dst_x + gx;
                        if (canvas_col < 0 || canvas_col >= canvasW) continue;
                        const unsigned char alpha = bitmap[gy * sz[0] + gx];
                        if (alpha == 0) continue;
                        const size_t idx =
                            (static_cast<size_t>(gl_row) * canvasW + canvas_col) * 4;
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

    data->textOverlays.push_back(std::move(ov));
    return SoCallbackAction::PRUNE;
}

// ==========================================================================
// SoHUDButton pre-callback: border rectangle + centered label via SbFont
// ==========================================================================
// SoHUDButton renders a rectangular border (GL_LINE_LOOP) and a centred text
// label during GLRender.  In the nanort path there is no GL state, so this
// callback does both steps in software:
//   1. Allocates an RGBA canvas the full pixel size of the button.
//   2. Draws a 1-pixel border rectangle using borderColor.
//   3. Rasterizes the centered label using SbFont / stb_truetype.
// The combined canvas is stored as an NrtTextOverlay and alpha-composited
// onto the framebuffer after ray-tracing — giving a complete button appearance.
static SoCallbackAction::Response
nrtHUDButtonPreCB(void * ud, SoCallbackAction * /*action*/, const SoNode * node)
{
    NrtProxyData * data = static_cast<NrtProxyData *>(ud);
    const SoHUDButton * btn = static_cast<const SoHUDButton *>(node);

    const SbVec2f pos  = btn->position.getValue();
    const SbVec2f size = btn->size.getValue();
    const int btnW = static_cast<int>(size[0]);
    const int btnH = static_cast<int>(size[1]);
    if (btnW <= 0 || btnH <= 0) return SoCallbackAction::PRUNE;

    // Canvas covers the full button extent (GL bottom-up row convention).
    NrtTextOverlay ov;
    ov.w = btnW;
    ov.h = btnH;
    ov.x = static_cast<int>(pos[0]);
    ov.y = static_cast<int>(pos[1]);
    ov.pixbuf.assign(static_cast<size_t>(btnW) * btnH * 4, 0);

    // --- 1. Draw 1-pixel border rectangle using borderColor -----------------
    const SbColor & bcol = btn->borderColor.getValue();
    const unsigned char br = static_cast<unsigned char>(bcol[0] * 255.0f);
    const unsigned char bg = static_cast<unsigned char>(bcol[1] * 255.0f);
    const unsigned char bb = static_cast<unsigned char>(bcol[2] * 255.0f);

    // Inline helper: write a fully-opaque border pixel at (col, glRow) in the canvas.
    auto drawBorderPixel = [&](int col, int glRow) {
        if (col < 0 || col >= btnW || glRow < 0 || glRow >= btnH) return;
        const size_t idx = (static_cast<size_t>(glRow) * btnW + col) * 4;
        ov.pixbuf[idx + 0] = br;
        ov.pixbuf[idx + 1] = bg;
        ov.pixbuf[idx + 2] = bb;
        ov.pixbuf[idx + 3] = 255;
    };
    // Bottom edge (GL row 0) and top edge (GL row btnH-1)
    for (int c = 0; c < btnW; ++c) {
        drawBorderPixel(c, 0);
        drawBorderPixel(c, btnH - 1);
    }
    // Left edge (col 0) and right edge (col btnW-1)
    for (int r = 1; r < btnH - 1; ++r) {
        drawBorderPixel(0,        r);
        drawBorderPixel(btnW - 1, r);
    }

    // --- 2. Draw centered text label using color ----------------------------
    const SbString & str  = btn->string.getValue();
    const float fontSize  = btn->fontSize.getValue();
    if (str.getLength() > 0 && fontSize > 0.0f) {
        SbFont font;
        font.setSize(fontSize);

        const SbColor & tcol = btn->color.getValue();
        const unsigned char cr = static_cast<unsigned char>(tcol[0] * 255.0f);
        const unsigned char cg = static_cast<unsigned char>(tcol[1] * 255.0f);
        const unsigned char cb = static_cast<unsigned char>(tcol[2] * 255.0f);

        // Measure string width for horizontal centering.
        int strWidth = 0;
        {
            const char * p = str.getString();
            while (*p) {
                const unsigned char ch2 = static_cast<unsigned char>(*p++);
                SbVec2s sz2, bearing2;
                font.getGlyphBitmap(ch2, sz2, bearing2);
                strWidth += static_cast<int>(font.getGlyphAdvance(ch2)[0]);
            }
        }

        const int lineH = static_cast<int>(fontSize * kNrtHUDLineHeightFactor) + 1;

        // Top-left origin of the text canvas within the button canvas.
        // textX0: horizontal offset so the string is centred.
        // textY0: GL bottom-up row in the button canvas where the text bottom sits.
        const int textX0 = (btnW - strWidth) / 2;
        const int textY0 = (btnH - lineH)   / 2;  // GL row of text bottom in button

        const char * p = str.getString();
        int xpen = textX0;
        while (*p) {
            const unsigned char ch = static_cast<unsigned char>(*p++);
            SbVec2s sz, bearing;
            const unsigned char * bitmap = font.getGlyphBitmap(ch, sz, bearing);
            const SbVec2f adv = font.getGlyphAdvance(ch);

            if (bitmap && sz[0] > 0 && sz[1] > 0) {
                // Within the text canvas (lineH rows, top-down):
                //   baseline top-down row = lineH - fontSize
                //   glyph top top-down row = baseline - bearing[1]
                const int baseline   = lineH - static_cast<int>(fontSize);
                const int glyph_top  = baseline - bearing[1];  // top-down

                for (int gy = 0; gy < sz[1]; ++gy) {
                    // Top-down row in text canvas → GL bottom-up row in button canvas
                    const int text_td_row   = glyph_top + gy;
                    const int btn_gl_row    = textY0 + (lineH - 1 - text_td_row);
                    if (btn_gl_row < 0 || btn_gl_row >= btnH) continue;

                    for (int gx = 0; gx < sz[0]; ++gx) {
                        const int btn_col = xpen + bearing[0] + gx;
                        if (btn_col < 0 || btn_col >= btnW) continue;
                        const unsigned char alpha = bitmap[gy * sz[0] + gx];
                        if (alpha == 0) continue;
                        const size_t idx =
                            (static_cast<size_t>(btn_gl_row) * btnW + btn_col) * 4;
                        // Overwrite border pixels that happen to coincide with text
                        // (unlikely given centred text vs 1-pixel rim, but safe).
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

    data->textOverlays.push_back(std::move(ov));
    return SoCallbackAction::PRUNE;
}


enum NrtLightType { NRT_DIRECTIONAL, NRT_POINT, NRT_SPOT };

struct NrtLightInfo {
    NrtLightType type;
    float rgb[3];       // light colour
    float intensity;    // intensity scale
    // Directional lights use dir only (points away from light source).
    // Point lights use pos only.
    // Spot lights use both pos and dir (dir = cone axis direction).
    float dir[3];       // world-space direction (DIRECTIONAL: away from light; SPOT: cone axis)
    float pos[3];       // world-space position (POINT, SPOT)
    float cutOffAngle;  // spot cone half-angle in radians (SPOT only)
    float dropOffRate;  // spot intensity drop-off exponent (SPOT only)
};

// ==========================================================================
// Light collection pre-callbacks (registered on the main SoCallbackAction)
// ==========================================================================

static SoCallbackAction::Response
nrtDirectionalLightCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    std::vector<NrtLightInfo> * lights =
        static_cast<std::vector<NrtLightInfo> *>(ud);
    const SoDirectionalLight * dl =
        static_cast<const SoDirectionalLight *>(node);
    if (!dl->on.getValue())
        return SoCallbackAction::CONTINUE;

    // Transform direction to world space using the model matrix at
    // the time of traversal.
    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wDir;
    mm.multDirMatrix(dl->direction.getValue(), wDir);
    // Normalize defensively; the world may have a scaling transform.
    float wlen = wDir.length();
    if (wlen > 1e-6f) wDir /= wlen;

    NrtLightInfo li;
    li.type = NRT_DIRECTIONAL;
    li.dir[0] = wDir[0]; li.dir[1] = wDir[1]; li.dir[2] = wDir[2];
    li.pos[0] = li.pos[1] = li.pos[2] = 0.0f;
    const SbColor & c = dl->color.getValue();
    li.rgb[0] = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity    = dl->intensity.getValue();
    li.cutOffAngle  = 0.0f;
    li.dropOffRate  = 0.0f;
    lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}

static SoCallbackAction::Response
nrtPointLightCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    std::vector<NrtLightInfo> * lights =
        static_cast<std::vector<NrtLightInfo> *>(ud);
    const SoPointLight * pl =
        static_cast<const SoPointLight *>(node);
    if (!pl->on.getValue())
        return SoCallbackAction::CONTINUE;

    // Transform position to world space.
    const SbMatrix & mm = action->getModelMatrix();
    SbVec3f wPos;
    mm.multVecMatrix(pl->location.getValue(), wPos);

    NrtLightInfo li;
    li.type = NRT_POINT;
    li.dir[0] = li.dir[1] = li.dir[2] = 0.0f;
    li.pos[0] = wPos[0]; li.pos[1] = wPos[1]; li.pos[2] = wPos[2];
    const SbColor & c = pl->color.getValue();
    li.rgb[0] = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity   = pl->intensity.getValue();
    li.cutOffAngle = 0.0f;
    li.dropOffRate = 0.0f;
    lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}

static SoCallbackAction::Response
nrtSpotLightCB(void * ud, SoCallbackAction * action, const SoNode * node)
{
    std::vector<NrtLightInfo> * lights =
        static_cast<std::vector<NrtLightInfo> *>(ud);
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

    NrtLightInfo li;
    li.type = NRT_SPOT;
    li.dir[0] = wDir[0]; li.dir[1] = wDir[1]; li.dir[2] = wDir[2];
    li.pos[0] = wPos[0]; li.pos[1] = wPos[1]; li.pos[2] = wPos[2];
    const SbColor & c = sl->color.getValue();
    li.rgb[0] = c[0]; li.rgb[1] = c[1]; li.rgb[2] = c[2];
    li.intensity   = sl->intensity.getValue();
    li.cutOffAngle = sl->cutOffAngle.getValue();
    li.dropOffRate = sl->dropOffRate.getValue();
    lights->push_back(li);
    return SoCallbackAction::CONTINUE;
}



struct NrtScene {
    std::vector<float>        vertices; // 9 floats/triangle (3 verts × xyz)
    std::vector<unsigned int> faces;    // 3 indices/triangle (sequential)
    std::vector<float>        normals;  // 9 floats/triangle
    std::vector<NrtTriangle>  tris;     // source data (for shading)
    nanort::BVHAccel<float>   accel;

    bool build()
    {
        const size_t n = tris.size();
        if (n == 0) return false;

        vertices.resize(n * 9);
        normals.resize(n * 9);
        faces.resize(n * 3);

        for (size_t i = 0; i < n; ++i) {
            for (int v = 0; v < 3; ++v) {
                const size_t base = 9 * i + 3 * v;
                vertices[base + 0] = tris[i].pos[v][0];
                vertices[base + 1] = tris[i].pos[v][1];
                vertices[base + 2] = tris[i].pos[v][2];
                normals[base + 0]  = tris[i].norm[v][0];
                normals[base + 1]  = tris[i].norm[v][1];
                normals[base + 2]  = tris[i].norm[v][2];
                faces[3 * i + v]   = static_cast<unsigned int>(3 * i + v);
            }
        }

        nanort::BVHBuildOptions<float> opts;
        opts.cache_bbox = false;
        nanort::TriangleMesh<float> tmesh(vertices.data(), faces.data(),
                                          sizeof(float) * 3);
        nanort::TriangleSAHPred<float> tpred(vertices.data(), faces.data(),
                                              sizeof(float) * 3);
        return accel.Build(static_cast<unsigned int>(n), tmesh, tpred, opts);
    }
};

// ==========================================================================
// Math helpers
// ==========================================================================

static inline float nrt_clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
static inline float nrt_dot3(const float * a, const float * b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
static inline void nrt_normalize3(float v[3]) {
    const float len = std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 1e-6f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

// Minimal xorshift32 PRNG for AA jitter (no allocations, no state setup).
static inline uint32_t nrt_xorshift32(uint32_t s) {
    s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s;
}
static inline float nrt_rand01(uint32_t & s) {
    s = nrt_xorshift32(s);
    return static_cast<float>(s) * (1.0f / 4294967296.0f);
}

// Phong shading contribution from one light.
// N, V, L are unit vectors.  lightIntensity scales the light colour.
// out[3] is accumulated in-place (caller initializes and adds contribution).
static void nrt_phong(const float * N, const float * V, const float * L,
                      const NrtMaterial & mat,
                      const float * lightRGB, float lightIntensity,
                      float out[3])
{
    const float NdotL = nrt_clamp01(nrt_dot3(N, L));

    out[0] += mat.diffuse[0] * lightRGB[0] * lightIntensity * NdotL;
    out[1] += mat.diffuse[1] * lightRGB[1] * lightIntensity * NdotL;
    out[2] += mat.diffuse[2] * lightRGB[2] * lightIntensity * NdotL;

    const float specExp = mat.shininess * 128.0f;
    if (NdotL > 0.0f && specExp > 0.0f) {
        const float R[3] = {
            2.0f * NdotL * N[0] - L[0],
            2.0f * NdotL * N[1] - L[1],
            2.0f * NdotL * N[2] - L[2]
        };
        const float VdotR = nrt_clamp01(nrt_dot3(V, R));
        const float spec  = std::pow(VdotR, specExp);
        out[0] += mat.specular[0] * lightRGB[0] * lightIntensity * spec;
        out[1] += mat.specular[1] * lightRGB[1] * lightIntensity * spec;
        out[2] += mat.specular[2] * lightRGB[2] * lightIntensity * spec;
    }
}

// Test whether a shadow ray from hitPt (offset along N) toward direction L
// is blocked before reaching maxT.
// N      : surface normal at hitPt (used to offset origin away from surface)
// L      : unit vector toward the light
// maxT   : maximum intersection distance (1e30 for directional, finite for positional)
static bool nrt_is_shadowed(const NrtScene & scene,
                             const nanort::TriangleIntersector<float> & isect,
                             const float hitPt[3],
                             const float N[3],
                             const float L[3],
                             float maxT)
{
    nanort::Ray<float> shadowRay;
    // Offset origin along normal to avoid self-intersection.
    const float kEps = 1e-3f;
    shadowRay.org[0] = hitPt[0] + N[0] * kEps;
    shadowRay.org[1] = hitPt[1] + N[1] * kEps;
    shadowRay.org[2] = hitPt[2] + N[2] * kEps;
    shadowRay.dir[0] = L[0];
    shadowRay.dir[1] = L[1];
    shadowRay.dir[2] = L[2];
    shadowRay.min_t  = kEps;
    shadowRay.max_t  = maxT - kEps;

    nanort::TriangleIntersection<float> si;
    return scene.accel.Traverse(shadowRay, isect, &si);
}

// Shade a hit point given surface data and scene lights.
// hitPt  : world-space hit position
// N      : surface normal (unit, facing viewer)
// V      : unit view direction (camera → hit point, negated)
// mat    : surface material
// Returns the RGB contribution for this point (emission + ambient + lights).
static void nrt_shade(const NrtScene & scene,
                       const nanort::TriangleIntersector<float> & isect,
                       const float hitPt[3],
                       const float N[3],
                       const float V[3],
                       const NrtMaterial & mat,
                       const std::vector<NrtLightInfo> & lights,
                       float ambientFill,
                       bool shadowsEnabled,
                       float out[3])
{
    // Emission + ambient fill
    out[0] = mat.emission[0] + ambientFill * mat.ambient[0];
    out[1] = mat.emission[1] + ambientFill * mat.ambient[1];
    out[2] = mat.emission[2] + ambientFill * mat.ambient[2];

    for (const NrtLightInfo & li : lights) {
        float L[3];
        float attenuation = li.intensity;
        float shadowMaxT  = 1.0e30f;

        if (li.type == NRT_DIRECTIONAL) {
            // Direction points away from the light source; negate for 'toward light'.
            // Re-normalize: a scaling transform could have stretched the direction
            // before it was captured in nrtDirectionalLightCB.
            L[0] = -li.dir[0]; L[1] = -li.dir[1]; L[2] = -li.dir[2];
            nrt_normalize3(L);
            // Directional lights are infinitely far; no distance attenuation.

        } else if (li.type == NRT_POINT) {
            // Vector from hit point to light position.
            L[0] = li.pos[0] - hitPt[0];
            L[1] = li.pos[1] - hitPt[1];
            L[2] = li.pos[2] - hitPt[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;
            // Inverse-square attenuation (avoid div-by-zero for very close lights).
            const float d2 = dist * dist;
            attenuation = li.intensity / (1.0f + d2);
            shadowMaxT  = dist;

        } else { // NRT_SPOT
            L[0] = li.pos[0] - hitPt[0];
            L[1] = li.pos[1] - hitPt[1];
            L[2] = li.pos[2] - hitPt[2];
            const float dist = std::sqrt(L[0]*L[0] + L[1]*L[1] + L[2]*L[2]);
            if (dist < 1e-6f) continue;
            L[0] /= dist; L[1] /= dist; L[2] /= dist;

            // Spot cone check: dot product of cone axis (li.dir) with direction from
            // surface toward light (-L).  li.dir points from the light outward.
            const float cosHalfCone = std::cos(li.cutOffAngle);
            const float cosSurface  = -nrt_dot3(li.dir, L); // cos of angle to cone axis
            if (cosSurface < cosHalfCone)
                continue;  // outside the cone: no contribution

            // Smooth edge with drop-off
            const float spotFactor = (li.dropOffRate > 0.0f)
                ? std::pow(cosSurface, li.dropOffRate)
                : 1.0f;
            const float d2  = dist * dist;
            attenuation = li.intensity * spotFactor / (1.0f + d2);
            shadowMaxT  = dist;
        }

        // Shadow test (skip if shadowsEnabled is false)
        if (shadowsEnabled && nrt_is_shadowed(scene, isect, hitPt, N, L, shadowMaxT))
            continue;

        nrt_phong(N, V, L, mat, li.rgb, attenuation, out);
    }

    // Fallback if no lights: simple diffuse-from-eye
    if (lights.empty()) {
        const float NdotV = nrt_clamp01(nrt_dot3(N, V));
        out[0] = mat.diffuse[0] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[1] = mat.diffuse[1] * (ambientFill + (1.0f - ambientFill) * NdotV);
        out[2] = mat.diffuse[2] * (ambientFill + (1.0f - ambientFill) * NdotV);
    }
}

// Trace a ray through the scene and return the RGB colour (background or shaded hit).
// depth: number of additional reflection bounces allowed (0 = no reflections).
// bgColor: background colour to use for misses.
static void nrt_trace(const NrtScene & scene,
                       const nanort::TriangleIntersector<float> & intersector,
                       const float org[3], const float dir[3],
                       const std::vector<NrtLightInfo> & lights,
                       float ambientFill,
                       bool shadowsEnabled,
                       int depth,
                       const float bgColor[3],
                       float out[3])
{
    nanort::Ray<float> ray;
    ray.org[0] = org[0]; ray.org[1] = org[1]; ray.org[2] = org[2];
    ray.dir[0] = dir[0]; ray.dir[1] = dir[1]; ray.dir[2] = dir[2];
    ray.min_t  = 0.001f;
    ray.max_t  = 1.0e30f;

    nanort::TriangleIntersection<float> isect;
    if (!scene.accel.Traverse(ray, intersector, &isect)) {
        out[0] = bgColor[0]; out[1] = bgColor[1]; out[2] = bgColor[2];
        return;
    }

    const unsigned int fid = isect.prim_id;
    const float w0 = 1.0f - isect.u - isect.v;
    const float w1 = isect.u;
    const float w2 = isect.v;

    // Interpolated normal
    const float * n0 = scene.normals.data() + 9 * fid + 0;
    const float * n1 = scene.normals.data() + 9 * fid + 3;
    const float * n2 = scene.normals.data() + 9 * fid + 6;
    float N[3] = {
        w0*n0[0] + w1*n1[0] + w2*n2[0],
        w0*n0[1] + w1*n1[1] + w2*n2[1],
        w0*n0[2] + w1*n1[2] + w2*n2[2]
    };
    nrt_normalize3(N);

    // View direction (from hit toward camera)
    const float V[3] = { -dir[0], -dir[1], -dir[2] };
    // Do NOT flip the normal to face the viewer: GL uses one-sided lighting by
    // default (GL_LIGHT_MODEL_TWO_SIDE = GL_FALSE), so back-facing surfaces
    // (e.g. SoText3 BACK part viewed from the front) should appear unlit/dark,
    // not fully lit.  Flipping here would make back faces as bright as front
    // faces, which diverges from the GL reference render.

    // World-space hit position: p0 + t * dir
    const float hitPt[3] = {
        org[0] + dir[0] * isect.t,
        org[1] + dir[1] * isect.t,
        org[2] + dir[2] * isect.t
    };

    const NrtMaterial & mat = scene.tris[fid].mat;

    // Shade the direct illumination component
    nrt_shade(scene, intersector, hitPt, N, V, mat, lights,
              ambientFill, shadowsEnabled, out);

    // Reflection bounce (only when depth > 0 and surface is specular)
    if (depth > 0) {
        // Specular reflectance proxy: use the luminance of the specular colour
        // scaled by shininess.  Only trace if there is a meaningful contribution.
        const float specLum = (mat.specular[0] * 0.2126f +
                               mat.specular[1] * 0.7152f +
                               mat.specular[2] * 0.0722f) * mat.shininess;
        if (specLum > 0.01f) {
            // Reflection direction R = dir - 2*(N·dir)*N
            const float NdotI = nrt_dot3(N, dir);
            const float Rdir[3] = {
                dir[0] - 2.0f * NdotI * N[0],
                dir[1] - 2.0f * NdotI * N[1],
                dir[2] - 2.0f * NdotI * N[2]
            };
            // Offset origin along normal to avoid self-intersection
            const float kEps = 1e-3f;
            const float Rorg[3] = {
                hitPt[0] + N[0] * kEps,
                hitPt[1] + N[1] * kEps,
                hitPt[2] + N[2] * kEps
            };
            float reflColor[3];
            nrt_trace(scene, intersector, Rorg, Rdir, lights,
                      ambientFill, shadowsEnabled, depth - 1, bgColor,
                      reflColor);
            out[0] += mat.specular[0] * specLum * reflColor[0];
            out[1] += mat.specular[1] * specLum * reflColor[1];
            out[2] += mat.specular[2] * specLum * reflColor[2];
        }
    }
}

// ==========================================================================
// SoNanoRTContextManager
// ==========================================================================
//
// Geometry cache
// ──────────────
// Building the nanort BVH from scratch on every renderScene() call is
// expensive for scenes that contain many triangles – most notably scenes
// with manipulators/draggers whose ring proxies (nrtCylinderPreCB) each
// spawn a sub-callback-action traversal to collect ~32 faceted cylinder
// segments.  When the viewer calls renderScene() multiple times per frame
// (e.g. during the timedStepIn_ coarse-resolution calibration loop), or
// when only the camera changes between frames, rebuilding the BVH is
// entirely unnecessary.
//
// The cache works as follows:
//   • After a successful geometry traversal, the collected triangles,
//     the built BVH, the lights, and the SoRaytracingParams settings are
//     stored as the "cached scene".  The root node's getNodeId() value at
//     the time of the build is stored as cachedRootId_.
//   • Text/HUD overlays are always regenerated on every renderScene() call
//     (both cache hits and misses) because their screen-space pixel positions
//     depend on the current camera/viewport, which may change independently
//     of the scene geometry.
//   • On subsequent renderScene() calls with the same root node pointer:
//       – If root->getNodeId() == cachedRootId_, no node in the scene has
//         changed (Coin propagates field-change notifications up to the
//         root, bumping uniqueId at every ancestor including the root).
//         The full geometry traversal + BVH build are skipped; only a
//         lightweight text/HUD-overlay traversal is run.
//       – If root->getNodeId() != cachedRootId_, the scene has changed
//         (geometry, material, transform, or a light moved).  The full
//         traversal and BVH rebuild are performed and the cache is updated.
//         Text/HUD overlays are also collected during this traversal.
//   • Switching to a different root node pointer invalidates the cache.
//
// This gives a large speedup for the interactive viewer where:
//   1. timedStepIn_() calls doRender_() N times at increasing resolutions;
//      only the first call rebuilds the BVH; subsequent calls just raytrace.
//   2. Camera-only orbits (no geometry change) reuse the BVH.
//      Because the camera is a child of the scene root, any camera field
//      change (position, orientation) also bumps root->getNodeId() via
//      Coin's notification propagation.  A separate cachedCamPtr_ /
//      cachedCamId_ pair tracks the camera node independently so that a
//      root-nodeId change that is solely due to a camera move is not
//      mistaken for a geometry change requiring a BVH rebuild.
//   3. Static scenes (same root, no field changes) skip the traversal.
//
// Thread safety: SoNanoRTContextManager is not thread-safe; it is expected
// to be used from a single thread (the FLTK UI thread or the test process).

class SoNanoRTContextManager : public SoDB::ContextManager {
public:
    // -----------------------------------------------------------------------
    // GL context lifecycle – no-op (no GL needed for this renderer)
    // -----------------------------------------------------------------------
    void * createOffscreenContext(unsigned int, unsigned int) override
    { return nullptr; }

    SbBool makeContextCurrent(void *) override
    { return FALSE; }

    void restorePreviousContext(void *) override {}

    void destroyContext(void *) override {}

    // -----------------------------------------------------------------------
    // Rendering path
    // -----------------------------------------------------------------------
    SbBool renderScene(SoNode * scene,
                       unsigned int width, unsigned int height,
                       unsigned char * pixels,
                       unsigned int nrcomponents,
                       const float background_rgb[3]) override
    {
        if (!scene) return FALSE;

        const SbViewportRegion vp(static_cast<short>(width),
                                  static_cast<short>(height));

        // --- 1. Find the camera and determine whether geometry needs rebuild. ---
        // The camera is found early (before the rebuild check) so its nodeId
        // can be used to distinguish camera-only moves from geometry changes.
        // Coin propagates field-change notifications up through the scene
        // graph so that any ancestor (including the root) has its uniqueId
        // bumped whenever a descendant changes.  Because the camera is a
        // child of the root, any camera orbit or dolly also bumps the root's
        // nodeId.  To avoid a spurious BVH rebuild on every drag frame, we
        // check whether the root-nodeId change is solely due to the camera
        // moving (same camera node pointer, camera nodeId changed).  If so,
        // we skip the rebuild and just re-read the view volume below.
        // Proxy nodes created internally during traversal are standalone
        // (never parented to the scene root) and do not perturb the root's
        // nodeId between renders.
        SoCamera * cam = nullptr;
        {
            SoSearchAction sa;
            sa.setType(SoCamera::getClassTypeId());
            sa.setInterest(SoSearchAction::FIRST);
            sa.apply(scene);
            if (sa.getPath())
                cam = static_cast<SoCamera *>(sa.getPath()->getTail());
        }
        const SbUniqueId camId = cam ? cam->getNodeId() : 0;

        // A "camera-only move" is inferred when the same camera node (same
        // pointer, and that pointer has been recorded from a prior BVH build)
        // has a new nodeId but no other scene change is known.
        const bool cameraOnlyMoved =
            (cam != nullptr) &&
            (cachedCamPtr_ != nullptr) &&
            (cam == cachedCamPtr_) &&
            (camId != cachedCamId_);
        const bool needRebuild =
            (scene != cachedScene_) ||
            (scene->getNodeId() != cachedRootId_ && !cameraOnlyMoved);

        // Text/HUD overlay data – always regenerated so that screen-space
        // positions track the current camera even on cache hits.
        NrtProxyData proxyData;
        proxyData.vp = vp;

        if (needRebuild) {
            // --- 1a. Full traversal: geometry + lights + overlays -----------
            NrtSceneCollector collector;
            std::vector<NrtLightInfo> lights;
            proxyData.collector = &collector;

            {
                SoCallbackAction cba(vp);
                // Geometry
                cba.addTriangleCallback(SoShape::getClassTypeId(),
                                        nrtTriangleCB, &collector);
                // Skip shapes with DrawStyle INVISIBLE (e.g. picking spheres
                // in rotational draggers) so they don't appear as solid geo.
                cba.addPreCallback(SoShape::getClassTypeId(),
                                   nrtShapePreCB, nullptr);
                // Proxy geometry for lines/points
                cba.addPreCallback(SoLineSet::getClassTypeId(),
                                   nrtLineSetPreCB, &proxyData);
                cba.addPreCallback(SoIndexedLineSet::getClassTypeId(),
                                   nrtIndexedLineSetPreCB, &proxyData);
                cba.addPreCallback(SoPointSet::getClassTypeId(),
                                   nrtPointSetPreCB, &proxyData);
                // Cylinders with DrawStyle LINES represent rotational dragger
                // rings; replace them with thin facetized cylinder rings.
                cba.addPreCallback(SoCylinder::getClassTypeId(),
                                   nrtCylinderPreCB, &proxyData);
                cba.addPreCallback(SoText2::getClassTypeId(),
                                   nrtText2PreCB, &proxyData);
                cba.addPreCallback(SoHUDLabel::getClassTypeId(),
                                   nrtHUDLabelPreCB, &proxyData);
                cba.addPreCallback(SoHUDButton::getClassTypeId(),
                                   nrtHUDButtonPreCB, &proxyData);
                // All supported light types (with world-space transforms)
                cba.addPreCallback(SoDirectionalLight::getClassTypeId(),
                                   nrtDirectionalLightCB, &lights);
                cba.addPreCallback(SoPointLight::getClassTypeId(),
                                   nrtPointLightCB, &lights);
                cba.addPreCallback(SoSpotLight::getClassTypeId(),
                                   nrtSpotLightCB, &lights);
                cba.apply(scene);
            }

            // --- 1b. Rebuild BVH and update cache ---------------------------
            // Reset the cached scene to a clean default-constructed state
            // before populating it with the new geometry.
            cachedNrtScene_ = NrtScene();
            cachedNrtScene_.tris = std::move(collector.tris);
            if (!cachedNrtScene_.tris.empty()) {
                if (!cachedNrtScene_.build()) return FALSE;
            }
            cachedLights_ = std::move(lights);

            // Cache SoRaytracingParams (part of scene graph; any change
            // bumps the root nodeId and triggers a rebuild anyway).
            cachedShadowsEnabled_    = false;
            cachedMaxBouncesAllowed_ = 0;
            cachedSamplesPerPixel_   = 1;
            cachedAmbientFill_       = 0.20f;
            {
                SoSearchAction sa;
                sa.setType(SoRaytracingParams::getClassTypeId());
                sa.setInterest(SoSearchAction::FIRST);
                sa.apply(scene);
                if (sa.getPath()) {
                    const SoRaytracingParams * rp =
                        static_cast<const SoRaytracingParams *>(
                            sa.getPath()->getTail());
                    cachedShadowsEnabled_    =
                        rp->shadowsEnabled.getValue() != FALSE;
                    cachedMaxBouncesAllowed_ =
                        rp->maxReflectionBounces.getValue();
                    cachedSamplesPerPixel_   =
                        rp->samplesPerPixel.getValue();
                    cachedAmbientFill_       =
                        rp->ambientIntensity.getValue();
                    if (cachedSamplesPerPixel_  < 1) cachedSamplesPerPixel_  = 1;
                    if (cachedMaxBouncesAllowed_ < 0) cachedMaxBouncesAllowed_ = 0;
                    cachedAmbientFill_ = nrt_clamp01(cachedAmbientFill_);
                }
            }

            // Record the cache key so subsequent calls can detect hits.
            cachedScene_  = scene;
            cachedRootId_ = scene->getNodeId();
            cachedCamPtr_ = cam;
        } else {
            // --- 1c. Cache hit: lightweight text/HUD-overlay traversal ------
            // The geometry and lights are reused from the previous render.
            // Only text/HUD overlays are regenerated because their screen-
            // space positions depend on the current camera/viewport state.
            NrtSceneCollector textOverlayCollector;
            proxyData.collector = &textOverlayCollector;

            {
                SoCallbackAction cba(vp);
                cba.addPreCallback(SoText2::getClassTypeId(),
                                   nrtText2PreCB, &proxyData);
                cba.addPreCallback(SoHUDLabel::getClassTypeId(),
                                   nrtHUDLabelPreCB, &proxyData);
                cba.addPreCallback(SoHUDButton::getClassTypeId(),
                                   nrtHUDButtonPreCB, &proxyData);
                cba.apply(scene);
            }
        }


        // Local aliases for the cached rendering parameters.
        const bool  shadowsEnabled    = cachedShadowsEnabled_;
        const int   maxBouncesAllowed = cachedMaxBouncesAllowed_;
        const int   samplesPerPixel   = cachedSamplesPerPixel_;
        const float ambientFill       = cachedAmbientFill_;

        // --- 2-5. BVH build and ray-trace (skipped if scene has no triangles) ---
        if (!cachedNrtScene_.tris.empty()) {
        // The BVH is already built (either freshly or from cache).
        // Alias for readability in the sections below.
        NrtScene & nrtScene = cachedNrtScene_;
        const std::vector<NrtLightInfo> & lights = cachedLights_;

        // --- 3. Read SoRaytracingParams hints from scene --------------------
        // (Already cached above; local aliases used from here on.)

        // --- 4. Extract view volume from camera in scene --------------------
        // (cam was found at the top of renderScene, before the rebuild check)
        if (!cam) return FALSE;  // no camera: fall through to GL

        const float aspect_ratio =
            static_cast<float>(width) / static_cast<float>(height);
        SbViewVolume vv = cam->getViewVolume(aspect_ratio);
        if (aspect_ratio < 1.0f) vv.scale(1.0f / aspect_ratio);

        // --- 5. Raytrace ----------------------------------------------------
        nanort::TriangleIntersector<float> intersector(
            nrtScene.vertices.data(), nrtScene.faces.data(),
            sizeof(float) * 3);

        // Per-pixel state for jitter PRNG seed (deterministic for reproducibility)
        uint32_t rngState = 0xDEADBEEFu;

        for (unsigned int y = 0; y < height; ++y) {
            for (unsigned int x = 0; x < width; ++x) {
                // GL convention: row 0 is the BOTTOM of the screen.
                const float fx = static_cast<float>(x);
                const float fy = static_cast<float>(y);
                const float fw = static_cast<float>(width);
                const float fh = static_cast<float>(height);

                // Accumulate colour across AA samples.
                float accum[3] = { 0.0f, 0.0f, 0.0f };
                int hitSamples = 0;

                for (int s = 0; s < samplesPerPixel; ++s) {
                    // Jitter offset within pixel (stratified sub-pixel sampling).
                    float jx = 0.5f, jy = 0.5f;
                    if (samplesPerPixel > 1) {
                        jx = nrt_rand01(rngState);
                        jy = nrt_rand01(rngState);
                    }
                    const float nx = (fx + jx) / fw;
                    const float ny = (fy + jy) / fh;

                    SbVec3f p0, p1;
                    vv.projectPointToLine(SbVec2f(nx, ny), p0, p1);
                    SbVec3f d = p1 - p0;
                    d.normalize();

                    // Primary ray
                    nanort::Ray<float> ray;
                    ray.org[0] = p0[0]; ray.org[1] = p0[1]; ray.org[2] = p0[2];
                    ray.dir[0] = d[0];  ray.dir[1] = d[1];  ray.dir[2] = d[2];
                    ray.min_t  = 0.001f;
                    ray.max_t  = 1.0e30f;

                    nanort::TriangleIntersection<float> isect;
                    if (!nrtScene.accel.Traverse(ray, intersector, &isect))
                        continue;  // primary miss: leave pre-filled background pixel
                    ++hitSamples;

                    const unsigned int fid = isect.prim_id;
                    const float w0 = 1.0f - isect.u - isect.v;
                    const float w1 = isect.u;
                    const float w2 = isect.v;

                    const float * n0 = nrtScene.normals.data() + 9 * fid + 0;
                    const float * n1 = nrtScene.normals.data() + 9 * fid + 3;
                    const float * n2 = nrtScene.normals.data() + 9 * fid + 6;
                    float N[3] = {
                        w0*n0[0] + w1*n1[0] + w2*n2[0],
                        w0*n0[1] + w1*n1[1] + w2*n2[1],
                        w0*n0[2] + w1*n1[2] + w2*n2[2]
                    };
                    nrt_normalize3(N);

                    const float dir[3] = { d[0], d[1], d[2] };
                    const float V[3]   = { -dir[0], -dir[1], -dir[2] };
                    // No normal flip: match GL one-sided lighting (see nrt_trace).

                    const float hitPt[3] = {
                        p0[0] + dir[0] * isect.t,
                        p0[1] + dir[1] * isect.t,
                        p0[2] + dir[2] * isect.t
                    };

                    const NrtMaterial & mat = nrtScene.tris[fid].mat;

                    // Direct illumination
                    float px[3] = { 0.0f, 0.0f, 0.0f };
                    nrt_shade(nrtScene, intersector, hitPt, N, V, mat,
                              lights, ambientFill, shadowsEnabled, px);

                    // Reflection bounce (uses nrt_trace for secondary rays;
                    // background_rgb is used for missed secondary rays).
                    if (maxBouncesAllowed > 0) {
                        const float specLum =
                            (mat.specular[0] * 0.2126f +
                             mat.specular[1] * 0.7152f +
                             mat.specular[2] * 0.0722f) * mat.shininess;
                        if (specLum > 0.01f) {
                            const float NdotI = nrt_dot3(N, dir);
                            const float Rdir[3] = {
                                dir[0] - 2.0f * NdotI * N[0],
                                dir[1] - 2.0f * NdotI * N[1],
                                dir[2] - 2.0f * NdotI * N[2]
                            };
                            const float kEps = 1e-3f;
                            const float Rorg[3] = {
                                hitPt[0] + N[0] * kEps,
                                hitPt[1] + N[1] * kEps,
                                hitPt[2] + N[2] * kEps
                            };
                            float reflColor[3];
                            nrt_trace(nrtScene, intersector, Rorg, Rdir,
                                      lights, ambientFill, shadowsEnabled,
                                      maxBouncesAllowed - 1,
                                      background_rgb, reflColor);
                            px[0] += mat.specular[0] * specLum * reflColor[0];
                            px[1] += mat.specular[1] * specLum * reflColor[1];
                            px[2] += mat.specular[2] * specLum * reflColor[2];
                        }
                    }

                    accum[0] += px[0];
                    accum[1] += px[1];
                    accum[2] += px[2];
                }

                // Only write pixel if at least one sample hit geometry.
                // Pixels with no hits keep the pre-filled background colour
                // (which may be a gradient set by the caller).
                if (hitSamples == 0) continue;

                const float inv_s = 1.0f / static_cast<float>(hitSamples);
                const size_t idx  = (y * width + x) * nrcomponents;
                pixels[idx + 0] = static_cast<unsigned char>(
                    nrt_clamp01(accum[0] * inv_s) * 255.0f);
                pixels[idx + 1] = static_cast<unsigned char>(
                    nrt_clamp01(accum[1] * inv_s) * 255.0f);
                pixels[idx + 2] = static_cast<unsigned char>(
                    nrt_clamp01(accum[2] * inv_s) * 255.0f);
                if (nrcomponents == 4) pixels[idx + 3] = 255;
            }
        }
        } // end BVH/raytrace block (skipped when no triangles)

        // --- 6. Composite SoText2 overlays directly onto the framebuffer ----
        // Text overlays were collected during the scene traversal above.
        // They contain crisp (binary-threshold) RGBA pixel buffers stored in
        // GL convention (row 0 = bottom of viewport).  We composite them here
        // so each glyph shows its actual letter shape rather than a solid block.
        for (const NrtTextOverlay & ov : proxyData.textOverlays) {
            // Clamp the overlay rectangle to the framebuffer.
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
                // Source: ov.pixbuf row (src_y0 + row) counted from bottom.
                const int src_row = src_y0 + row;
                const unsigned char * src =
                    ov.pixbuf.data() + (src_row * ov.w + src_x0) * 4;

                // Destination: pixels row (dst_y0 + row) counted from bottom
                // (same GL convention the ray-tracer uses).
                const int dst_row = dst_y0 + row;
                const size_t dst_base =
                    static_cast<size_t>(dst_row) * width + dst_x0;

                for (int col = 0; col < draw_w; ++col) {
                    const unsigned char r_src = src[0];
                    const unsigned char g_src = src[1];
                    const unsigned char b_src = src[2];
                    const unsigned char a_src = src[3];
                    src += 4;

                    if (a_src == 0) continue; // fully transparent – skip

                    // Alpha-composite (src over dst) using the full stb_truetype
                    // grayscale alpha value for smooth anti-aliased text edges.
                    const float fa = a_src * (1.0f / 255.0f);
                    const float fb = 1.0f - fa;
                    const size_t idx = (dst_base + col) * nrcomponents;
                    pixels[idx + 0] = static_cast<unsigned char>(r_src * fa + pixels[idx + 0] * fb);
                    pixels[idx + 1] = static_cast<unsigned char>(g_src * fa + pixels[idx + 1] * fb);
                    pixels[idx + 2] = static_cast<unsigned char>(b_src * fa + pixels[idx + 2] * fb);
                    if (nrcomponents == 4) pixels[idx + 3] = 255;
                }
            }
        }

        cachedCamId_ = camId;
        return TRUE;
    }

private:
    // -----------------------------------------------------------------------
    // Geometry cache state
    //
    // cachedScene_  – pointer to the scene root that was last built.
    //                 nullptr means the cache is empty.
    // cachedRootId_ – SoNode::getNodeId() of cachedScene_ at BVH-build time.
    //                 Any geometry/material/light change propagates a
    //                 notification up to the root (bumping its uniqueId), so
    //                 comparing this value is an O(1) "did geometry change?"
    //                 check — provided camera-only changes are excluded (see
    //                 cachedCamPtr_ / cachedCamId_ below).
    // cachedCamPtr_ – the SoCamera node found in the scene at the last BVH
    //                 build.  Used to detect whether a root-nodeId change is
    //                 solely due to a camera move (same pointer, changed id)
    //                 rather than a true geometry/material/light change.
    // cachedCamId_  – SoNode::getNodeId() of the camera at the LAST render
    //                 (updated after every successful render, not just after
    //                 rebuilds).  Together with cachedCamPtr_ this lets us
    //                 infer "camera-only moved → skip BVH rebuild".
    // cachedNrtScene_ – the built BVH plus triangle/normal data.
    // cachedLights_   – world-space light descriptors from the last build.
    //
    // Rendering parameters (SoRaytracingParams) are also cached because
    // they live inside the scene graph; any change to them bumps the root
    // nodeId and triggers a rebuild.
    // -----------------------------------------------------------------------
    SoNode *                  cachedScene_           = nullptr;
    SbUniqueId                cachedRootId_          = 0;
    SoCamera *                cachedCamPtr_          = nullptr;
    SbUniqueId                cachedCamId_           = 0;
    NrtScene                  cachedNrtScene_;
    std::vector<NrtLightInfo> cachedLights_;
    bool                      cachedShadowsEnabled_    = false;
    int                       cachedMaxBouncesAllowed_ = 0;
    int                       cachedSamplesPerPixel_   = 1;
    float                     cachedAmbientFill_       = 0.20f;
};

#endif // OBOL_NANORT_CONTEXT_MANAGER_H
