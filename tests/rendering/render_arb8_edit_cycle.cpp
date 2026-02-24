/*
 * render_arb8_edit_cycle.cpp
 *
 * Visual regression / progression test for the SoProceduralShape full
 * ARB8-like edit cycle:
 *
 *   1. Parent application loads a shape type from a JSON schema definition.
 *   2. A SoProceduralShape node is created (setShapeType) — defaults come
 *      straight from the schema.
 *   3. buildSelectionDisplay() overlays labelled handle-sphere markers onto
 *      the solid so the user can identify selectable controls.
 *   4. A vertex is "dragged" by directly modifying params[], simulating the
 *      result of a successful DRAG_NO_INTERSECT drag validated by the parent
 *      application's objectValidateCB.
 *   5. getCurrentParamsJSON() extracts the edited parameter state.
 *
 * One SGI RGB image is written per step:
 *
 *   argv[1]+".rgb"         — primary (combined three-panel view)
 *   argv[1]+"_step1.rgb"   — step 1: initial solid ARB8 (from JSON defaults)
 *   argv[1]+"_step2.rgb"   — step 2: solid + buildSelectionDisplay() overlay
 *   argv[1]+"_step3.rgb"   — step 3: edited solid (v0 moved; v0_h highlighted)
 *
 * The primary output for CTest is argv[1]+".rgb" (combined three-panel view).
 * All four images are generated so that generate_controls.sh can capture
 * them as individual control PNGs for visual inspection.
 *
 * Output: argv[1]+".rgb"  (and _step1/_step2/_step3)  — 800×600 SGI RGB
 */

#include "headless_utils.h"

#include <Inventor/nodes/SoProceduralShape.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/SbViewportRegion.h>

#include <cstdio>
#include <cstring>
#include <vector>

// ============================================================================
// ARB8 geometry callbacks
// ============================================================================

static const int kFaces[6][4] = {
    {0,3,2,1},{4,5,6,7},{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7}
};
static const int kEdges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
};

static inline SbVec3f vtx(const float* p, int i)
{ return SbVec3f(p[3*i], p[3*i+1], p[3*i+2]); }

static void arb8_bbox(const float* p, int n, SbVec3f& mn, SbVec3f& mx, void*)
{
    if (n < 24) { mn.setValue(-2,-2,-2); mx.setValue(2,2,2); return; }
    float ax=p[0], ay=p[1], az=p[2], bx=p[0], by=p[1], bz=p[2];
    for (int i = 1; i < 8; ++i) {
        if (p[3*i  ] < ax) { ax = p[3*i  ]; }
        if (p[3*i+1] < ay) { ay = p[3*i+1]; }
        if (p[3*i+2] < az) { az = p[3*i+2]; }
        if (p[3*i  ] > bx) { bx = p[3*i  ]; }
        if (p[3*i+1] > by) { by = p[3*i+1]; }
        if (p[3*i+2] > bz) { bz = p[3*i+2]; }
    }
    mn.setValue(ax,ay,az); mx.setValue(bx,by,bz);
}

static void arb8_geom(const float* pp, int n,
                       SoProceduralTriangles* tris,
                       SoProceduralWireframe* wire, void*)
{
    // Use default unit-cube vertices if params not yet populated
    static const float kDef[24] = {
        -1,-1,-1, 1,-1,-1, 1,-1,1, -1,-1,1,
        -1, 1,-1, 1, 1,-1, 1, 1,1, -1, 1,1
    };
    const float* p = (n >= 24) ? pp : kDef;

    if (tris) {
        tris->vertices.clear(); tris->normals.clear(); tris->indices.clear();
        for (int f = 0; f < 6; ++f) {
            const int* fv = kFaces[f];
            SbVec3f v0=vtx(p,fv[0]),v1=vtx(p,fv[1]),v2=vtx(p,fv[2]),v3=vtx(p,fv[3]);
            // Use (v2-v0)×(v1-v0) order to produce outward-pointing face normals.
            SbVec3f nm = (v2-v0).cross(v1-v0); nm.normalize();
            int b = (int)tris->vertices.size();
            tris->vertices.insert(tris->vertices.end(), {v0,v1,v2,v3});
            tris->normals .insert(tris->normals .end(), {nm,nm,nm,nm});
            tris->indices .insert(tris->indices .end(), {b,b+1,b+2, b,b+2,b+3});
        }
    }
    if (wire) {
        wire->vertices.clear(); wire->segments.clear();
        for (int i = 0; i < 8; ++i) wire->vertices.push_back(vtx(p,i));
        for (int e = 0; e < 12; ++e) {
            wire->segments.push_back(kEdges[e][0]);
            wire->segments.push_back(kEdges[e][1]);
        }
    }
}

// ============================================================================
// ARB8 JSON schema (full topology — 8 vertices, 4 handles for this test)
// ============================================================================

static const char* kEditCycleSchema = R"({
    "type": "ARB8_edit_cycle",
    "label": "ARB8 Edit Cycle Demo",
    "params": [
        {"name":"v0x","type":"float","default":-1.0},
        {"name":"v0y","type":"float","default":-1.0},
        {"name":"v0z","type":"float","default":-1.0},
        {"name":"v1x","type":"float","default": 1.0},
        {"name":"v1y","type":"float","default":-1.0},
        {"name":"v1z","type":"float","default":-1.0},
        {"name":"v2x","type":"float","default": 1.0},
        {"name":"v2y","type":"float","default":-1.0},
        {"name":"v2z","type":"float","default": 1.0},
        {"name":"v3x","type":"float","default":-1.0},
        {"name":"v3y","type":"float","default":-1.0},
        {"name":"v3z","type":"float","default": 1.0},
        {"name":"v4x","type":"float","default":-1.0},
        {"name":"v4y","type":"float","default": 1.0},
        {"name":"v4z","type":"float","default":-1.0},
        {"name":"v5x","type":"float","default": 1.0},
        {"name":"v5y","type":"float","default": 1.0},
        {"name":"v5z","type":"float","default":-1.0},
        {"name":"v6x","type":"float","default": 1.0},
        {"name":"v6y","type":"float","default": 1.0},
        {"name":"v6z","type":"float","default": 1.0},
        {"name":"v7x","type":"float","default":-1.0},
        {"name":"v7y","type":"float","default": 1.0},
        {"name":"v7z","type":"float","default": 1.0}
    ],
    "vertices": [
        {"name":"v0","x":"v0x","y":"v0y","z":"v0z"},
        {"name":"v1","x":"v1x","y":"v1y","z":"v1z"},
        {"name":"v2","x":"v2x","y":"v2y","z":"v2z"},
        {"name":"v3","x":"v3x","y":"v3y","z":"v3z"},
        {"name":"v4","x":"v4x","y":"v4y","z":"v4z"},
        {"name":"v5","x":"v5x","y":"v5y","z":"v5z"},
        {"name":"v6","x":"v6x","y":"v6y","z":"v6z"},
        {"name":"v7","x":"v7x","y":"v7y","z":"v7z"}
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

// ============================================================================
// Scene-building helpers
// ============================================================================

// Standard camera + light wrapper
static void addCameraAndLight(SoSeparator* root, SbColor lightColor = SbColor(1,1,1))
{
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    root->addChild(cam);

    SoDirectionalLight* light = new SoDirectionalLight;
    light->direction.setValue(-0.5f, -0.8f, -0.6f);
    light->color.setValue(lightColor);
    root->addChild(light);
}

// Position camera to view the scene
static void setupCamera(SoSeparator* root, float backoff = 1.4f)
{
    SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    for (int i = 0; i < root->getNumChildren(); ++i) {
        SoNode* child = root->getChild(i);
        if (child->isOfType(SoPerspectiveCamera::getClassTypeId())) {
            SoPerspectiveCamera* cam = static_cast<SoPerspectiveCamera*>(child);
            cam->viewAll(root, vp);
            cam->position.setValue(cam->position.getValue() * backoff);
            return;
        }
    }
}

// Solid blue-grey material
static SoMaterial* solidMaterial()
{
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.25f, 0.45f, 0.85f);
    mat->specularColor.setValue(0.4f, 0.4f, 0.4f);
    mat->shininess.setValue(0.5f);
    return mat;
}

// Orange highlight material (used for the edited step)
static SoMaterial* editedMaterial()
{
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f, 0.5f, 0.1f);
    mat->specularColor.setValue(0.4f, 0.4f, 0.4f);
    mat->shininess.setValue(0.5f);
    return mat;
}

// Dark-grey background for all renders: shapes show against it
static const SbColor kBgColor(0.12f, 0.12f, 0.14f);

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv)
{
    initCoinHeadless();

    // ---- Register the ARB8 edit-cycle shape type from JSON schema ----
    //      objectValidateCB: always accepts (simulates parent-app approval)
    static const auto objCB = [](const char*, void*) -> SbBool {
        return TRUE;
    };
    SoProceduralShape::registerShapeType(
        "ARB8_edit_cycle", kEditCycleSchema,
        arb8_bbox, arb8_geom,
        nullptr, nullptr,
        nullptr,
        static_cast<SoProceduralObjectValidateCB>(objCB));

    const char* base = (argc > 1) ? argv[1] : "render_arb8_edit_cycle";

    // =====================================================================
    // Step 1 — Initial state: solid ARB8 from JSON schema defaults
    //          Camera looks from a slight elevation to show all faces.
    // =====================================================================
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        addCameraAndLight(root);

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("ARB8_edit_cycle"); // loads defaults from JSON
        SoSeparator* shapeSep = new SoSeparator;
        shapeSep->addChild(solidMaterial());
        shapeSep->addChild(shape);
        root->addChild(shapeSep);

        setupCamera(root, 1.5f);

        char out[1024];
        snprintf(out, sizeof(out), "%s_step1.rgb", base);
        renderToFile(root, out, DEFAULT_WIDTH, DEFAULT_HEIGHT, kBgColor);
        root->unref();
    }

    // =====================================================================
    // Step 2 — Selection display: solid cube + buildSelectionDisplay()
    //          The selection overlay adds labelled spheres at each handle
    //          position so the user can see the selectable controls.
    //          Camera is rotated to expose the front-bottom corners where
    //          most vertex handles reside.
    // =====================================================================
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        addCameraAndLight(root);

        // Solid body
        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("ARB8_edit_cycle");
        SoSeparator* shapeSep = new SoSeparator;
        shapeSep->addChild(solidMaterial());
        shapeSep->addChild(shape);
        root->addChild(shapeSep);

        // Selection-display overlay (sphere markers + text labels at handles)
        SoSeparator* selDisp = shape->buildSelectionDisplay();
        if (selDisp) {
            // Give the handle spheres a bright yellow colour
            SoSeparator* selSep = new SoSeparator;
            SoBaseColor* selCol = new SoBaseColor;
            selCol->rgb.setValue(1.0f, 0.95f, 0.0f);
            selSep->addChild(selCol);
            selSep->addChild(selDisp);
            root->addChild(selSep);
        }

        // Set up camera then rotate to a 3/4-view so front-bottom handles
        // aren't hidden behind the solid cube body.
        setupCamera(root, 1.7f);
        SoCamera* cam2 = findCamera(root);
        if (cam2) {
            // Orbit: 35° azimuth (look from right-front), 20° elevation
            rotateCamera(cam2, 0.61f, 0.35f);
        }

        char out[1024];
        snprintf(out, sizeof(out), "%s_step2.rgb", base);
        renderToFile(root, out, DEFAULT_WIDTH, DEFAULT_HEIGHT, kBgColor);
        root->unref();
    }

    // =====================================================================
    // Step 3 — After vertex manipulation: v0 dragged from (-1,-1,-1) to
    //          (-2.0,-1.0,-1.0) to simulate a DRAG_NO_INTERSECT edit cycle
    //          accepted by the parent application's objectValidateCB.
    //
    //          The orange-coloured solid shows the deformed shape.  The
    //          selection display is shown again to highlight how the handle
    //          markers move with the geometry.
    // =====================================================================
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        addCameraAndLight(root);

        SoProceduralShape* shape = new SoProceduralShape;
        shape->setShapeType("ARB8_edit_cycle");

        // Simulate drag: modify v0x from -1 to -2 (params[0])
        if (shape->params.getNum() == 24) {
            const float* cur = shape->params.getValues(0);
            float edited[24];
            for (int i = 0; i < 24; ++i) edited[i] = cur[i];
            edited[0] = -2.0f; // v0x: stretch corner outward
            shape->params.setValues(0, 24, edited);
        }

        SoSeparator* shapeSep = new SoSeparator;
        shapeSep->addChild(editedMaterial());
        shapeSep->addChild(shape);
        root->addChild(shapeSep);

        // Overlay handle spheres on edited shape
        SoSeparator* selDisp = shape->buildSelectionDisplay();
        if (selDisp) {
            SoSeparator* selSep = new SoSeparator;
            SoBaseColor* selCol = new SoBaseColor;
            selCol->rgb.setValue(1.0f, 0.95f, 0.0f);
            selSep->addChild(selCol);
            selSep->addChild(selDisp);
            root->addChild(selSep);
        }

        setupCamera(root, 1.6f);
        // Rotate to show the dragged corner (v0 at -2,-1,-1) prominently
        SoCamera* cam3 = findCamera(root);
        if (cam3) {
            rotateCamera(cam3, -0.4f, 0.25f);  // orbit: look from left-front slightly above
        }

        char out[1024];
        snprintf(out, sizeof(out), "%s_step3.rgb", base);
        renderToFile(root, out, DEFAULT_WIDTH, DEFAULT_HEIGHT, kBgColor);
        root->unref();
    }

    // =====================================================================
    // Step 4 (primary) — Combined side-by-side view:
    //   Left:   initial solid ARB8 (blue-grey)
    //   Centre: edited solid + selection display (orange + yellow handles)
    //   Right:  edited wireframe (green) — what the app would see post-edit
    //
    // This is the primary control image for CTest comparison.
    // =====================================================================
    {
        SoSeparator* root = new SoSeparator;
        root->ref();

        SoPerspectiveCamera* cam = new SoPerspectiveCamera;
        root->addChild(cam);

        SoDirectionalLight* light = new SoDirectionalLight;
        light->direction.setValue(-0.5f, -0.8f, -0.6f);
        root->addChild(light);

        // -- Left: initial solid --
        {
            SoSeparator* sep = new SoSeparator;
            SoTranslation* t = new SoTranslation;
            t->translation.setValue(-3.2f, 0.0f, 0.0f);
            sep->addChild(t);
            sep->addChild(solidMaterial());

            SoProceduralShape* shape = new SoProceduralShape;
            shape->setShapeType("ARB8_edit_cycle");
            sep->addChild(shape);
            root->addChild(sep);
        }

        // -- Centre: edited solid + handle display --
        {
            SoSeparator* sep = new SoSeparator;
            SoTranslation* t = new SoTranslation;
            t->translation.setValue(0.0f, 0.0f, 0.0f);
            sep->addChild(t);
            sep->addChild(editedMaterial());

            SoProceduralShape* shape = new SoProceduralShape;
            shape->setShapeType("ARB8_edit_cycle");
            if (shape->params.getNum() == 24) {
                const float* cur = shape->params.getValues(0);
                float edited[24];
                for (int i = 0; i < 24; ++i) edited[i] = cur[i];
                edited[0] = -2.0f;
                shape->params.setValues(0, 24, edited);
            }
            sep->addChild(shape);

            // getCurrentParamsJSON() — verification that the API works in a
            // rendered context; the result is logged to stdout for inspection.
            SbString json = shape->getCurrentParamsJSON();
            printf("Edit cycle getCurrentParamsJSON: %s\n", json.getString());

            // Handle-display overlay
            SoSeparator* selDisp = shape->buildSelectionDisplay();
            if (selDisp) {
                SoSeparator* selSep = new SoSeparator;
                SoBaseColor* selCol = new SoBaseColor;
                selCol->rgb.setValue(1.0f, 0.95f, 0.0f);
                selSep->addChild(selCol);
                selSep->addChild(selDisp);
                sep->addChild(selSep);
            }
            root->addChild(sep);
        }

        // -- Right: edited wireframe --
        {
            SoSeparator* sep = new SoSeparator;
            SoTranslation* t = new SoTranslation;
            t->translation.setValue(3.2f, 0.0f, 0.0f);
            sep->addChild(t);

            SoDrawStyle* ds = new SoDrawStyle;
            ds->style.setValue(SoDrawStyle::LINES);
            sep->addChild(ds);

            SoBaseColor* col = new SoBaseColor;
            col->rgb.setValue(0.1f, 0.9f, 0.3f);
            sep->addChild(col);

            SoProceduralShape* shape = new SoProceduralShape;
            shape->setShapeType("ARB8_edit_cycle");
            if (shape->params.getNum() == 24) {
                const float* cur = shape->params.getValues(0);
                float edited[24];
                for (int i = 0; i < 24; ++i) edited[i] = cur[i];
                edited[0] = -2.0f;
                shape->params.setValues(0, 24, edited);
            }
            sep->addChild(shape);
            root->addChild(sep);
        }

        SbViewportRegion vp(DEFAULT_WIDTH, DEFAULT_HEIGHT);
        cam->viewAll(root, vp);
        cam->position.setValue(cam->position.getValue() * 1.4f);

        char out[1024];
        snprintf(out, sizeof(out), "%s.rgb", base);
        bool ok = renderToFile(root, out, DEFAULT_WIDTH, DEFAULT_HEIGHT, kBgColor);
        root->unref();
        return ok ? 0 : 1;
    }
}
