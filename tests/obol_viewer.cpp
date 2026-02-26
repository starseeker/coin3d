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
 * OSMesa panel (dual-GL builds only: COIN3D_BUILD_DUAL_GL)
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
 *
 * Layout (dual + nanort)                    Layout (dual only)
 * ──────────────────────────────────────    ─────────────────────────────────
 *  ┌──────┬───────────┬────────┬────────┐    ┌──────┬─────────────┬─────────┐
 *  │Scene │ System GL │ OSMesa │ NanoRT │    │Scene │  System GL  │  OSMesa │
 *  │brows.│  panel    │ panel  │ panel  │    │brows.│   panel     │  panel  │
 *  ├──────┴───────────┴────────┴────────┤    ├──────┴─────────────┴─────────┤
 *  │[Reload][Save]  [×]GL+OSMesa [×]NRT│    │[Reload][Save] [×]Sync GL+OSM│
 *  └────────────────────────────────────┘    └──────────────────────────────┘
 *
 * Non-dual builds fall back to the existing 1- or 2-panel layout.
 *
 * Camera interaction
 *   Left-drag  → orbit
 *   Right-drag → dolly
 *   Scroll     → zoom
 *   Sync checkboxes mirror camera between matching panel pairs.
 *
 * Building
 * ────────
 * Enabled by -DCOIN_BUILD_VIEWER=ON at CMake configure time.
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
#ifdef COIN3D_BUILD_DUAL_GL
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
        m->specularColor.setValue(0.6f,0.6f,0.6f);
        m->shininess.setValue(0.5f);
        sep->addChild(m); sep->addChild(p.shape); root->addChild(sep);
    }
    SbViewportRegion vp(w,h); cam->viewAll(root,vp);
    cam->position.setValue(cam->position.getValue()*1.1f);
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
    floor_mat->diffuseColor.setValue(0.7f,0.6f,0.5f); sg->addChild(floor_mat);
    SoTransform* floor_xf = new SoTransform;
    floor_xf->scaleFactor.setValue(8,0.1f,8);
    floor_xf->translation.setValue(0,-0.5f,0); sg->addChild(floor_xf);
    sg->addChild(new SoCube);
    SoMaterial* sphere_mat = new SoMaterial;
    sphere_mat->diffuseColor.setValue(0.2f,0.4f,0.9f); sg->addChild(sphere_mat);
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
    mat->diffuseColor.setValue(0.5f,0.7f,0.3f); root->addChild(mat);
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
    mat->diffuseColor.setValue(0.9f,0.5f,0.1f); root->addChild(mat);
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
};
static const SceneEntry s_scenes[] = {
    {"primitives",       "Rendering", "Basic primitives: sphere, cube, cone, cylinder", scene_primitives},
    {"materials",        "Rendering", "Material property showcase",                      scene_materials},
    {"lighting",         "Rendering", "Multiple light sources",                          scene_lighting},
    {"transforms",       "Rendering", "Hierarchical transform chain",                    scene_transforms},
    {"colored_cube",     "Rendering", "Simple red cube (smoke test)",                    scene_colored_cube},
    {"coordinates",      "Rendering", "XYZ coordinate axis visualization",               scene_coordinates},
    {"shadow",           "Rendering", "SoShadowGroup shadow casting",                    scene_shadow},
    {"transparency",     "Rendering", "Alpha-blended transparent spheres",               scene_transparency},
    {"lod",              "Rendering", "Level-of-detail SoComplexity comparison",         scene_lod},
    {"drawstyle",        "Rendering", "Filled / wireframe / points draw styles",         scene_drawstyle},
    {"indexed_face_set", "Rendering", "SoIndexedFaceSet tetrahedron",                    scene_indexed_face_set},
};
static const int s_scene_count = (int)(sizeof(s_scenes)/sizeof(s_scenes[0]));

/* =========================================================================
 * Scene state
 * ======================================================================= */
struct SceneState {
    SoSeparator*         root   = nullptr;
    SoPerspectiveCamera* cam    = nullptr;
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
                    /* orbit */
                    float az = -(float)dx * 0.01f;
                    float el =  (float)dy * 0.01f;
                    SbVec3f center(0,0,0);
                    SbVec3f offset = state->cam->position.getValue() - center;
                    SbRotation azR(SbVec3f(0,1,0), az); azR.multVec(offset,offset);
                    SbVec3f viewDir = -offset; viewDir.normalize();
                    SbVec3f right = SbVec3f(0,1,0).cross(viewDir);
                    float rl = right.length();
                    if (rl > 1e-4f) right *= 1.0f/rl; else right = SbVec3f(1,0,0);
                    SbRotation elR(right, el); elR.multVec(offset,offset);
                    state->cam->position.setValue(center+offset);
                    state->cam->pointAt(center, SbVec3f(0,1,0));
                } else if (state->drag_btn == 3) {
                    /* dolly */
                    float dist = state->cam->focalDistance.getValue();
                    dist *= (1.0f + dy * 0.01f);
                    if (dist < 0.1f) dist = 0.1f;
                    SbVec3f dir = state->cam->position.getValue(); dir.normalize();
                    state->cam->position.setValue(dir * dist);
                    state->cam->focalDistance.setValue(dist);
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
                SbVec3f dir = state->cam->position.getValue(); dir.normalize();
                state->cam->position.setValue(dir * dist);
                state->cam->focalDistance.setValue(dist);
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

    void setScene(SoSeparator* r, SoPerspectiveCamera* c) {
        root = r; cam = c; status_text.clear();
        refreshRender();
    }

    void refreshRender() {
        if (!root) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);
        /* renderScene() fills pixels bottom-up in RGBA order. */
        pixel_buf.resize((size_t)pw * ph * 4, 0);
        const float bg[3] = { 0.15f, 0.15f, 0.2f };
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
        if (!root || !cam) return Fl_Box::handle(event);
        switch (event) {
        case FL_PUSH:
            take_focus(); return 1;
        case FL_RELEASE:
            notifyCameraChanged(); refreshRender(); return 1;
        case FL_DRAG: {
            /* Mirror CoinPanel orbit/dolly so sync works correctly. */
            static int last_x = 0, last_y = 0;
            static int drag_btn = 0;
            if (Fl::event_is_click()) { last_x = Fl::event_x(); last_y = Fl::event_y(); drag_btn = Fl::event_button(); }
            int dx = Fl::event_x() - last_x, dy = Fl::event_y() - last_y;
            last_x = Fl::event_x(); last_y = Fl::event_y();
            if (drag_btn == 1) {
                float az = -(float)dx * 0.01f, el = (float)dy * 0.01f;
                SbVec3f center(0,0,0), offset = cam->position.getValue() - center;
                SbRotation(SbVec3f(0,1,0), az).multVec(offset, offset);
                SbVec3f viewDir = -offset; viewDir.normalize();
                SbVec3f right = SbVec3f(0,1,0).cross(viewDir);
                float rl = right.length();
                if (rl > 1e-4f) right *= 1.0f/rl; else right = SbVec3f(1,0,0);
                SbRotation(right, el).multVec(offset, offset);
                cam->position.setValue(center + offset);
                cam->pointAt(center, SbVec3f(0,1,0));
            } else if (drag_btn == 3) {
                float dist = cam->focalDistance.getValue() * (1.0f + dy*0.01f);
                if (dist < 0.1f) dist = 0.1f;
                SbVec3f dir = cam->position.getValue(); dir.normalize();
                cam->position.setValue(dir * dist);
                cam->focalDistance.setValue(dist);
            }
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_MOUSEWHEEL: {
            float dist = cam->focalDistance.getValue() * (1.0f + (float)Fl::event_dy()*0.1f);
            if (dist < 0.1f) dist = 0.1f;
            SbVec3f dir = cam->position.getValue(); dir.normalize();
            cam->position.setValue(dir * dist);
            cam->focalDistance.setValue(dist);
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
            take_focus(); return 1;
        case FL_RELEASE:
            notifyCameraChanged(); refreshRender(); return 1;
        case FL_DRAG: {
            static int last_x = 0, last_y = 0;
            static int drag_btn = 0;
            if (Fl::event_is_click()) { last_x = Fl::event_x(); last_y = Fl::event_y(); drag_btn = Fl::event_button(); }
            int dx = Fl::event_x() - last_x, dy = Fl::event_y() - last_y;
            last_x = Fl::event_x(); last_y = Fl::event_y();
            if (drag_btn == 1) {
                float az = -(float)dx * 0.01f, el = (float)dy * 0.01f;
                SbVec3f center(0,0,0), offset = cam->position.getValue() - center;
                SbRotation(SbVec3f(0,1,0), az).multVec(offset, offset);
                SbVec3f viewDir = -offset; viewDir.normalize();
                SbVec3f right = SbVec3f(0,1,0).cross(viewDir);
                float rl = right.length();
                if (rl > 1e-4f) right *= 1.0f/rl; else right = SbVec3f(1,0,0);
                SbRotation(right, el).multVec(offset, offset);
                cam->position.setValue(center + offset);
                cam->pointAt(center, SbVec3f(0,1,0));
            } else if (drag_btn == 3) {
                float dist = cam->focalDistance.getValue() * (1.0f + dy*0.01f);
                if (dist < 0.1f) dist = 0.1f;
                SbVec3f dir = cam->position.getValue(); dir.normalize();
                cam->position.setValue(dir * dist);
                cam->focalDistance.setValue(dist);
            }
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_MOUSEWHEEL: {
            float dist = cam->focalDistance.getValue() * (1.0f + (float)Fl::event_dy()*0.1f);
            if (dist < 0.1f) dist = 0.1f;
            SbVec3f dir = cam->position.getValue(); dir.normalize();
            cam->position.setValue(dir * dist);
            cam->focalDistance.setValue(dist);
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

    void notifyCameraChanged() { if (on_camera_changed) on_camera_changed(this); }
};
#endif /* OBOL_VIEWER_OSMESA_PANEL */


class ObolViewerWindow : public Fl_Double_Window {
    Fl_Hold_Browser*  browser_;
    CoinPanel*        coin_panel_;
    Fl_Button*        reload_btn_;
    Fl_Button*        save_btn_;
    Fl_Box*           status_bar_;
#ifdef OBOL_VIEWER_OSMESA_PANEL
    OSMesaPanel*      osmesa_panel_ = nullptr;
    Fl_Check_Button*  osmesa_sync_btn_ = nullptr;
    bool              osmesa_syncing_ = false;
#endif
#ifdef OBOL_VIEWER_NANORT
    NanoRTPanel*      nrt_panel_  = nullptr;
    Fl_Check_Button*  sync_btn_   = nullptr;
    bool              syncing_    = false;
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

#ifdef OBOL_VIEWER_OSMESA_PANEL
        /* OSMesa panel shares the same scene root and camera as the Coin panel. */
        if (osmesa_panel_ && coin_panel_->state && coin_panel_->state->root) {
            osmesa_panel_->setScene(coin_panel_->state->root,
                                    coin_panel_->state->cam);
        }
        /* Wire up camera sync: Coin ↔ OSMesa. */
        coin_panel_->on_camera_changed = [this](CoinPanel* src) {
            if (!osmesa_syncing_ && osmesa_sync_btn_ && osmesa_sync_btn_->value()
                && osmesa_panel_) {
                osmesa_syncing_ = true;
                float pos[3], orient[4], dist = 1.0f;
                src->getCamera(pos, orient, dist);
                osmesa_panel_->setCamera(pos, orient, dist);
                osmesa_syncing_ = false;
            }
        };
        if (osmesa_panel_) {
            osmesa_panel_->on_camera_changed = [this](OSMesaPanel* src) {
                if (!osmesa_syncing_ && osmesa_sync_btn_ && osmesa_sync_btn_->value()) {
                    osmesa_syncing_ = true;
                    float pos[3], orient[4], dist = 1.0f;
                    src->getCamera(pos, orient, dist);
                    coin_panel_->setCamera(pos, orient, dist);
                    osmesa_syncing_ = false;
                }
            };
        }
#endif /* OBOL_VIEWER_OSMESA_PANEL */

#ifdef OBOL_VIEWER_NANORT
        /* NanoRT panel shares the same scene root and camera as the Coin
         * panel.  Both renderers traverse the same graph, so camera state
         * set in either panel is immediately visible in the other. */
        if (nrt_panel_ && coin_panel_->state && coin_panel_->state->root) {
            nrt_panel_->setScene(coin_panel_->state->root,
                                 coin_panel_->state->cam);
        }
        /* Wire up cross-panel camera sync callbacks once per load. */
#  ifndef OBOL_VIEWER_OSMESA_PANEL  /* avoid double-wiring coin_panel_ callback */
        coin_panel_->on_camera_changed = [this](CoinPanel* src) {
            if (!syncing_ && sync_btn_ && sync_btn_->value() && nrt_panel_) {
                syncing_ = true;
                float pos[3], orient[4], dist = 1.0f;
                src->getCamera(pos, orient, dist);
                nrt_panel_->setCamera(pos, orient, dist);
                syncing_ = false;
            }
        };
#  endif
        if (nrt_panel_) {
            nrt_panel_->on_camera_changed = [this](NanoRTPanel* src) {
                if (!syncing_ && sync_btn_ && sync_btn_->value()) {
                    syncing_ = true;
                    float pos[3], orient[4], dist = 1.0f;
                    src->getCamera(pos, orient, dist);
                    coin_panel_->setCamera(pos, orient, dist);
                    syncing_ = false;
                }
            };
        }
#endif /* OBOL_VIEWER_NANORT */

        std::string s = "Scene: "; s += name;
        status_bar_->copy_label(s.c_str());
    }

private:
    /* ---- coin panel label, chosen at compile time ---- */
    static const char* coinLabel() {
#if defined(COIN3D_BUILD_DUAL_GL)
        return "System GL";
#elif defined(COIN3D_OSMESA_BUILD)
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

        /* Render area: Fl_Tile so panels can be resized by dragging.
         * Panel layout depends on which optional panels are compiled in:
         *   dual + nanort  → 3 panels: System GL | OSMesa | NanoRT
         *   dual only      → 2 panels: System GL | OSMesa
         *   nanort only    → 2 panels: Coin GL   | NanoRT
         *   neither        → 1 panel:  Coin GL
         */
        Fl_Tile* tile = new Fl_Tile(BROWSER_W, 0, W - BROWSER_W, content_h);
        {
#if defined(OBOL_VIEWER_OSMESA_PANEL) && defined(OBOL_VIEWER_NANORT)
            /* Three panels */
            int panel_w = (W - BROWSER_W) / 3;
            int panel_w2 = (W - BROWSER_W) - 2 * panel_w; // last gets remainder
            coin_panel_   = new CoinPanel(BROWSER_W, 0, panel_w, content_h, coinLabel());
            osmesa_panel_ = new OSMesaPanel(BROWSER_W + panel_w, 0,
                                            panel_w, content_h,
                                            "OSMesa (per-renderer backend)");
            nrt_panel_    = new NanoRTPanel(BROWSER_W + 2*panel_w, 0,
                                            panel_w2, content_h,
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
        tile->end();

        /* Toolbar */
        Fl_Group* tb = new Fl_Group(0, content_h, W, TOOLBAR_H);
        tb->box(FL_UP_BOX);
        {
            int bx = 6, by = content_h+4, bh = TOOLBAR_H-8;
            reload_btn_ = new Fl_Button(bx, by, 70, bh, "Reload");
            reload_btn_->callback(reloadCB, this); bx += 76;
            save_btn_ = new Fl_Button(bx, by, 80, bh, "Save RGB...");
            save_btn_->callback(saveCB, this); bx += 86;
#ifdef OBOL_VIEWER_OSMESA_PANEL
            osmesa_sync_btn_ = new Fl_Check_Button(bx, by, 120, bh, "Sync GL+OSMesa");
            osmesa_sync_btn_->value(1); bx += 126;
#endif
#ifdef OBOL_VIEWER_NANORT
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
