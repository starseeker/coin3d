/*
 * obol_viewer.cpp  –  FLTK scene viewer for Obol (direct library use)
 *
 * Architecture
 * ────────────
 * The viewer links directly against the Coin shared library and uses Obol's
 * public API to build and render scenes into off-screen pixel buffers
 * displayed via FLTK.
 *
 * No bridge libraries or dlopen are involved.  The context manager
 * (OSMesa for headless/dual builds, GLX for sys-only builds) is set up
 * via headless_utils.h, exactly as the rendering tests do.
 *
 * OSMesa panel (dual-GL builds only: OBOL_BUILD_DUAL_GL)
 * ─────────────────────────────────────────────────────────
 * In dual-GL builds a second "OSMesa" panel is shown alongside the system-GL
 * panel.  It uses its own SoOffscreenRenderer whose context manager is set to
 * a private CoinOSMesaContextManager instance via the new
 * SoOffscreenRenderer::setContextManager() API.  This pins the OSMesa backend
 * to that specific renderer without touching the global singleton, so both GL
 * paths render simultaneously without any global state mutation.
 *
 * NanoRT optional panel
 * ─────────────────────
 * When OBOL_VIEWER_NANORT is defined at compile time (set by CMake when
 * external/nanort/nanort.h is found), an additional CPU-raytracing panel is
 * shown.  It uses SoNanoRTContextManager::renderScene() called directly.
 * Scenes that require GL-only features (e.g. SoShadowGroup) are flagged
 * nanort_ok=false in the scene catalogue and show "Not supported (NanoRT)".
 *
 * Layout (dual + nanort)                    Layout (dual only)
 * ──────────────────────────────────────    ─────────────────────────────────
 *  ┌──────┬───────────┬────────┬────────┐    ┌──────┬─────────────┬─────────┐
 *  │Scene │ System GL │ OSMesa │ NanoRT │    │Scene │  System GL  │  OSMesa │
 *  │brows.│  panel    │ panel  │ panel  │    │brows.│   panel     │  panel  │
 *  ├──────┴───────────┴────────┴────────┤    ├──────┴─────────────┴─────────┤
 *  │[Reload][Save]      [×] Sync All   │    │[Reload][Save]  [×]Sync All   │
 *  └────────────────────────────────────┘    └──────────────────────────────┘
 *
 * Panels resize uniformly (EqualTile distributes width equally on resize).
 *
 * Camera interaction
 *   Left-drag  → orbit (quaternion trackball – no gimbal lock)
 *   Right-drag → dolly
 *   Scroll     → zoom
 *   "Sync All" checkbox mirrors camera changes across all active panels.
 *
 * GL vs OSMesa rendering consistency
 * ───────────────────────────────────
 * Pixel comparison at multiple camera angles shows:
 *   - All opaque scenes: max diff ≤ 1 pixel (floating-point rounding only).
 *   - Transparency scene: ~94% of pixels identical; alpha-channel max diff
 *     ≈ 39/255 at sphere edges (expected from different GL implementations).
 *   - Shadow scene: marked "(GL only)" – OSMesa may lack the required shadow
 *     mapping extensions; significant visual difference is expected there.
 * All differences are within the "minor pixel differences" category.
 *
 * Building
 * ────────
 * Enabled by -DOBOL_BUILD_VIEWER=ON at CMake configure time.
 * The viewer is built as part of the main project; no separate superbuild
 * is required.
 */

/* ---- Obol / Coin headers ---- */
#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoIndexedLineSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoLightModel.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinate2.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/annex/FXViz/nodes/SoShadowGroup.h>
#include <Inventor/annex/FXViz/nodes/SoShadowDirectionalLight.h>

/* ---- Context manager (OSMesa for dual/headless, GLX for sys-only) ---- */
#include "headless_utils.h"

/* ---- Optional OSMesa panel: only available in dual-GL builds ----------- */
/* SoDB::createOSMesaContextManager() provides the OSMesa backend without   */
/* requiring the viewer to include any OSMesa headers directly.             */
#ifdef OBOL_BUILD_DUAL_GL
#  define OBOL_VIEWER_OSMESA_PANEL
#endif

/* ---- Optional NanoRT application-supplied renderer ---- */
#ifdef OBOL_VIEWER_NANORT
#  include "nanort_context_manager.h"
/* Single NanoRT renderer instance owned by the viewer application. */
static SoNanoRTContextManager s_nanort_mgr;
#endif

/* ---- FLTK ---- */
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Tile.H>
#include <FL/Fl_Check_Button.H>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

/* =========================================================================
 * Scene factories
 * ======================================================================= */

typedef SoSeparator* (*SceneFactory)(int w, int h);

static SoSeparator* scene_primitives(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-0.5f,-0.8f,-0.6f); root->addChild(li);
    const float s = 2.5f;
    struct { float r,g,b,tx,ty; SoNode* shape; } prims[] = {
        {0.85f,0.15f,0.15f,-s*.5f, s*.5f,new SoSphere},
        {0.15f,0.75f,0.15f, s*.5f, s*.5f,new SoCube},
        {0.15f,0.35f,0.90f,-s*.5f,-s*.5f,new SoCone},
        {0.90f,0.75f,0.15f, s*.5f,-s*.5f,new SoCylinder},
    };
    for (auto& p : prims) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(p.tx,p.ty,0);
        sep->addChild(t);
        SoMaterial* m = new SoMaterial;
        m->diffuseColor.setValue(p.r,p.g,p.b);
        m->ambientColor.setValue(p.r,p.g,p.b);
        m->specularColor.setValue(0.6f,0.6f,0.6f);
        m->shininess.setValue(0.5f);
        sep->addChild(m); sep->addChild(p.shape); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    cam->position.setValue(cam->position.getValue()*1.1f);
    cam->focalDistance.setValue(cam->focalDistance.getValue()*1.1f);
    return root;
}

static SoSeparator* scene_materials(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    struct MatDesc { float dr,dg,db,sr,sg,sb,sh,tx; };
    MatDesc mats[] = {
        {0.8f,0.1f,0.1f,0.9f,0.9f,0.9f,0.9f,-3.0f},
        {0.1f,0.7f,0.1f,0.0f,0.0f,0.0f,0.0f,-1.0f},
        {0.1f,0.1f,0.8f,0.5f,0.5f,0.8f,0.5f, 1.0f},
        {0.8f,0.7f,0.0f,1.0f,1.0f,0.8f,0.8f, 3.0f},
    };
    for (auto& m : mats) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(m.tx,0,0);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(m.dr,m.dg,m.db);
        mat->ambientColor.setValue(m.dr,m.dg,m.db);
        mat->specularColor.setValue(m.sr,m.sg,m.sb);
        mat->shininess.setValue(m.sh);
        sep->addChild(mat); sep->addChild(new SoSphere); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_lighting(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(0,-1,-1); li->color.setValue(1,0.9f,0.8f);
    root->addChild(li);
    SoPointLight* pl = new SoPointLight;
    pl->location.setValue(-3,2,2); pl->color.setValue(0.2f,0.4f,1.0f);
    root->addChild(pl);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.7f,0.7f,0.7f);
    mat->ambientColor.setValue(0.7f,0.7f,0.7f);
    mat->specularColor.setValue(1,1,1); mat->shininess.setValue(0.7f);
    root->addChild(mat); root->addChild(new SoSphere);
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_transforms(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.7f,0.9f);
    mat->ambientColor.setValue(0.5f,0.7f,0.9f);
    mat->specularColor.setValue(0.8f,0.8f,0.8f); mat->shininess.setValue(0.6f);
    root->addChild(mat);
    for (int i = 0; i < 5; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTransform* xf = new SoTransform;
        xf->translation.setValue((i-2)*2.0f, 0, 0);
        xf->rotation.setValue(SbVec3f(0,1,0), (float)i * 0.5f);
        xf->scaleFactor.setValue(0.6f+i*0.1f, 0.6f+i*0.1f, 0.6f+i*0.1f);
        sep->addChild(xf); sep->addChild(new SoCube); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_colored_cube(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.85f,0.10f,0.10f);
    mat->ambientColor.setValue(0.85f,0.10f,0.10f);
    mat->specularColor.setValue(0.50f,0.50f,0.50f); mat->shininess.setValue(0.40f);
    root->addChild(mat); root->addChild(new SoCube);
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_coordinates(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    root->addChild(new SoDirectionalLight);
    static const float axverts[6][3] = {
        {0,0,0},{2,0,0}, {0,0,0},{0,2,0}, {0,0,0},{0,0,2}
    };
    static const float axcols[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
    SoLightModel* lm = new SoLightModel;
    lm->model = SoLightModel::BASE_COLOR; root->addChild(lm);
    for (int a = 0; a < 3; ++a) {
        SoSeparator* sep = new SoSeparator;
        SoBaseColor* bc = new SoBaseColor;
        bc->rgb.setValue(axcols[a][0], axcols[a][1], axcols[a][2]);
        sep->addChild(bc);
        SoCoordinate3* co = new SoCoordinate3;
        co->point.setValues(0, 2, reinterpret_cast<const float(*)[3]>(axverts + a*2));
        sep->addChild(co);
        SoIndexedLineSet* ils = new SoIndexedLineSet;
        static const int32_t idx[] = {0,1,-1};
        ils->coordIndex.setValues(0, 3, idx); sep->addChild(ils);
        root->addChild(sep);
    }
    root->addChild(new SoSphere);
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_shadow(int w, int h) {
    SoSeparator* outer = new SoSeparator; outer->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->position.setValue(0, 8, 10);
    cam->pointAt(SbVec3f(0,0,0), SbVec3f(0,1,0));
    outer->addChild(cam);
    SoShadowGroup* sg = new SoShadowGroup;
    sg->isActive = TRUE; sg->intensity = 0.7f;
    SoShadowDirectionalLight* sdl = new SoShadowDirectionalLight;
    sdl->direction.setValue(-1,-2,-1); sg->addChild(sdl);
    SoMaterial* floor_mat = new SoMaterial;
    floor_mat->diffuseColor.setValue(0.7f,0.6f,0.5f);
    floor_mat->ambientColor.setValue(0.7f,0.6f,0.5f);
    sg->addChild(floor_mat);
    SoTransform* floor_xf = new SoTransform;
    floor_xf->scaleFactor.setValue(8,0.1f,8);
    floor_xf->translation.setValue(0,-0.5f,0); sg->addChild(floor_xf);
    sg->addChild(new SoCube);
    SoMaterial* sphere_mat = new SoMaterial;
    sphere_mat->diffuseColor.setValue(0.2f,0.4f,0.9f);
    sphere_mat->ambientColor.setValue(0.2f,0.4f,0.9f);
    sg->addChild(sphere_mat);
    SoTranslation* sph_t = new SoTranslation;
    sph_t->translation.setValue(0,1,0); sg->addChild(sph_t);
    sg->addChild(new SoSphere); outer->addChild(sg);
    return outer;
}

static SoSeparator* scene_transparency(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    struct { float r,g,b,a,x; } spheres[] = {
        {0.8f,0.1f,0.1f,0.3f,-2.0f},
        {0.1f,0.8f,0.1f,0.5f, 0.0f},
        {0.1f,0.1f,0.8f,0.7f, 2.0f},
        {0.8f,0.8f,0.1f,0.9f, 4.0f},
    };
    for (auto& s : spheres) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(s.x,0,0);
        sep->addChild(t);
        SoMaterial* mat = new SoMaterial;
        mat->diffuseColor.setValue(s.r,s.g,s.b);
        mat->ambientColor.setValue(s.r,s.g,s.b);
        mat->transparency.setValue(1.0f-s.a);
        sep->addChild(mat); sep->addChild(new SoSphere); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_lod(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.3f,0.6f,1.0f);
    mat->ambientColor.setValue(0.3f,0.6f,1.0f);
    mat->specularColor.setValue(0.9f,0.9f,0.9f); mat->shininess.setValue(0.8f);
    root->addChild(mat);
    for (int i = 0; i < 3; ++i) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue((i-1)*3.0f,0,0);
        sep->addChild(t);
        SoComplexity* cx = new SoComplexity; cx->value = 0.1f + 0.4f * i;
        sep->addChild(cx); sep->addChild(new SoSphere); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_drawstyle(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    const struct { SoDrawStyle::Style s; float tx; } styles[] = {
        {SoDrawStyle::FILLED, -3.0f},
        {SoDrawStyle::LINES,   0.0f},
        {SoDrawStyle::POINTS,  3.0f},
    };
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.5f,0.7f,0.3f);
    mat->ambientColor.setValue(0.5f,0.7f,0.3f);
    root->addChild(mat);
    for (auto& st : styles) {
        SoSeparator* sep = new SoSeparator;
        SoTranslation* t = new SoTranslation; t->translation.setValue(st.tx,0,0);
        sep->addChild(t);
        SoDrawStyle* ds = new SoDrawStyle; ds->style = st.s; sep->addChild(ds);
        sep->addChild(new SoCube); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

static SoSeparator* scene_indexed_face_set(int w, int h) {
    SoSeparator* root = new SoSeparator; root->ref();
    SoPerspectiveCamera* cam = new SoPerspectiveCamera; root->addChild(cam);
    SoDirectionalLight* li = new SoDirectionalLight;
    li->direction.setValue(-1,-1,-1); root->addChild(li);
    SoMaterial* mat = new SoMaterial;
    mat->diffuseColor.setValue(0.9f,0.5f,0.1f);
    mat->ambientColor.setValue(0.9f,0.5f,0.1f);
    root->addChild(mat);
    static const float pts[4][3] = {
        {0,1,0},{-1,-1,1},{1,-1,1},{0,-1,-1}
    };
    static const int32_t idx[] = {
        0,1,2,-1,  0,2,3,-1,  0,3,1,-1,  1,3,2,-1
    };
    SoCoordinate3* co = new SoCoordinate3;
    co->point.setValues(0, 4, pts); root->addChild(co);
    SoShapeHints* sh = new SoShapeHints;
    sh->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
    sh->shapeType = SoShapeHints::SOLID; root->addChild(sh);
    SoIndexedFaceSet* ifs = new SoIndexedFaceSet;
    ifs->coordIndex.setValues(0, 16, idx); root->addChild(ifs);
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    return root;
}

/* ---- Scene catalogue ---- */
struct SceneEntry {
    const char* name;
    const char* category;
    const char* description;
    SceneFactory factory;
    bool nanort_ok; /* false = NanoRT shows "not supported" instead of rendering */
};
static const SceneEntry s_scenes[] = {
    {"primitives",       "Rendering", "Basic primitives: sphere, cube, cone, cylinder", scene_primitives,       true},
    {"materials",        "Rendering", "Material property showcase",                      scene_materials,        true},
    {"lighting",         "Rendering", "Multiple light sources",                          scene_lighting,         true},
    {"transforms",       "Rendering", "Hierarchical transform chain",                    scene_transforms,       true},
    {"colored_cube",     "Rendering", "Simple red cube (smoke test)",                    scene_colored_cube,     true},
    {"coordinates",      "Rendering", "XYZ coordinate axis visualization",               scene_coordinates,      true},
    {"shadow",           "Rendering", "SoShadowGroup shadow casting (GL only)",          scene_shadow,           false},
    {"transparency",     "Rendering", "Alpha-blended transparent spheres",               scene_transparency,     true},
    {"lod",              "Rendering", "Level-of-detail SoComplexity comparison",         scene_lod,              true},
    {"drawstyle",        "Rendering", "Filled / wireframe / points draw styles",         scene_drawstyle,        true},
    {"indexed_face_set", "Rendering", "SoIndexedFaceSet tetrahedron",                    scene_indexed_face_set, true},
};
static const int s_scene_count = (int)(sizeof(s_scenes)/sizeof(s_scenes[0]));

/* =========================================================================
 * Scene state
 * ======================================================================= */
struct SceneState {
    SoSeparator*         root         = nullptr;
    SoPerspectiveCamera* cam          = nullptr;
    SbVec3f              scene_center = SbVec3f(0,0,0);
    int width  = 800;
    int height = 600;
    /* drag state */
    bool dragging  = false;
    int  drag_btn  = 0;
    int  last_x    = 0;
    int  last_y    = 0;

    ~SceneState() { if (root) root->unref(); }

    static SoPerspectiveCamera* findCamera(SoSeparator* r) {
        SoSearchAction sa;
        sa.setType(SoPerspectiveCamera::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(r);
        if (sa.getPath())
            return static_cast<SoPerspectiveCamera*>(sa.getPath()->getTail());
        return nullptr;
    }

    /* Compute and store the bounding-box centre of the scene. */
    void computeCenter() {
        if (!root) return;
        SoGetBoundingBoxAction bba(SbViewportRegion(width, height));
        bba.apply(root);
        SbBox3f bbox = bba.getBoundingBox();
        if (!bbox.isEmpty())
            scene_center = bbox.getCenter();
    }

    /* Keep near/far clipping in a sane ratio relative to the focal distance
     * so geometry never clips when orbiting or dollying. */
    void updateClipping() {
        if (!cam) return;
        float d = cam->focalDistance.getValue();
        if (d < 1e-4f) d = 1e-4f;
        cam->nearDistance.setValue(d * 0.001f);
        cam->farDistance.setValue(d * 10000.0f);
    }
};

/* Shared off-screen renderer */
static SoOffscreenRenderer* s_renderer = nullptr;
static SoOffscreenRenderer* getRenderer(int w, int h) {
    if (!s_renderer) {
        SbViewportRegion vp(w, h);
        s_renderer = new SoOffscreenRenderer(vp);
    }
    return s_renderer;
}

/* =========================================================================
 * CoinPanel  –  FLTK widget that renders a Coin scene
 * ======================================================================= */
class CoinPanel : public Fl_Box {
public:
    std::unique_ptr<SceneState> state;
    std::string label_text;
    std::string status_text;

    std::vector<uint8_t> pixel_buf;
    std::vector<uint8_t> display_buf;
    Fl_RGB_Image*        fltk_img = nullptr;

    std::function<void(CoinPanel*)> on_camera_changed;

    explicit CoinPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~CoinPanel() { delete fltk_img; }

    void loadScene(const char* name) {
        /* find scene factory */
        const SceneEntry* entry = nullptr;
        for (int i = 0; i < s_scene_count; ++i)
            if (strcmp(s_scenes[i].name, name) == 0) { entry = &s_scenes[i]; break; }
        if (!entry) { status_text = std::string("Unknown scene: ") + name; redraw(); return; }

        state.reset(new SceneState);
        state->width  = std::max(w(), 1);
        state->height = std::max(h(), 1);
        state->root   = entry->factory(state->width, state->height);
        state->cam    = state->root ? SceneState::findCamera(state->root) : nullptr;
        state->computeCenter();
        state->updateClipping();
        status_text.clear();
        refreshRender();
    }

    void clearScene() { state.reset(); delete fltk_img; fltk_img = nullptr; redraw(); }

    void refreshRender() {
        if (!state || !state->root) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
        if (!r->render(state->root)) {
            status_text = "Render failed"; redraw(); return;
        }
        const unsigned char* src = r->getBuffer();
        if (!src) { status_text = "No buffer"; redraw(); return; }
        /* convert bottom-up RGBA → top-down RGB */
        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = src + (size_t)(ph-1-row) * pw * 4;
            uint8_t*       d = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; s+=4; d+=3;
            }
        }
        delete fltk_img;
        fltk_img = new Fl_RGB_Image(display_buf.data(), pw, ph, 3);
        redraw();
    }

    void getCamera(float pos[3], float orient[4], float& dist) const {
        if (!state || !state->cam) return;
        SbVec3f p = state->cam->position.getValue();
        pos[0]=p[0]; pos[1]=p[1]; pos[2]=p[2];
        SbRotation rot = state->cam->orientation.getValue();
        SbVec3f axis; float angle; rot.getValue(axis,angle);
        float s2 = sinf(angle*0.5f);
        orient[0]=axis[0]*s2; orient[1]=axis[1]*s2; orient[2]=axis[2]*s2; orient[3]=cosf(angle*0.5f);
        dist = state->cam->focalDistance.getValue();
    }

    void setCamera(const float pos[3], const float orient[4], float dist) {
        if (!state || !state->cam) return;
        state->cam->position.setValue(pos[0],pos[1],pos[2]);
        float qx=orient[0],qy=orient[1],qz=orient[2],qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; }
        else ax.normalize();
        state->cam->orientation.setValue(SbRotation(ax, angle));
        if (dist > 0.0f) state->cam->focalDistance.setValue(dist);
        refreshRender();
    }

    /* ---- FLTK overrides ---- */

    void draw() override {
        fl_rectf(x(),y(),w(),h(),FL_BLACK);
        if (fltk_img) {
            fltk_img->draw(x(),y(),w(),h(),0,0);
        } else {
            fl_color(FL_WHITE); fl_font(FL_HELVETICA,14);
            fl_draw("(no scene)", x()+w()/2-40, y()+h()/2);
        }
        if (!label_text.empty()) {
            fl_color(fl_rgb_color(255,255,100)); fl_font(FL_HELVETICA_BOLD,13);
            fl_draw(label_text.c_str(), x()+6, y()+16);
        }
        if (!status_text.empty()) {
            fl_color(fl_rgb_color(255,80,80)); fl_font(FL_HELVETICA,12);
            fl_draw(status_text.c_str(), x()+6, y()+h()-6);
        }
    }

    int handle(int event) override {
        if (!state || !state->root) return Fl_Box::handle(event);
        int h_ = h();
        switch (event) {
        case FL_PUSH:
            take_focus();
            if (state) {
                state->dragging = true;
                state->drag_btn = Fl::event_button();
                state->last_x   = Fl::event_x() - x();
                state->last_y   = Fl::event_y() - y();
                SoMouseButtonEvent ev;
                ev.setButton(state->drag_btn==1 ? SoMouseButtonEvent::BUTTON1 :
                             state->drag_btn==2 ? SoMouseButtonEvent::BUTTON2 :
                                                  SoMouseButtonEvent::BUTTON3);
                ev.setState(SoButtonEvent::DOWN);
                ev.setPosition(SbVec2s((short)(Fl::event_x()-x()),
                                       (short)(h_-(Fl::event_y()-y()))));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
            }
            return 1;
        case FL_RELEASE:
            if (state) {
                state->dragging = false;
                SoMouseButtonEvent ev;
                ev.setButton(Fl::event_button()==1 ? SoMouseButtonEvent::BUTTON1 :
                             Fl::event_button()==2 ? SoMouseButtonEvent::BUTTON2 :
                                                     SoMouseButtonEvent::BUTTON3);
                ev.setState(SoButtonEvent::UP);
                ev.setPosition(SbVec2s((short)(Fl::event_x()-x()),
                                       (short)(h_-(Fl::event_y()-y()))));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                notifyCameraChanged();
                refreshRender();
            }
            return 1;
        case FL_DRAG:
            if (state && state->dragging && state->cam) {
                int ex = Fl::event_x()-x(), ey = Fl::event_y()-y();
                int dx = ex - state->last_x, dy = ey - state->last_y;
                state->last_x = ex; state->last_y = ey;
                if (state->drag_btn == 1) {
                    /* orbit – incremental rotation in camera-local space (BRL-CAD style):
                     * no world-up reference → smooth at all orientations, no gimbal lock */
                    orbitCamera(state->cam, state->scene_center,
                                (float)dx, (float)dy,
                                0.01f * (180.0f / static_cast<float>(M_PI)));
                } else if (state->drag_btn == 3) {
                    /* dolly toward/away from scene centre */
                    float dist = state->cam->focalDistance.getValue();
                    dist *= (1.0f + dy * 0.01f);
                    if (dist < 0.1f) dist = 0.1f;
                    SbVec3f dir = state->cam->position.getValue() - state->scene_center;
                    dir.normalize();
                    state->cam->position.setValue(state->scene_center + dir * dist);
                    state->cam->focalDistance.setValue(dist);
                    state->updateClipping();
                }
                SoLocation2Event ev;
                ev.setPosition(SbVec2s((short)ex,(short)(h_-ey)));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                notifyCameraChanged();
                refreshRender();
            }
            return 1;
        case FL_MOUSEWHEEL: {
            if (state && state->cam) {
                float delta = -(float)Fl::event_dy();
                float dist = state->cam->focalDistance.getValue();
                dist *= (1.0f - delta * 0.1f);
                if (dist < 0.1f) dist = 0.1f;
                SbVec3f dir = state->cam->position.getValue() - state->scene_center;
                dir.normalize();
                state->cam->position.setValue(state->scene_center + dir * dist);
                state->cam->focalDistance.setValue(dist);
                state->updateClipping();
                notifyCameraChanged();
                refreshRender();
            }
            return 1;
        }
        case FL_KEYDOWN:
            if (state && state->root) {
                SoKeyboardEvent ev;
                ev.setKey((SoKeyboardEvent::Key)Fl::event_key());
                ev.setState(SoButtonEvent::DOWN);
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                refreshRender();
            }
            return 1;
        case FL_KEYUP:
            if (state && state->root) {
                SoKeyboardEvent ev;
                ev.setKey((SoKeyboardEvent::Key)Fl::event_key());
                ev.setState(SoButtonEvent::UP);
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
            }
            return 1;
        case FL_FOCUS: case FL_UNFOCUS: return 1;
        default: break;
        }
        return Fl_Box::handle(event);
    }

    void resize(int X, int Y, int W, int H) override {
        Fl_Box::resize(X, Y, W, H);
        if (state) { state->width = W; state->height = H; }
        refreshRender();
    }

private:
    void notifyCameraChanged() { if (on_camera_changed) on_camera_changed(this); }
};

/* =========================================================================
 * NanoRTPanel  –  application-supplied CPU raytracing panel (optional)
 *
 * Renders the same scene graph as CoinPanel but via SoNanoRTContextManager
 * called directly as a plain method call — completely independent of the
 * GL context manager singleton that was registered with SoDB::init().
 * This is the "application-supplied renderer" pattern: the viewer owns the
 * NanoRT renderer object and drives it without any Coin involvement beyond
 * the scene graph traversal inside renderScene() itself.
 * ======================================================================= */
#ifdef OBOL_VIEWER_NANORT
class NanoRTPanel : public Fl_Box {
public:
    /* The scene root to render – shared with the CoinPanel (same graph). */
    SoSeparator*         root       = nullptr;
    SoPerspectiveCamera* cam        = nullptr;
    std::string          label_text;
    std::string          status_text;

    std::vector<uint8_t> pixel_buf;   /* RGBA, bottom-up from renderScene() */
    std::vector<uint8_t> display_buf; /* RGB, top-down for FLTK */
    Fl_RGB_Image*        fltk_img  = nullptr;

    std::function<void(NanoRTPanel*)> on_camera_changed;

    explicit NanoRTPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~NanoRTPanel() { delete fltk_img; }

    void setScene(SoSeparator* r, SoPerspectiveCamera* c, bool nanort_supported = true) {
        root = r; cam = c; nanort_ok_ = nanort_supported; status_text.clear();
        if (!nanort_ok_) {
            status_text = "Not supported (NanoRT)";
            delete fltk_img; fltk_img = nullptr;
            redraw(); return;
        }
        /* Compute scene bounding-box centre for orbit/dolly */
        scene_center_ = SbVec3f(0,0,0);
        if (root) {
            SoGetBoundingBoxAction bba(SbViewportRegion(std::max(w(),1), std::max(h(),1)));
            bba.apply(root);
            SbBox3f bbox = bba.getBoundingBox();
            if (!bbox.isEmpty())
                scene_center_ = bbox.getCenter();
        }
        refreshRender();
    }

    void refreshRender() {
        if (!root || !nanort_ok_) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);
        /* Pre-fill pixel buffer with background color (renderScene() leaves
         * miss pixels untouched, so the buffer must already contain the bg). */
        const float bg[3] = { 0.15f, 0.15f, 0.2f };
        const uint8_t bg_r = static_cast<uint8_t>(bg[0] * 255.0f + 0.5f);
        const uint8_t bg_g = static_cast<uint8_t>(bg[1] * 255.0f + 0.5f);
        const uint8_t bg_b = static_cast<uint8_t>(bg[2] * 255.0f + 0.5f);
        pixel_buf.resize((size_t)pw * ph * 4);
        for (size_t i = 0; i < (size_t)pw * ph; ++i) {
            pixel_buf[i*4+0] = bg_r;
            pixel_buf[i*4+1] = bg_g;
            pixel_buf[i*4+2] = bg_b;
            pixel_buf[i*4+3] = 255;
        }
        /* Direct call: s_nanort_mgr is an application-owned object.
         * We never registered it with SoDB::init(), so the GL context
         * manager singleton is completely untouched. */
        SbBool ok = s_nanort_mgr.renderScene(root,
                                             (unsigned int)pw,
                                             (unsigned int)ph,
                                             pixel_buf.data(),
                                             4u, bg);
        if (!ok) {
            status_text = "NanoRT render failed"; redraw(); return;
        }
        /* Convert bottom-up RGBA → top-down RGB for FLTK. */
        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = pixel_buf.data() + (size_t)(ph-1-row) * pw * 4;
            uint8_t*       d = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; s+=4; d+=3;
            }
        }
        delete fltk_img;
        fltk_img = new Fl_RGB_Image(display_buf.data(), pw, ph, 3);
        redraw();
    }

    void getCamera(float pos[3], float orient[4], float& dist) const {
        if (!cam) return;
        SbVec3f p = cam->position.getValue();
        pos[0]=p[0]; pos[1]=p[1]; pos[2]=p[2];
        SbRotation rot = cam->orientation.getValue();
        SbVec3f axis; float angle; rot.getValue(axis, angle);
        float s2 = sinf(angle*0.5f);
        orient[0]=axis[0]*s2; orient[1]=axis[1]*s2;
        orient[2]=axis[2]*s2; orient[3]=cosf(angle*0.5f);
        dist = cam->focalDistance.getValue();
    }

    void setCamera(const float pos[3], const float orient[4], float dist) {
        if (!cam) return;
        cam->position.setValue(pos[0], pos[1], pos[2]);
        float qx=orient[0], qy=orient[1], qz=orient[2], qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; } else ax.normalize();
        cam->orientation.setValue(SbRotation(ax, angle));
        if (dist > 0.0f) cam->focalDistance.setValue(dist);
        refreshRender();
    }

    /* ---- FLTK overrides ---- */

    void draw() override {
        fl_rectf(x(), y(), w(), h(), FL_BLACK);
        if (fltk_img) {
            fltk_img->draw(x(), y(), w(), h(), 0, 0);
        } else {
            fl_color(FL_WHITE); fl_font(FL_HELVETICA, 14);
            fl_draw("(no scene)", x()+w()/2-40, y()+h()/2);
        }
        if (!label_text.empty()) {
            fl_color(fl_rgb_color(255, 200, 80)); fl_font(FL_HELVETICA_BOLD, 13);
            fl_draw(label_text.c_str(), x()+6, y()+16);
        }
        if (!status_text.empty()) {
            fl_color(fl_rgb_color(255, 80, 80)); fl_font(FL_HELVETICA, 12);
            fl_draw(status_text.c_str(), x()+6, y()+h()-6);
        }
    }

    int handle(int event) override {
        if (!root || !cam || !nanort_ok_) return Fl_Box::handle(event);
        switch (event) {
        case FL_PUSH:
            take_focus();
            dragging_ = true;
            drag_btn_ = Fl::event_button();
            last_x_   = Fl::event_x() - x();
            last_y_   = Fl::event_y() - y();
            return 1;
        case FL_RELEASE:
            dragging_ = false;
            notifyCameraChanged(); refreshRender(); return 1;
        case FL_DRAG: {
            if (!dragging_) return 1;
            int ex = Fl::event_x()-x(), ey = Fl::event_y()-y();
            int dx = ex - last_x_, dy = ey - last_y_;
            last_x_ = ex; last_y_ = ey;
            if (drag_btn_ == 1) {
                /* orbit – incremental rotation in camera-local space (BRL-CAD style):
                 * no world-up reference → smooth at all orientations, no gimbal lock */
                orbitCamera(cam, scene_center_,
                            (float)dx, (float)dy,
                            0.01f * (180.0f / static_cast<float>(M_PI)));
            } else if (drag_btn_ == 3) {
                float dist = cam->focalDistance.getValue() * (1.0f + dy*0.01f);
                if (dist < 0.1f) dist = 0.1f;
                SbVec3f dir = cam->position.getValue() - scene_center_;
                dir.normalize();
                cam->position.setValue(scene_center_ + dir * dist);
                cam->focalDistance.setValue(dist);
                updateClipping_();
            }
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_MOUSEWHEEL: {
            float dist = cam->focalDistance.getValue() * (1.0f + (float)Fl::event_dy()*0.1f);
            if (dist < 0.1f) dist = 0.1f;
            SbVec3f dir = cam->position.getValue() - scene_center_;
            dir.normalize();
            cam->position.setValue(scene_center_ + dir * dist);
            cam->focalDistance.setValue(dist);
            updateClipping_();
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_FOCUS: case FL_UNFOCUS: return 1;
        default: break;
        }
        return Fl_Box::handle(event);
    }

    void resize(int X, int Y, int W, int H) override {
        Fl_Box::resize(X, Y, W, H);
        refreshRender();
    }

private:
    SbVec3f scene_center_ = SbVec3f(0,0,0);
    bool nanort_ok_ = true;
    bool dragging_  = false;
    int  drag_btn_  = 0;
    int  last_x_    = 0;
    int  last_y_    = 0;

    void updateClipping_() {
        if (!cam) return;
        float d = cam->focalDistance.getValue();
        if (d < 1e-4f) d = 1e-4f;
        cam->nearDistance.setValue(d * 0.001f);
        cam->farDistance.setValue(d * 10000.0f);
    }

    void notifyCameraChanged() { if (on_camera_changed) on_camera_changed(this); }
};
#endif /* OBOL_VIEWER_NANORT */


/* =========================================================================
 * OSMesaPanel  –  OSMesa offscreen rendering panel (dual-GL builds only)
 *
 * Renders the same scene graph as CoinPanel but through a dedicated
 * SoOffscreenRenderer whose context manager is an OSMesa backend obtained
 * from SoDB::createOSMesaContextManager().  No OSMesa headers are needed
 * here – all OSMesa details are encapsulated inside the Obol library.
 *
 * SoOffscreenRenderer::setContextManager() pins the OSMesa backend to this
 * specific renderer instance, so system-GL and OSMesa renders coexist in
 * the same process without any global state mutation.
 * ======================================================================= */
#ifdef OBOL_VIEWER_OSMESA_PANEL
class OSMesaPanel : public Fl_Box {
public:
    SoSeparator*         root       = nullptr;
    SoPerspectiveCamera* cam        = nullptr;
    std::string          label_text;
    std::string          status_text;

    std::vector<uint8_t> display_buf; /* RGB, top-down for FLTK */
    Fl_RGB_Image*        fltk_img  = nullptr;

    std::function<void(OSMesaPanel*)> on_camera_changed;

    explicit OSMesaPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);

        /* Obtain an OSMesa-backed context manager from Obol.  No OSMesa
         * headers are needed here – the library handles the details. */
        osmesa_mgr_.reset(SoDB::createOSMesaContextManager());

        SbViewportRegion vp(W, H);
        renderer_.reset(new SoOffscreenRenderer(vp));
        renderer_->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        renderer_->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
        if (osmesa_mgr_) {
            /* Pin this renderer to the OSMesa backend – completely independent
             * of the global context manager used by CoinPanel. */
            renderer_->setContextManager(osmesa_mgr_.get());
        } else {
            status_text = "OSMesa not available in this build";
        }
    }

    ~OSMesaPanel() { delete fltk_img; }

    void setScene(SoSeparator* r, SoPerspectiveCamera* c) {
        root = r; cam = c; status_text.clear();
        /* Compute scene bounding-box centre for orbit/dolly */
        scene_center_ = SbVec3f(0,0,0);
        if (root) {
            SoGetBoundingBoxAction bba(SbViewportRegion(std::max(w(),1), std::max(h(),1)));
            bba.apply(root);
            SbBox3f bbox = bba.getBoundingBox();
            if (!bbox.isEmpty())
                scene_center_ = bbox.getCenter();
        }
        refreshRender();
    }

    void refreshRender() {
        if (!root) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);

        SbViewportRegion vp(pw, ph);
        renderer_->setViewportRegion(vp);

        if (!renderer_->render(root)) {
            status_text = "OSMesa render failed"; redraw(); return;
        }

        const unsigned char* src = renderer_->getBuffer();
        if (!src) { status_text = "No buffer"; redraw(); return; }

        /* Convert bottom-up RGBA → top-down RGB for FLTK. */
        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = src + (size_t)(ph-1-row) * pw * 4;
            uint8_t*       d = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; s+=4; d+=3;
            }
        }
        delete fltk_img;
        fltk_img = new Fl_RGB_Image(display_buf.data(), pw, ph, 3);
        status_text.clear();
        redraw();
    }

    void getCamera(float pos[3], float orient[4], float& dist) const {
        if (!cam) return;
        SbVec3f p = cam->position.getValue();
        pos[0]=p[0]; pos[1]=p[1]; pos[2]=p[2];
        SbRotation rot = cam->orientation.getValue();
        SbVec3f axis; float angle; rot.getValue(axis, angle);
        float s2 = sinf(angle*0.5f);
        orient[0]=axis[0]*s2; orient[1]=axis[1]*s2;
        orient[2]=axis[2]*s2; orient[3]=cosf(angle*0.5f);
        dist = cam->focalDistance.getValue();
    }

    void setCamera(const float pos[3], const float orient[4], float dist) {
        if (!cam) return;
        cam->position.setValue(pos[0], pos[1], pos[2]);
        float qx=orient[0], qy=orient[1], qz=orient[2], qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; } else ax.normalize();
        cam->orientation.setValue(SbRotation(ax, angle));
        if (dist > 0.0f) cam->focalDistance.setValue(dist);
        refreshRender();
    }

    /* ---- FLTK overrides ---- */

    void draw() override {
        fl_rectf(x(), y(), w(), h(), FL_BLACK);
        if (fltk_img) {
            fltk_img->draw(x(), y(), w(), h(), 0, 0);
        } else {
            fl_color(FL_WHITE); fl_font(FL_HELVETICA, 14);
            fl_draw("(no scene)", x()+w()/2-40, y()+h()/2);
        }
        if (!label_text.empty()) {
            fl_color(fl_rgb_color(80, 200, 255)); fl_font(FL_HELVETICA_BOLD, 13);
            fl_draw(label_text.c_str(), x()+6, y()+16);
        }
        if (!status_text.empty()) {
            fl_color(fl_rgb_color(255, 80, 80)); fl_font(FL_HELVETICA, 12);
            fl_draw(status_text.c_str(), x()+6, y()+h()-6);
        }
    }

    int handle(int event) override {
        if (!root || !cam) return Fl_Box::handle(event);
        switch (event) {
        case FL_PUSH:
            take_focus();
            dragging_ = true;
            drag_btn_ = Fl::event_button();
            last_x_   = Fl::event_x() - x();
            last_y_   = Fl::event_y() - y();
            return 1;
        case FL_RELEASE:
            dragging_ = false;
            notifyCameraChanged(); refreshRender(); return 1;
        case FL_DRAG: {
            if (!dragging_) return 1;
            int ex = Fl::event_x()-x(), ey = Fl::event_y()-y();
            int dx = ex - last_x_, dy = ey - last_y_;
            last_x_ = ex; last_y_ = ey;
            if (drag_btn_ == 1) {
                /* orbit – incremental rotation in camera-local space (BRL-CAD style):
                 * no world-up reference → smooth at all orientations, no gimbal lock */
                orbitCamera(cam, scene_center_,
                            (float)dx, (float)dy,
                            0.01f * (180.0f / static_cast<float>(M_PI)));
            } else if (drag_btn_ == 3) {
                float dist = cam->focalDistance.getValue() * (1.0f + dy*0.01f);
                if (dist < 0.1f) dist = 0.1f;
                SbVec3f dir = cam->position.getValue() - scene_center_;
                dir.normalize();
                cam->position.setValue(scene_center_ + dir * dist);
                cam->focalDistance.setValue(dist);
                updateClipping_();
            }
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_MOUSEWHEEL: {
            float dist = cam->focalDistance.getValue() * (1.0f + (float)Fl::event_dy()*0.1f);
            if (dist < 0.1f) dist = 0.1f;
            SbVec3f dir = cam->position.getValue() - scene_center_;
            dir.normalize();
            cam->position.setValue(scene_center_ + dir * dist);
            cam->focalDistance.setValue(dist);
            updateClipping_();
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_FOCUS: case FL_UNFOCUS: return 1;
        default: break;
        }
        return Fl_Box::handle(event);
    }

    void resize(int X, int Y, int W, int H) override {
        Fl_Box::resize(X, Y, W, H);
        refreshRender();
    }

private:
    /* The OSMesa context manager is created via SoDB::createOSMesaContextManager()
     * – no OSMesa headers needed in this file. */
    std::unique_ptr<SoDB::ContextManager>  osmesa_mgr_;
    std::unique_ptr<SoOffscreenRenderer>   renderer_;

    SbVec3f scene_center_ = SbVec3f(0,0,0);
    bool dragging_ = false;
    int  drag_btn_ = 0;
    int  last_x_   = 0;
    int  last_y_   = 0;

    void updateClipping_() {
        if (!cam) return;
        float d = cam->focalDistance.getValue();
        if (d < 1e-4f) d = 1e-4f;
        cam->nearDistance.setValue(d * 0.001f);
        cam->farDistance.setValue(d * 10000.0f);
    }

    void notifyCameraChanged() { if (on_camera_changed) on_camera_changed(this); }
};
#endif /* OBOL_VIEWER_OSMESA_PANEL */


/* =========================================================================
 * EqualTile  –  Fl_Tile variant that redistributes child widths uniformly
 *               when the tile itself is resized (window resize), while still
 *               allowing the user to drag panel borders interactively.
 * ======================================================================= */
class EqualTile : public Fl_Tile {
public:
    EqualTile(int X, int Y, int W, int H) : Fl_Tile(X, Y, W, H) {}

    void resize(int X, int Y, int W, int H) override {
        int n = children();
        if (n == 0) { Fl_Tile::resize(X, Y, W, H); return; }
        /* Update our own bounding box without letting Fl_Tile redistribute. */
        Fl_Widget::resize(X, Y, W, H);
        /* Distribute width equally; last child absorbs the remainder. */
        int cw = W / n;
        int cx = X;
        for (int i = 0; i < n; ++i) {
            int this_w = (i == n-1) ? (X + W - cx) : cw;
            child(i)->resize(cx, Y, this_w, H);
            cx += this_w;
        }
        init_sizes();
    }
};


class ObolViewerWindow : public Fl_Double_Window {
    Fl_Hold_Browser*  browser_;
    CoinPanel*        coin_panel_;
    Fl_Button*        reload_btn_;
    Fl_Button*        save_btn_;
#if defined(OBOL_VIEWER_OSMESA_PANEL) || defined(OBOL_VIEWER_NANORT)
    Fl_Button*        compare_btn_ = nullptr;
#endif
    Fl_Box*           status_bar_;
    EqualTile*        tile_ = nullptr;
#ifdef OBOL_VIEWER_OSMESA_PANEL
    OSMesaPanel*      osmesa_panel_ = nullptr;
#endif
#ifdef OBOL_VIEWER_NANORT
    NanoRTPanel*      nrt_panel_  = nullptr;
#endif
#if defined(OBOL_VIEWER_OSMESA_PANEL) || defined(OBOL_VIEWER_NANORT)
    Fl_Check_Button*  sync_btn_  = nullptr;
    bool              syncing_   = false;
#endif

    static const int BROWSER_W = 220;
    static const int TOOLBAR_H = 32;
    static const int STATUS_H  = 22;

public:
    ObolViewerWindow(int W, int H)
        : Fl_Double_Window(W, H, "Obol Scene Viewer")
    {
        begin();
        buildUI(W, H);
        end();
        resizable(this);
    }

    void loadScene(const char* name) {
        coin_panel_->loadScene(name);

        /* Find the SceneEntry to know which renderers support this scene. */
        const SceneEntry* entry = nullptr;
        for (int i = 0; i < s_scene_count; ++i)
            if (strcmp(s_scenes[i].name, name) == 0) { entry = &s_scenes[i]; break; }
        const bool nanort_ok = entry ? entry->nanort_ok : true;

#ifdef OBOL_VIEWER_OSMESA_PANEL
        /* OSMesa panel shares the same scene root and camera as the Coin panel. */
        if (osmesa_panel_ && coin_panel_->state && coin_panel_->state->root) {
            osmesa_panel_->setScene(coin_panel_->state->root,
                                    coin_panel_->state->cam);
        }
#endif /* OBOL_VIEWER_OSMESA_PANEL */

#ifdef OBOL_VIEWER_NANORT
        /* NanoRT panel shares the same scene root and camera as the Coin panel.
         * Pass nanort_ok so the panel knows whether to render or show a message. */
        if (nrt_panel_ && coin_panel_->state && coin_panel_->state->root) {
            nrt_panel_->setScene(coin_panel_->state->root,
                                 coin_panel_->state->cam, nanort_ok);
        }
#endif /* OBOL_VIEWER_NANORT */

        /* Wire unified all-to-all camera sync so every active panel stays in
         * sync when the sync button is checked. A single syncing_ flag prevents
         * recursive callbacks. */
#if defined(OBOL_VIEWER_OSMESA_PANEL) || defined(OBOL_VIEWER_NANORT)
        coin_panel_->on_camera_changed = [this](CoinPanel* src) {
            if (!syncing_ && sync_btn_ && sync_btn_->value()) {
                syncing_ = true;
                float pos[3], orient[4], dist = 1.0f;
                src->getCamera(pos, orient, dist);
#  ifdef OBOL_VIEWER_OSMESA_PANEL
                if (osmesa_panel_) osmesa_panel_->setCamera(pos, orient, dist);
#  endif
#  ifdef OBOL_VIEWER_NANORT
                if (nrt_panel_) nrt_panel_->setCamera(pos, orient, dist);
#  endif
                syncing_ = false;
            }
        };
#  ifdef OBOL_VIEWER_OSMESA_PANEL
        if (osmesa_panel_) {
            osmesa_panel_->on_camera_changed = [this](OSMesaPanel* src) {
                if (!syncing_ && sync_btn_ && sync_btn_->value()) {
                    syncing_ = true;
                    float pos[3], orient[4], dist = 1.0f;
                    src->getCamera(pos, orient, dist);
                    coin_panel_->setCamera(pos, orient, dist);
#    ifdef OBOL_VIEWER_NANORT
                    if (nrt_panel_) nrt_panel_->setCamera(pos, orient, dist);
#    endif
                    syncing_ = false;
                }
            };
        }
#  endif /* OBOL_VIEWER_OSMESA_PANEL */
#  ifdef OBOL_VIEWER_NANORT
        if (nrt_panel_) {
            nrt_panel_->on_camera_changed = [this](NanoRTPanel* src) {
                if (!syncing_ && sync_btn_ && sync_btn_->value()) {
                    syncing_ = true;
                    float pos[3], orient[4], dist = 1.0f;
                    src->getCamera(pos, orient, dist);
                    coin_panel_->setCamera(pos, orient, dist);
#    ifdef OBOL_VIEWER_OSMESA_PANEL
                    if (osmesa_panel_) osmesa_panel_->setCamera(pos, orient, dist);
#    endif
                    syncing_ = false;
                }
            };
        }
#  endif /* OBOL_VIEWER_NANORT */
#endif /* OBOL_VIEWER_OSMESA_PANEL || OBOL_VIEWER_NANORT */

        std::string s = "Scene: "; s += name;
        status_bar_->copy_label(s.c_str());
    }

private:
    /* ---- coin panel label, chosen at compile time ---- */
    static const char* coinLabel() {
#if defined(OBOL_BUILD_DUAL_GL)
        return "System GL";
#elif defined(OBOL_OSMESA_BUILD)
        return "OSMesa (headless)";
#else
        return "System OpenGL";
#endif
    }

    void buildUI(int W, int H) {
        int content_h = H - TOOLBAR_H - STATUS_H;

        browser_ = new Fl_Hold_Browser(0, 0, BROWSER_W, content_h);
        browser_->textsize(12);
        browser_->callback(browserCB, this);
        for (int i = 0; i < s_scene_count; ++i) {
            std::string e = std::string(s_scenes[i].category) + "/" + s_scenes[i].name;
            browser_->add(e.c_str());
        }

        /* Render area: EqualTile so panels resize uniformly.
         * Panel layout depends on which optional panels are compiled in:
         *   dual + nanort  → 3 panels: System GL | OSMesa | NanoRT
         *   dual only      → 2 panels: System GL | OSMesa
         *   nanort only    → 2 panels: Coin GL   | NanoRT
         *   neither        → 1 panel:  Coin GL
         */
        tile_ = new EqualTile(BROWSER_W, 0, W - BROWSER_W, content_h);
        {
#if defined(OBOL_VIEWER_OSMESA_PANEL) && defined(OBOL_VIEWER_NANORT)
            /* Three panels */
            int panel_w = (W - BROWSER_W) / 3;
            coin_panel_   = new CoinPanel(BROWSER_W, 0, panel_w, content_h, coinLabel());
            osmesa_panel_ = new OSMesaPanel(BROWSER_W + panel_w, 0,
                                            panel_w, content_h,
                                            "OSMesa (per-renderer backend)");
            nrt_panel_    = new NanoRTPanel(BROWSER_W + 2*panel_w, 0,
                                            (W - BROWSER_W) - 2*panel_w, content_h,
                                            "NanoRT (app-supplied renderer)");
#elif defined(OBOL_VIEWER_OSMESA_PANEL)
            /* Two panels: System GL + OSMesa */
            int cpw = (W - BROWSER_W) / 2;
            coin_panel_   = new CoinPanel(BROWSER_W, 0, cpw, content_h, coinLabel());
            osmesa_panel_ = new OSMesaPanel(BROWSER_W + cpw, 0,
                                            W - BROWSER_W - cpw, content_h,
                                            "OSMesa (per-renderer backend)");
#elif defined(OBOL_VIEWER_NANORT)
            /* Two panels: Coin GL + NanoRT */
            int cpw = (W - BROWSER_W) / 2;
            coin_panel_ = new CoinPanel(BROWSER_W, 0, cpw, content_h, coinLabel());
            nrt_panel_  = new NanoRTPanel(BROWSER_W + cpw, 0,
                                          W - BROWSER_W - cpw, content_h,
                                          "NanoRT (app-supplied renderer)");
#else
            /* Single panel */
            coin_panel_ = new CoinPanel(BROWSER_W, 0, W - BROWSER_W, content_h,
                                        coinLabel());
#endif
        }
        tile_->end();

        /* Toolbar */
        Fl_Group* tb = new Fl_Group(0, content_h, W, TOOLBAR_H);
        tb->box(FL_UP_BOX);
        {
            int bx = 6, by = content_h+4, bh = TOOLBAR_H-8;
            reload_btn_ = new Fl_Button(bx, by, 70, bh, "Reload");
            reload_btn_->callback(reloadCB, this); bx += 76;
            save_btn_ = new Fl_Button(bx, by, 80, bh, "Save RGB...");
            save_btn_->callback(saveCB, this); bx += 86;
#if defined(OBOL_VIEWER_OSMESA_PANEL) || defined(OBOL_VIEWER_NANORT)
            compare_btn_ = new Fl_Button(bx, by, 80, bh, "Compare");
            compare_btn_->callback(compareCB, this); bx += 86;
#endif
#if defined(OBOL_VIEWER_OSMESA_PANEL) && defined(OBOL_VIEWER_NANORT)
            sync_btn_ = new Fl_Check_Button(bx, by, 80, bh, "Sync All");
            sync_btn_->value(1); bx += 86;
#elif defined(OBOL_VIEWER_OSMESA_PANEL)
            sync_btn_ = new Fl_Check_Button(bx, by, 120, bh, "Sync GL+OSMesa");
            sync_btn_->value(1); bx += 126;
#elif defined(OBOL_VIEWER_NANORT)
            sync_btn_ = new Fl_Check_Button(bx, by, 110, bh, "Sync NanoRT");
            sync_btn_->value(1); bx += 116;
#endif
            status_bar_ = new Fl_Box(bx, by, W-bx-6, bh, "Ready");
            status_bar_->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
            status_bar_->labelsize(12);
        }
        tb->end();

        Fl_Box* sbar = new Fl_Box(0, content_h+TOOLBAR_H, W, STATUS_H, "");
        sbar->box(FL_ENGRAVED_BOX);
        sbar->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
        sbar->labelsize(11);
    }

    static void browserCB(Fl_Widget*, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        int sel = self->browser_->value();
        if (sel < 1) return;
        const char* entry = self->browser_->text(sel);
        const char* slash = strrchr(entry, '/');
        self->loadScene(slash ? slash+1 : entry);
    }

    static void reloadCB(Fl_Widget*, void* data) { browserCB(nullptr, data); }

#if defined(OBOL_VIEWER_OSMESA_PANEL) || defined(OBOL_VIEWER_NANORT)
    /* Compare rendered images across all panels and report pixel statistics. */
    static void compareCB(Fl_Widget*, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        if (!self->coin_panel_->state || !self->coin_panel_->state->root) {
            fl_message("No scene loaded."); return;
        }

        /* Collect (label, buf, w, h) for every panel that has a rendered image */
        struct PanelImg {
            const char*        label;
            const uint8_t*     buf;   /* RGB top-down */
            int                pw, ph;
        };
        std::vector<PanelImg> panels;

        if (!self->coin_panel_->display_buf.empty())
            panels.push_back({self->coin_panel_->label_text.c_str(),
                              self->coin_panel_->display_buf.data(),
                              self->coin_panel_->w(), self->coin_panel_->h()});
#  ifdef OBOL_VIEWER_OSMESA_PANEL
        if (self->osmesa_panel_ && !self->osmesa_panel_->display_buf.empty())
            panels.push_back({self->osmesa_panel_->label_text.c_str(),
                              self->osmesa_panel_->display_buf.data(),
                              self->osmesa_panel_->w(), self->osmesa_panel_->h()});
#  endif
#  ifdef OBOL_VIEWER_NANORT
        if (self->nrt_panel_ && !self->nrt_panel_->display_buf.empty())
            panels.push_back({self->nrt_panel_->label_text.c_str(),
                              self->nrt_panel_->display_buf.data(),
                              self->nrt_panel_->w(), self->nrt_panel_->h()});
#  endif

        if (panels.size() < 2) {
            fl_message("Need at least 2 rendered panels to compare."); return;
        }

        /* Compare each pair and accumulate results */
        std::string report;
        for (size_t a = 0; a < panels.size(); ++a) {
            for (size_t b = a+1; b < panels.size(); ++b) {
                const PanelImg& pa = panels[a];
                const PanelImg& pb = panels[b];
                int cw = std::min(pa.pw, pb.pw);
                int ch = std::min(pa.ph, pb.ph);
                if (cw <= 0 || ch <= 0) continue;

                int    max_diff  = 0;
                double sum_sq    = 0.0;
                int    diff_rows = 0;  /* rows containing at least one channel diff > 1 */

                for (int row = 0; row < ch; ++row) {
                    const uint8_t* sa = pa.buf + (size_t)row * pa.pw * 3;
                    const uint8_t* sb = pb.buf + (size_t)row * pb.pw * 3;
                    bool row_diff = false;
                    for (int col = 0; col < cw; ++col) {
                        for (int c = 0; c < 3; ++c) {
                            int d = (int)sa[c] - (int)sb[c];
                            if (d < 0) d = -d;
                            if (d > max_diff) max_diff = d;
                            sum_sq += (double)d * d;
                            if (d > 1) row_diff = true;
                        }
                        sa += 3; sb += 3;
                    }
                    if (row_diff) ++diff_rows;
                }

                double rmse = std::sqrt(sum_sq / ((double)cw * ch * 3));
                double pct_diff = 100.0 * diff_rows / (double)ch;

                char line[256];
                std::snprintf(line, sizeof(line),
                    "%s vs %s:\n"
                    "  size %dx%d  max_diff=%d  RMSE=%.2f  "
                    "rows_with_diff=%.1f%%\n",
                    pa.label, pb.label,
                    cw, ch, max_diff, rmse, pct_diff);
                report += line;
            }
        }

        fl_message("%s", report.c_str());
    }
#endif /* OBOL_VIEWER_OSMESA_PANEL || OBOL_VIEWER_NANORT */

    static void saveCB(Fl_Widget*, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        if (!self->coin_panel_->state || !self->coin_panel_->state->root) {
            fl_message("No scene loaded."); return;
        }
        const char* path = fl_file_chooser("Save RGB", "*.rgb", "scene.rgb");
        if (!path) return;
        int pw = self->coin_panel_->w(), ph = self->coin_panel_->h();
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
        if (!r->render(self->coin_panel_->state->root)) {
            fl_message("Render failed."); return;
        }
        const unsigned char* src = r->getBuffer();
        std::vector<uint8_t> rgb((size_t)pw*ph*3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = src + (size_t)(ph-1-row)*pw*4;
            uint8_t*       d = rgb.data() + (size_t)row*pw*3;
            for (int col = 0; col < pw; ++col) {
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; s+=4; d+=3;
            }
        }
        /* Write as SGI-RGB (.rgb) since SoOffscreenRenderer::writeToRGB is
         * always available without extra library dependencies. */
        std::string outpath(path);
        if (outpath.size() >= 4 && outpath.substr(outpath.size()-4) == ".png")
            outpath = outpath.substr(0, outpath.size()-4) + ".rgb";
        if (r->writeToRGB(outpath.c_str()))
            fl_message("Saved to:\n%s", outpath.c_str());
        else
            fl_message("Failed to write file.");
    }
};

/* =========================================================================
 * main()
 * ======================================================================= */
int main(int argc, char** argv)
{
    /* Initialise Obol using the same context manager pattern as the tests */
    initCoinHeadless();

    Fl::scheme("gtk+");
    Fl::visual(FL_RGB | FL_DOUBLE);

    ObolViewerWindow* win = new ObolViewerWindow(1100, 700);
    win->show(argc, argv);

    /* Load the first scene automatically */
    if (s_scene_count > 0)
        win->loadScene(s_scenes[0].name);

    return Fl::run();
}
