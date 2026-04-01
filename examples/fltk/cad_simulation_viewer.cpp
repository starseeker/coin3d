/*
 * cad_simulation_viewer.cpp  –  Interactive FLTK demo: Slider-Crank CAD simulation
 *
 * Overview
 * ────────
 * This application demonstrates a slider-crank mechanism, one of the
 * fundamental kinematic primitives in CAD/mechanical simulation.  The
 * scene is built directly with Obol scene-graph nodes; no testlib or
 * ObolEx dependency is required.
 *
 * Mechanism
 * ─────────
 *   • Crankshaft   – main journal cylinder (Z-axis) at the origin
 *   • Crank arm    – cylinder from origin to crank pin, rotates around Z
 *   • Crank pin    – gold sphere at the tip of the arm
 *   • Connecting rod – cylinder from crank pin to piston pin
 *   • Piston       – orange box that slides along the X axis
 *   • Cylinder bore – transparent guide channel along the X axis
 *   • Ground plate  – grey base plate
 *
 * Kinematics (standard slider-crank formulae)
 * ───────────────────────────────────────────
 *   crank-pin position  :  px = R·cos(α),  py = R·sin(α)
 *   piston X            :  qx = px + √(L² − py²)
 *   rod direction (Y→)  :  SbRotation(Y+, normalised(piston_pin − crank_pin))
 *   rod centre          :  midpoint of crank pin and piston pin
 *
 * Controls
 * ────────
 *   Left-drag   → orbit camera (BRL-CAD style: local yaw/pitch)
 *   Right-drag  → dolly (zoom in/out)
 *   Scroll      → zoom
 *   [Play/Pause]→ start / stop animation
 *   Speed slider→ animation speed  (0.1× – 5.0×)
 *   [Proj]      → toggle projection (Perspective ↔ Orthographic)
 *   [Reset View]→ restore default camera
 *
 * Building
 * ────────
 *   Enabled via  -DOBOL_BUILD_CAD_SIM_VIEWER=ON  (default: follows
 *   OBOL_BUILD_VIEWER).  Requires FLTK.
 */

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbRotation.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbTime.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>

/* BasicFLTKContextManager – FLTK GL context for off-screen rendering */
#include "fltk_context_manager.h"

/* FLTK */
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Mechanism parameters
 * ========================================================================= */

static constexpr float CRANK_RADIUS      = 0.8f;   ///< length of crank arm (R)
static constexpr float CONN_ROD_LENGTH   = 2.2f;   ///< connecting rod length (L)
static constexpr float CRANK_JOURNAL_R   = 0.10f;  ///< crankshaft journal radius
static constexpr float CRANK_ARM_R       = 0.07f;  ///< crank arm cylinder radius
static constexpr float CONN_ROD_R        = 0.05f;  ///< connecting rod radius
static constexpr float PISTON_W          = 0.50f;  ///< piston box width (X)
static constexpr float PISTON_H          = 0.45f;  ///< piston box height (Y)
static constexpr float PISTON_D          = 0.45f;  ///< piston box depth (Z)
static constexpr float PIN_R             = 0.10f;  ///< crank/piston pin sphere radius
static constexpr float FRAME_INTERVAL    = 1.0 / 30.0;  ///< animation period (s)

/* =========================================================================
 * Animation state (global for timer callback access)
 * ========================================================================= */

static float s_crank_angle = 0.0f;  ///< current crank angle (radians)
static float s_speed       = 1.0f;  ///< revolutions per second
static bool  s_running     = true;

/* =========================================================================
 * Scene node handles (updated every frame)
 * ========================================================================= */

struct MechanismNodes {
    SoSeparator* root         = nullptr;
    SoCamera*    camera       = nullptr;
    SoTransform*         crankXf      = nullptr;  ///< crank arm rotation (Z-axis)
    SoTransform*         connRodXf    = nullptr;  ///< rod position + tilt
    SoTransform*         pistonXf     = nullptr;  ///< piston X translation
    SoTransform*         pistonPinXf  = nullptr;  ///< piston-pin sphere position
};

static MechanismNodes s_nodes;

/* =========================================================================
 * Scene-graph helpers
 * ========================================================================= */

/** Return a ref-counted separator containing a material + SoCylinder. */
static SoSeparator* makeCylinder(float r, float g, float b,
                                  float shininess,
                                  float radius, float height)
{
    SoSeparator* sep = new SoSeparator;
    SoMaterial*  mat = new SoMaterial;
    mat->diffuseColor.setValue(r, g, b);
    mat->specularColor.setValue(0.6f * r + 0.4f, 0.6f * g + 0.4f, 0.6f * b + 0.4f);
    mat->shininess.setValue(shininess);
    sep->addChild(mat);
    SoCylinder* cyl = new SoCylinder;
    cyl->radius.setValue(radius);
    cyl->height.setValue(height);
    sep->addChild(cyl);
    return sep;
}

/** Return a ref-counted separator containing a material + SoCube. */
static SoSeparator* makeBox(float r, float g, float b,
                              float shininess,
                              float w, float h, float d)
{
    SoSeparator* sep = new SoSeparator;
    SoMaterial*  mat = new SoMaterial;
    mat->diffuseColor.setValue(r, g, b);
    mat->specularColor.setValue(0.5f, 0.5f, 0.5f);
    mat->shininess.setValue(shininess);
    sep->addChild(mat);
    SoCube* box = new SoCube;
    box->width.setValue(w);
    box->height.setValue(h);
    box->depth.setValue(d);
    sep->addChild(box);
    return sep;
}

/** Return a ref-counted separator containing a material + SoSphere. */
static SoSeparator* makeSphere(float r, float g, float b,
                                 float shininess, float radius)
{
    SoSeparator* sep = new SoSeparator;
    SoMaterial*  mat = new SoMaterial;
    mat->diffuseColor.setValue(r, g, b);
    mat->specularColor.setValue(0.8f, 0.8f, 0.5f);
    mat->shininess.setValue(shininess);
    sep->addChild(mat);
    SoSphere* sph = new SoSphere;
    sph->radius.setValue(radius);
    sep->addChild(sph);
    return sep;
}

/* =========================================================================
 * Build the slider-crank scene graph
 *
 * Geometry layout (side view, Y up, X right):
 *
 *   Crank centre is at the origin.
 *   At α=0, crank pin is at (+R, 0) – rightmost position (TDC).
 *   Piston slides along X; piston pin is always at (qx, 0).
 *   Ground plate centred near (R/2, –R–0.4, 0).
 * ========================================================================= */
static MechanismNodes buildScene(int width, int height)
{
    MechanismNodes nodes;

    SoSeparator* root = new SoSeparator;
    root->ref();
    nodes.root = root;

    /* ---- Camera ---- */
    SoPerspectiveCamera* cam = new SoPerspectiveCamera;
    cam->position.setValue(1.5f, 0.8f, 8.0f);
    cam->pointAt(SbVec3f(1.5f, 0.0f, 0.0f), SbVec3f(0, 1, 0));
    root->addChild(cam);
    nodes.camera = cam;

    /* ---- Two directional lights ---- */
    {
        SoDirectionalLight* lt = new SoDirectionalLight;
        lt->direction.setValue(-1.0f, -1.5f, -1.0f);
        lt->color.setValue(1.0f, 1.0f, 0.95f);
        lt->intensity.setValue(0.85f);
        root->addChild(lt);
    }
    {
        SoDirectionalLight* lt = new SoDirectionalLight;
        lt->direction.setValue(1.0f, 0.5f, 0.8f);
        lt->color.setValue(0.4f, 0.5f, 0.8f);
        lt->intensity.setValue(0.30f);
        root->addChild(lt);
    }

    /* ---- Ground plate ---- */
    {
        SoSeparator* gnd = new SoSeparator;
        SoTransform* xf  = new SoTransform;
        /* centre the plate under the crank + piston assembly */
        xf->translation.setValue(1.5f, -(CRANK_RADIUS + 0.45f), 0.0f);
        gnd->addChild(xf);
        gnd->addChild(makeBox(0.42f, 0.42f, 0.46f, 0.35f,
                               CONN_ROD_LENGTH + CRANK_RADIUS * 2.0f + 1.5f,
                               0.16f, 0.9f));
        root->addChild(gnd);
    }

    /* ---- Crankshaft bearing pillars ---- */
    for (int side : {-1, 1}) {
        SoSeparator* pillar = new SoSeparator;
        SoTransform* xf     = new SoTransform;
        float pillarH = CRANK_RADIUS + 0.35f;
        xf->translation.setValue((float)side * 0.28f,
                                  -(pillarH * 0.5f + 0.08f), 0.0f);
        pillar->addChild(xf);
        pillar->addChild(makeBox(0.50f, 0.50f, 0.55f, 0.4f,
                                  0.18f, pillarH, 0.50f));
        root->addChild(pillar);
    }

    /* ---- Crankshaft journal (main shaft, Z-axis cylinder) ---- */
    {
        SoSeparator* shaft = new SoSeparator;
        SoTransform* xf    = new SoTransform;
        /* rotate default Y-cylinder to Z alignment */
        xf->rotation.setValue(SbVec3f(1, 0, 0), (float)(M_PI / 2.0));
        shaft->addChild(xf);
        shaft->addChild(makeCylinder(0.60f, 0.62f, 0.70f, 0.6f,
                                      CRANK_JOURNAL_R, 0.70f));
        root->addChild(shaft);
    }

    /* ---- Crank assembly (arm + pin) – rotates around Z ---- */
    {
        SoSeparator* crankAssembly = new SoSeparator;

        /* This transform is updated each frame to rotate the arm */
        SoTransform* crankXf = new SoTransform;
        crankAssembly->addChild(crankXf);
        nodes.crankXf = crankXf;

        /* Crank arm: Y-aligned cylinder from origin to (0, R, 0).
         * Centre it at (0, R/2, 0). */
        SoSeparator* arm = new SoSeparator;
        SoTransform* armXf = new SoTransform;
        armXf->translation.setValue(0.0f, CRANK_RADIUS * 0.5f, 0.0f);
        arm->addChild(armXf);
        arm->addChild(makeCylinder(0.80f, 0.25f, 0.20f, 0.55f,
                                    CRANK_ARM_R, CRANK_RADIUS));
        crankAssembly->addChild(arm);

        /* Crank pin sphere at (0, R, 0) in local frame */
        SoSeparator* pin = new SoSeparator;
        SoTransform* pinXf = new SoTransform;
        pinXf->translation.setValue(0.0f, CRANK_RADIUS, 0.0f);
        pin->addChild(pinXf);
        pin->addChild(makeSphere(0.90f, 0.75f, 0.10f, 0.80f, PIN_R));
        crankAssembly->addChild(pin);

        root->addChild(crankAssembly);
    }

    /* ---- Connecting rod (position/orientation updated each frame) ---- */
    {
        SoSeparator* rod = new SoSeparator;
        SoTransform* rodXf = new SoTransform;
        rod->addChild(rodXf);
        nodes.connRodXf = rodXf;
        rod->addChild(makeCylinder(0.25f, 0.65f, 0.30f, 0.55f,
                                    CONN_ROD_R, CONN_ROD_LENGTH));
        root->addChild(rod);
    }

    /* ---- Cylinder bore / guide channel (transparent) ---- */
    {
        /* TDC piston X: CRANK_RADIUS + CONN_ROD_LENGTH = 3.0
         * BDC piston X: CONN_ROD_LENGTH - CRANK_RADIUS = 1.4
         * Centre of bore: (TDC + BDC) / 2 = 2.2; length = TDC - BDC = 1.6 */
        float bore_cx     = (CRANK_RADIUS + CONN_ROD_LENGTH +
                              CONN_ROD_LENGTH - CRANK_RADIUS) * 0.5f;
        float bore_length = 2.0f * CRANK_RADIUS + 0.5f;  /* stroke + margin */

        SoSeparator* bore = new SoSeparator;
        SoTransform* xf   = new SoTransform;
        xf->translation.setValue(bore_cx, 0.0f, 0.0f);
        /* Rotate Y-cylinder to X alignment */
        xf->rotation.setValue(SbVec3f(0, 0, 1), (float)(M_PI / 2.0));
        bore->addChild(xf);

        SoMaterial* boreMat = new SoMaterial;
        boreMat->diffuseColor.setValue(0.35f, 0.35f, 0.60f);
        boreMat->transparency.setValue(0.62f);
        bore->addChild(boreMat);

        SoCylinder* boreCyl = new SoCylinder;
        boreCyl->radius.setValue(PISTON_H * 0.52f);
        boreCyl->height.setValue(bore_length);
        bore->addChild(boreCyl);
        root->addChild(bore);
    }

    /* ---- Piston box (updated each frame) ---- */
    {
        SoSeparator* piston = new SoSeparator;
        SoTransform* pistonXf = new SoTransform;
        piston->addChild(pistonXf);
        nodes.pistonXf = pistonXf;
        piston->addChild(makeBox(0.75f, 0.40f, 0.18f, 0.50f,
                                  PISTON_W, PISTON_H, PISTON_D));
        root->addChild(piston);
    }

    /* ---- Piston-pin sphere (shows connection point, updated each frame) ---- */
    {
        SoSeparator* ppinSep = new SoSeparator;
        SoTransform* ppinXf  = new SoTransform;
        ppinSep->addChild(ppinXf);
        nodes.pistonPinXf = ppinXf;
        ppinSep->addChild(makeSphere(0.90f, 0.75f, 0.10f, 0.80f, PIN_R));
        root->addChild(ppinSep);
    }

    /* ---- Initial viewAll then pull back slightly ---- */
    SbViewportRegion vp((short)width, (short)height);
    cam->viewAll(root, vp);
    SbVec3f pos = cam->position.getValue();
    cam->position.setValue(pos * 1.35f);
    float dist = (pos * 1.35f - SbVec3f(1.5f, 0.0f, 0.0f)).length();
    cam->focalDistance.setValue(dist);
    cam->nearDistance.setValue(dist * 0.001f);
    cam->farDistance.setValue(dist * 1000.0f);

    return nodes;
}

/* =========================================================================
 * Kinematic update — called each frame
 *
 * At angle α:
 *   crank-pin   (px, py) = (R·cos α,  R·sin α)
 *   piston pin       qx  = px + √(L² − py²)
 * ========================================================================= */
static float updateKinematics(float alpha)
{
    if (!s_nodes.root) return 0.0f;

    /* Rotate crank arm (tip at (0,R,0) in local frame).
     * With rotation angle (alpha − π/2) around Z the local tip maps to
     * world (R·cos α, R·sin α, 0). */
    s_nodes.crankXf->rotation.setValue(SbVec3f(0, 0, 1),
                                        alpha - (float)(M_PI / 2.0));

    /* Crank-pin world position */
    float px = CRANK_RADIUS * std::cos(alpha);
    float py = CRANK_RADIUS * std::sin(alpha);

    /* Piston pin: constrained to Y = 0 */
    float sinBeta = py / CONN_ROD_LENGTH;
    sinBeta = std::max(-1.0f, std::min(1.0f, sinBeta));
    float cosBeta = std::sqrt(1.0f - sinBeta * sinBeta);
    float qx = px + CONN_ROD_LENGTH * cosBeta;

    /* Update piston & piston pin positions */
    s_nodes.pistonXf->translation.setValue(qx, 0.0f, 0.0f);
    if (s_nodes.pistonPinXf)
        s_nodes.pistonPinXf->translation.setValue(qx, 0.0f, 0.0f);

    /* Connecting rod:
     *   centre at midpoint of crank pin and piston pin
     *   rotate Y-cylinder to align with direction from crank pin to piston pin */
    float rcx = (px + qx) * 0.5f;
    float rcy = py * 0.5f;

    SbVec3f rodDir(qx - px, -py, 0.0f);
    /* rodDir has length L; normalize before passing to setValue(from,to) */
    if (rodDir.length() > 1e-6f) rodDir.normalize();

    SbRotation rodRot;
    rodRot.setValue(SbVec3f(0, 1, 0), rodDir);
    s_nodes.connRodXf->translation.setValue(rcx, rcy, 0.0f);
    s_nodes.connRodXf->rotation = rodRot;

    return qx;  /* caller uses this for status display */
}

/* =========================================================================
 * Off-screen renderer singleton
 * ========================================================================= */

static BasicFLTKContextManager* s_ctx_mgr = nullptr;
static SoOffscreenRenderer*     s_renderer = nullptr;

static SoOffscreenRenderer* getRenderer(int w, int h)
{
    if (!s_renderer) {
        SbViewportRegion vp(w, h);
        /* Pass the same BasicFLTKContextManager that was registered with
         * SoDB::init() so the renderer uses the FLTK GL window context. */
        s_renderer = new SoOffscreenRenderer(s_ctx_mgr, vp);
    }
    return s_renderer;
}

/* =========================================================================
 * RenderPanel  –  FLTK box that renders the Obol scene via off-screen path
 * ========================================================================= */

class RenderPanel : public Fl_Box {
public:
    explicit RenderPanel(int X, int Y, int W, int H)
        : Fl_Box(X, Y, W, H, "")
        , fltk_img_(nullptr)
        , dragging_(false), drag_btn_(0), last_x_(0), last_y_(0)
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~RenderPanel() { delete fltk_img_; }

    /** Recompute scene bounding-box centre for orbit pivot. */
    void updateSceneCenter()
    {
        if (!s_nodes.root) return;
        SoGetBoundingBoxAction bba(SbViewportRegion(w(), h()));
        bba.apply(s_nodes.root);
        SbBox3f bbox = bba.getBoundingBox();
        if (!bbox.isEmpty())
            scene_center_ = bbox.getCenter();
        else
            scene_center_.setValue(1.5f, 0.0f, 0.0f);
    }

    /** Render the current scene and update the displayed image. */
    void refreshRender()
    {
        if (!s_nodes.root) { redraw(); return; }

        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);

        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.10f, 0.11f, 0.18f));
        r->clearBackgroundGradient();

        if (!r->render(s_nodes.root)) {
            status_ = "Render failed (no GL context)";
            redraw();
            return;
        }

        const unsigned char* src = r->getBuffer();
        if (!src) { redraw(); return; }

        /* SoOffscreenRenderer fills bottom-to-top RGBA; FLTK wants top-down RGB. */
        display_buf_.resize(static_cast<size_t>(pw) * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = src + static_cast<size_t>(ph - 1 - row) * pw * 4;
            uint8_t*       d = display_buf_.data() + static_cast<size_t>(row) * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                s += 4; d += 3;
            }
        }

        delete fltk_img_;
        fltk_img_ = new Fl_RGB_Image(display_buf_.data(), pw, ph, 3);
        status_.clear();
        redraw();
    }

    /** Save the last rendered frame to an SGI RGB file. */
    void saveRGB()
    {
        if (!s_nodes.root) { fl_message("No scene loaded."); return; }
        const char* path = fl_file_chooser("Save snapshot", "*.rgb", "cad_sim.rgb");
        if (!path) return;
        int pw = std::max(w(), 1), ph = std::max(h(), 1);
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.10f, 0.11f, 0.18f));
        r->clearBackgroundGradient();
        if (r->render(s_nodes.root) && r->writeToRGB(path))
            fl_message("Saved: %s", path);
        else
            fl_message("Save failed.");
    }

    void draw() override
    {
        fl_rectf(x(), y(), w(), h(), FL_BLACK);
        if (fltk_img_)
            fltk_img_->draw(x(), y(), w(), h(), 0, 0);
        else {
            fl_color(FL_WHITE); fl_font(FL_HELVETICA, 14);
            fl_draw("(no scene)", x() + w()/2 - 42, y() + h()/2);
        }
        if (!status_.empty()) {
            fl_color(fl_rgb_color(255, 80, 80)); fl_font(FL_HELVETICA, 12);
            fl_draw(status_.c_str(), x() + 6, y() + h() - 6);
        }
    }

    int handle(int event) override
    {
        if (!s_nodes.camera) return Fl_Box::handle(event);
        const int ev_x = Fl::event_x() - x();
        const int ev_y = Fl::event_y() - y();

        switch (event) {
        case FL_PUSH:
            take_focus();
            dragging_  = true;
            drag_btn_  = Fl::event_button();
            last_x_    = ev_x;
            last_y_    = ev_y;
            return 1;

        case FL_RELEASE:
            dragging_ = false;
            return 1;

        case FL_DRAG:
            if (dragging_) {
                int dx = ev_x - last_x_;
                int dy = ev_y - last_y_;
                last_x_ = ev_x;
                last_y_ = ev_y;

                if (drag_btn_ == 1) {
                    /* Orbit – BRL-CAD style: camera-local yaw and pitch */
                    s_nodes.camera->orbitCamera(scene_center_,
                                                (float)dx, (float)dy,
                                                0.25f);
                    updateClipping();
                } else if (drag_btn_ == 3) {
                    /* Dolly */
                    float dist = s_nodes.camera->focalDistance.getValue();
                    dist *= (1.0f + dy * 0.01f);
                    dist = std::max(0.1f, dist);
                    SbVec3f dir = s_nodes.camera->position.getValue()
                                  - scene_center_;
                    dir.normalize();
                    s_nodes.camera->position.setValue(scene_center_ + dir * dist);
                    s_nodes.camera->focalDistance.setValue(dist);
                    updateClipping();
                }

                /* Re-render on camera move only when paused; the animation
                 * timer will redraw when running. */
                if (!s_running) refreshRender();
            }
            return 1;

        case FL_MOUSEWHEEL: {
            float delta = -(float)Fl::event_dy();
            float dist  = s_nodes.camera->focalDistance.getValue();
            dist *= (1.0f - delta * 0.10f);
            dist = std::max(0.1f, dist);
            SbVec3f dir = s_nodes.camera->position.getValue() - scene_center_;
            dir.normalize();
            s_nodes.camera->position.setValue(scene_center_ + dir * dist);
            s_nodes.camera->focalDistance.setValue(dist);
            updateClipping();
            if (!s_running) refreshRender();
            return 1;
        }

        case FL_FOCUS: case FL_UNFOCUS:
            return 1;

        default:
            break;
        }
        return Fl_Box::handle(event);
    }

    void resize(int X, int Y, int W, int H) override
    {
        Fl_Box::resize(X, Y, W, H);
        if (!s_running) refreshRender();
    }

private:
    void updateClipping()
    {
        if (!s_nodes.camera) return;
        float d = s_nodes.camera->focalDistance.getValue();
        d = std::max(d, 1e-4f);
        s_nodes.camera->nearDistance.setValue(d * 0.001f);
        s_nodes.camera->farDistance.setValue(d * 1000.0f);
    }

    std::vector<uint8_t> display_buf_;
    Fl_RGB_Image*        fltk_img_;
    std::string          status_;
    bool                 dragging_;
    int                  drag_btn_, last_x_, last_y_;
    SbVec3f              scene_center_{1.5f, 0.0f, 0.0f};
};

/* =========================================================================
 * StatusBar  –  one-line bottom bar showing crank angle and piston position
 * ========================================================================= */

class StatusBar : public Fl_Box {
public:
    explicit StatusBar(int X, int Y, int W, int H)
        : Fl_Box(X, Y, W, H, "")
    {
        box(FL_FLAT_BOX);
        color(fl_rgb_color(28, 30, 42));
        align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        labelsize(11);
        labelcolor(fl_rgb_color(170, 220, 170));
    }

    void update(float alpha, float pistonX)
    {
        float deg = alpha * (float)(180.0 / M_PI);
        /* Normalize to 0–360 */
        deg = std::fmod(deg, 360.0f);
        if (deg < 0.0f) deg += 360.0f;
        snprintf(buf_, sizeof(buf_),
                 "  Crank angle: %6.1f°   |   Piston X: %+.3f   |  %s",
                 deg, pistonX,
                 s_running ? "\u25B6 RUNNING" : "\u23F8 PAUSED");
        label(buf_);
        redraw();
    }

private:
    char buf_[256] = {};
};

/* =========================================================================
 * Globals shared between the animation timer and the UI widgets
 * ========================================================================= */

static RenderPanel* s_panel  = nullptr;
static StatusBar*   s_status = nullptr;
static bool s_perspectiveCamera = true;  // true=perspective, false=orthographic

/** Switch the scene camera between perspective and orthographic projection.
 *  Preserves position, orientation, focal distance, and near/far planes. */
static void switchCameraType(bool toPerspective)
{
    if (!s_nodes.root || !s_nodes.camera) return;

    SbVec3f    pos   = s_nodes.camera->position.getValue();
    SbRotation ori   = s_nodes.camera->orientation.getValue();
    float      focal = s_nodes.camera->focalDistance.getValue();
    float      nearD = s_nodes.camera->nearDistance.getValue();
    float      farD  = s_nodes.camera->farDistance.getValue();

    SoCamera* newCam = nullptr;
    if (toPerspective) {
        SoPerspectiveCamera* pcam = new SoPerspectiveCamera;
        SoOrthographicCamera* ocam = dynamic_cast<SoOrthographicCamera*>(s_nodes.camera);
        if (ocam && focal > 1e-6f)
            pcam->heightAngle.setValue(2.0f * std::atan(ocam->height.getValue() / (2.0f * focal)));
        newCam = pcam;
    } else {
        SoOrthographicCamera* ocam = new SoOrthographicCamera;
        SoPerspectiveCamera* pcam = dynamic_cast<SoPerspectiveCamera*>(s_nodes.camera);
        if (pcam)
            ocam->height.setValue(2.0f * focal * std::tan(pcam->heightAngle.getValue() * 0.5f));
        newCam = ocam;
    }

    newCam->position.setValue(pos);
    newCam->orientation.setValue(ori);
    newCam->focalDistance.setValue(focal);
    newCam->nearDistance.setValue(nearD);
    newCam->farDistance.setValue(farD);

    s_nodes.root->replaceChild(s_nodes.camera, newCam);
    s_nodes.camera = newCam;
}

/* =========================================================================
 * Animation timer callback – fires at FRAME_INTERVAL seconds
 * ========================================================================= */

static void animTimerCB(void* /*data*/)
{
    if (s_running) {
        /* Advance crank angle by one frame's worth of rotation */
        float dangle = static_cast<float>(2.0 * M_PI * s_speed * FRAME_INTERVAL);
        s_crank_angle += dangle;
        if (s_crank_angle >= (float)(2.0 * M_PI))
            s_crank_angle -= (float)(2.0 * M_PI);
    }

    float pistonX = updateKinematics(s_crank_angle);

    if (s_panel)  s_panel->refreshRender();
    if (s_status) s_status->update(s_crank_angle, pistonX);

    Fl::repeat_timeout(FRAME_INTERVAL, animTimerCB, nullptr);
}

/* =========================================================================
 * CadSimWindow  –  main application window
 *
 * Layout:
 *  ┌──────────────────────────────────────────────────────┐
 *  │                RenderPanel (3-D view)                │
 *  ├──────────────────────────────────────────────────────┤
 *  │ [▶/⏸]  Speed: [slider]  [Proj] [Reset View] [Save] │
 *  ├──────────────────────────────────────────────────────┤
 *  │ Crank angle: NNN°  |  Piston X: ±N.NNN              │
 *  └──────────────────────────────────────────────────────┘
 * ========================================================================= */

class CadSimWindow : public Fl_Double_Window {
public:
    static constexpr int TOOLBAR_H = 38;
    static constexpr int STATUS_H  = 22;

    CadSimWindow(int W, int H)
        : Fl_Double_Window(W, H,
              "Obol CAD Simulation  \xe2\x80\x93  Slider-Crank Mechanism")
    {
        int panel_h = H - TOOLBAR_H - STATUS_H;

        /* Rendering panel */
        s_panel = new RenderPanel(0, 0, W, panel_h);

        /* Toolbar */
        Fl_Group* tb = new Fl_Group(0, panel_h, W, TOOLBAR_H);
        tb->box(FL_FLAT_BOX);
        tb->color(fl_rgb_color(38, 40, 54));
        {
            play_btn_ = new Fl_Button(6, panel_h + 5, 86, 28, "\xe2\x8f\xb8  Pause");
            play_btn_->callback(playCB, this);
            play_btn_->labelsize(12);
            play_btn_->color(fl_rgb_color(70, 95, 145));
            play_btn_->labelcolor(FL_WHITE);

            Fl_Box* spd_lbl = new Fl_Box(98, panel_h + 5, 50, 28, "Speed:");
            spd_lbl->labelsize(11);
            spd_lbl->labelcolor(fl_rgb_color(190, 190, 190));
            spd_lbl->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

            speed_slider_ = new Fl_Value_Slider(152, panel_h + 9, 200, 20, "");
            speed_slider_->type(FL_HOR_SLIDER);
            speed_slider_->bounds(0.1, 5.0);
            speed_slider_->value(1.0);
            speed_slider_->callback(speedCB, this);
            speed_slider_->color(fl_rgb_color(55, 58, 78));
            speed_slider_->selection_color(fl_rgb_color(90, 130, 200));

            proj_btn_ = new Fl_Button(360, panel_h + 5, 110, 28, "Proj: Persp");
            proj_btn_->callback(projCB, this);
            proj_btn_->labelsize(11);
            proj_btn_->color(fl_rgb_color(50, 110, 120));
            proj_btn_->labelcolor(FL_WHITE);

            Fl_Button* reset = new Fl_Button(W - 190, panel_h + 5, 88, 28, "Reset View");
            reset->callback(resetCB, this);
            reset->labelsize(11);
            reset->color(fl_rgb_color(55, 78, 100));
            reset->labelcolor(FL_WHITE);

            Fl_Button* save = new Fl_Button(W - 98, panel_h + 5, 92, 28, "Save RGB...");
            save->callback(saveCB, this);
            save->labelsize(11);
            save->color(fl_rgb_color(55, 78, 100));
            save->labelcolor(FL_WHITE);
        }
        tb->end();

        /* Status bar */
        s_status = new StatusBar(0, panel_h + TOOLBAR_H, W, STATUS_H);

        end();
        resizable(s_panel);
    }

    /** Build the scene and start the animation loop.  Call after show(). */
    void initScene()
    {
        const int pw = std::max(s_panel->w(), 1);
        const int ph = std::max(s_panel->h(), 1);
        s_nodes = buildScene(pw, ph);
        // Restore chosen projection type after rebuild
        if (!s_perspectiveCamera)
            switchCameraType(false);
        s_panel->updateSceneCenter();
        float pistonX = updateKinematics(s_crank_angle);
        s_panel->refreshRender();
        if (s_status) s_status->update(s_crank_angle, pistonX);
        Fl::add_timeout(FRAME_INTERVAL, animTimerCB, nullptr);
    }

private:
    Fl_Button*       play_btn_     = nullptr;
    Fl_Value_Slider* speed_slider_ = nullptr;
    Fl_Button*       proj_btn_     = nullptr;

    /* ---- callbacks ---- */

    static void playCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<CadSimWindow*>(data);
        s_running = !s_running;
        if (self->play_btn_) {
            self->play_btn_->label(s_running
                                    ? "\xe2\x8f\xb8  Pause"
                                    : "\xe2\x96\xb6  Play");
            self->play_btn_->color(s_running
                                    ? fl_rgb_color(70, 95, 145)
                                    : fl_rgb_color(55, 130, 60));
            self->play_btn_->redraw();
        }
    }

    static void speedCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<CadSimWindow*>(data);
        if (self->speed_slider_)
            s_speed = (float)self->speed_slider_->value();
    }

    static void projCB(Fl_Widget*, void* data)
    {
        auto* self = static_cast<CadSimWindow*>(data);
        s_perspectiveCamera = !s_perspectiveCamera;
        switchCameraType(s_perspectiveCamera);
        if (self->proj_btn_) {
            self->proj_btn_->label(s_perspectiveCamera ? "Proj: Persp" : "Proj: Ortho");
            self->proj_btn_->color(s_perspectiveCamera
                ? fl_rgb_color(50, 110, 120)
                : fl_rgb_color(120, 90, 40));
            self->proj_btn_->redraw();
        }
        if (!s_running && s_panel) s_panel->refreshRender();
    }

    static void resetCB(Fl_Widget*, void* /*data*/)
    {
        if (!s_nodes.root || !s_nodes.camera) return;
        if (!s_panel) return;

        /* Rebuild the scene with a fresh default camera */
        const int pw = std::max(s_panel->w(), 1);
        const int ph = std::max(s_panel->h(), 1);
        float savedAngle = s_crank_angle;

        s_nodes.root->unref();
        s_nodes = buildScene(pw, ph);
        // Restore chosen projection type after rebuild
        if (!s_perspectiveCamera)
            switchCameraType(false);
        s_crank_angle = savedAngle;
        s_panel->updateSceneCenter();
        float pistonX = updateKinematics(s_crank_angle);
        s_panel->refreshRender();
        if (s_status) s_status->update(s_crank_angle, pistonX);
    }

    static void saveCB(Fl_Widget*, void* /*data*/)
    {
        if (s_panel) s_panel->saveRGB();
    }
};

/* =========================================================================
 * main
 * ========================================================================= */

int main(int argc, char** argv)
{
    /* Create the BasicFLTKContextManager first – it must exist before
     * SoDB::init() so the manager is live when SoDB registers it. */
    BasicFLTKContextManager basicMgr;
    s_ctx_mgr = &basicMgr;

    SoDB::init(&basicMgr);
    SoDB::setRealTimeInterval(SbTime(FRAME_INTERVAL));
    SoNodeKit::init();
    SoInteraction::init();

    Fl::scheme("gtk+");
    if (!Fl::visual(FL_RGB | FL_DOUBLE | FL_DEPTH))
        Fl::visual(FL_RGB | FL_DOUBLE);

    CadSimWindow* win = new CadSimWindow(960, 700);
    win->show(argc, argv);
    win->wait_for_expose();

    /* Build the scene graph after the window is on screen so that the FLTK
     * GL context window for BasicFLTKContextManager is already visible. */
    win->initScene();

    return Fl::run();
}
