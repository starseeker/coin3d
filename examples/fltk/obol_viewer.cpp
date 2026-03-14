/*
 * obol_viewer.cpp  –  FLTK scene viewer for Obol (direct library use)
 *
 * Architecture
 * ────────────
 * The viewer links directly against the Coin shared library and uses Obol's
 * public API to build and render scenes into off-screen pixel buffers
 * displayed via FLTK.
 *
 * No bridge libraries or dlopen are involved.  The context manager used for
 * the main (System GL) panel is selected at compile time via CMake:
 *
 *   OBOL_VIEWER_FLTK_GL (default ON)
 *     fltk_context_manager.h: FLTKContextManager provides the OpenGL context
 *     via a small hidden 1×1 Fl_Gl_Window.  CoinPanel is a plain Fl_Box like
 *     all other panels; rendered pixels are delivered via fltk_img->draw().
 *     This is portable across all platforms that FLTK supports (Linux/X11,
 *     Windows, macOS) without requiring Xvfb or a direct X11 connection.
 *
 *   OBOL_VIEWER_USE_FLTK_GL=OFF (legacy / CI fallback)
 *     headless_utils.h: GLX pbuffer/pixmap/window fallback on X11/Linux,
 *     or a stub context manager on other platforms.  Use this if FLTK's
 *     OpenGL cannot obtain a context in the CI environment.
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
 * CPU raytracing optional panels (NanoRT and/or Embree)
 * ──────────────────────────────────────────────────────
 * When OBOL_VIEWER_EMBREE is defined at compile time (set by CMake when
 * Embree 4 is found and OBOL_VIEWER_USE_EMBREE=ON), a CPU-raytracing panel
 * is available using SoEmbreeContextManager::renderScene().
 * When OBOL_VIEWER_NANORT is defined (external/nanort/nanort.h found), a
 * NanoRT CPU-raytracing panel is also available.  Both can be compiled in
 * simultaneously; each has its own runtime toggle checkbox.
 * Scenes that require GL-only features are flagged nanort_ok / embree_ok =
 * false in the scene catalogue and show "Not supported".  SoText2 nodes are
 * rendered as coloured billboard quads by both backends.
 *
 * Runtime panel control
 * ─────────────────────
 * All compiled-in panels are created at startup; command-line flags select
 * the initial visible set.  Per-panel checkboxes allow toggling at runtime.
 *
 *   obol_viewer                            – all compiled-in panels visible
 *   obol_viewer --system-enable            – only System GL panel visible
 *   obol_viewer --system-enable \
 *               --osmesa-enable            – System GL + OSMesa visible
 *   obol_viewer --osmesa-enable \
 *               --nanort-enable            – OSMesa + NanoRT visible
 *
 * Layout (all four panels compiled in, all visible)
 * ─────────────────────────────────────────────────
 *  ┌──────┬──────────┬────────┬────────┬────────┐
 *  │Scene │System GL │ OSMesa │ NanoRT │ Embree │
 *  │brows.│  panel   │ panel  │ panel  │ panel  │
 *  ├──────┴──────────┴────────┴────────┴────────┤
 *  │[Reload][Save] [×]OSMesa [×]NanoRT [×]Embree [×]Sync All│
 *  ├─────────────────────────────────────────────────────────┤
 *  │ System GL vs OSMesa: max_diff=1 RMSE=...               │
 *  └─────────────────────────────────────────────────────────┘
 *
 * Panels resize uniformly (EqualTile distributes width equally among visible
 * panels on resize; hidden panels are moved off-screen with zero width).
 *
 * Camera interaction
 *   Left-drag  → orbit (quaternion trackball – no gimbal lock)
 *   Right-drag → dolly
 *   Scroll     → zoom
 *   "Sync All" checkbox mirrors camera changes across all visible panels.
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
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoKeyboardEvent.h>

/* ---- Unified test registry (scene factories + nanort_ok flags) ---- */
#include "testlib/test_registry.h"

/* ---- Context manager selection ---- */
/* OBOL_VIEWER_FLTK_GL: FLTKContextManager (fltk_context_manager.h) provides  */
/* the GL context via a small hidden 1×1 Fl_Gl_Window.  CoinPanel remains a   */
/* plain Fl_Box; no Fl_Gl_Window widget is used for the rendering surface.    */
/* Default (no define):  system-GL / GLX context via headless_utils.h.        */
#ifdef OBOL_VIEWER_FLTK_GL
#  include "fltk_context_manager.h"
#else
#  include "headless_utils.h"
#endif

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

/* ---- Optional Embree application-supplied renderer ---- */
#ifdef OBOL_VIEWER_EMBREE
#  include "embree_context_manager.h"
/* Single Embree renderer instance owned by the viewer application. */
static SoEmbreeContextManager s_embree_mgr;
#endif

/* ---- Optional Vulkan application-supplied renderer ---- */
#ifdef OBOL_VIEWER_VULKAN
#  include "vulkan_context_manager.h"
/* Single Vulkan renderer instance owned by the viewer application. */
static SoVulkanContextManager s_vulkan_mgr;
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
#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

/* =========================================================================
 * Scene catalogue
 *
 * All visual scenes are sourced from the unified test registry (testlib).
 * ObolTest::TestRegistry::instance() provides every scene that has
 * e.has_visual = true, including its nanort_ok flag.
 * ======================================================================= */

/* Helper: return only visual tests from the registry in registration order. */
static std::vector<const ObolTest::TestEntry*> getVisualTests()
{
    std::vector<const ObolTest::TestEntry*> out;
    for (const auto& e : ObolTest::TestRegistry::instance().allTests())
        if (e.has_visual && e.create_scene)
            out.push_back(&e);
    return out;
}

/* =========================================================================
 * FLTK → SoKeyboardEvent::Key translation
 *
 * FLTK key codes for printable ASCII characters (0x20–0x7e) happen to match
 * the SoKeyboardEvent::Key values exactly.  Special keys need mapping.
 * ======================================================================= */
static SoKeyboardEvent::Key fltkKeyToSo(int fltk_key)
{
    /* Printable ASCII: FLTK and Coin share the same values */
    if (fltk_key >= 0x20 && fltk_key <= 0x7e)
        return static_cast<SoKeyboardEvent::Key>(fltk_key);

    /* Special keys */
    switch (fltk_key) {
    case FL_BackSpace: return SoKeyboardEvent::BACKSPACE;
    case FL_Tab:       return SoKeyboardEvent::TAB;
    case FL_Enter:     return SoKeyboardEvent::RETURN;
    case FL_Escape:    return SoKeyboardEvent::ESCAPE;
    case FL_Delete:    return SoKeyboardEvent::KEY_DELETE;
    case FL_Home:      return SoKeyboardEvent::HOME;
    case FL_End:       return SoKeyboardEvent::END;
    case FL_Page_Up:   return SoKeyboardEvent::PAGE_UP;
    case FL_Page_Down: return SoKeyboardEvent::PAGE_DOWN;
    case FL_Left:      return SoKeyboardEvent::LEFT_ARROW;
    case FL_Right:     return SoKeyboardEvent::RIGHT_ARROW;
    case FL_Up:        return SoKeyboardEvent::UP_ARROW;
    case FL_Down:      return SoKeyboardEvent::DOWN_ARROW;
    case FL_Shift_L:   return SoKeyboardEvent::LEFT_SHIFT;
    case FL_Shift_R:   return SoKeyboardEvent::RIGHT_SHIFT;
    case FL_Control_L: return SoKeyboardEvent::LEFT_CONTROL;
    case FL_Control_R: return SoKeyboardEvent::RIGHT_CONTROL;
    case FL_Alt_L:     return SoKeyboardEvent::LEFT_ALT;
    case FL_Alt_R:     return SoKeyboardEvent::RIGHT_ALT;
    case FL_F + 1:     return SoKeyboardEvent::F1;
    case FL_F + 2:     return SoKeyboardEvent::F2;
    case FL_F + 3:     return SoKeyboardEvent::F3;
    case FL_F + 4:     return SoKeyboardEvent::F4;
    case FL_F + 5:     return SoKeyboardEvent::F5;
    case FL_F + 6:     return SoKeyboardEvent::F6;
    case FL_F + 7:     return SoKeyboardEvent::F7;
    case FL_F + 8:     return SoKeyboardEvent::F8;
    case FL_F + 9:     return SoKeyboardEvent::F9;
    case FL_F + 10:    return SoKeyboardEvent::F10;
    case FL_F + 11:    return SoKeyboardEvent::F11;
    case FL_F + 12:    return SoKeyboardEvent::F12;
    default:           return SoKeyboardEvent::ANY;
    }
}

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
    bool dragging       = false;
    bool dragger_active = false;   /* true when a scene dragger consumed FL_PUSH */
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
        s_renderer = new SoOffscreenRenderer(getCoinHeadlessContextManager(), vp);
    }
    return s_renderer;
}

/* =========================================================================
 * CoinPanel  –  FLTK widget that renders a Coin scene
 *
 * CoinPanel is a plain Fl_Box like all other render panels (OSMesa, NanoRT,
 * Embree).  Off-screen rendering is performed by SoOffscreenRenderer using
 * an FBO managed by Obol; the resulting RGB pixels are delivered to FLTK
 * via fltk_img->draw() in the same way as every other panel.
 *
 * When compiled with OBOL_VIEWER_FLTK_GL, the OpenGL context is provided by
 * FLTKContextManager which creates a small hidden 1×1 Fl_Gl_Window for the
 * sole purpose of owning the GLX/WGL/CGL context.  CoinPanel itself has no
 * GL dependency and interacts with the context manager only indirectly,
 * through SoOffscreenRenderer.
 *
 * Without OBOL_VIEWER_FLTK_GL (headless/GLX path) the panel uses
 * headless_utils.h for GL context management.
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
    std::function<void()>           on_rendered;
    std::function<void(SoOffscreenRenderer*)> configure_renderer_fn;

    explicit CoinPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""),
          label_text(lbl ? lbl : ""),
          gl_context_failed_(false)
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~CoinPanel() {
        delete fltk_img;
    }

    void loadScene(const char* name) {
        /* Look up scene in the unified test registry. */
        const ObolTest::TestEntry* entry =
            ObolTest::TestRegistry::instance().findTest(name);
        if (!entry || !entry->create_scene) {
            status_text = std::string("Unknown scene: ") + name;
            redraw(); return;
        }

        state.reset(new SceneState);
        state->width  = std::max(w(), 1);
        state->height = std::max(h(), 1);
        state->root   = entry->create_scene(state->width, state->height);
        state->cam    = state->root ? SceneState::findCamera(state->root) : nullptr;
        state->computeCenter();
        state->updateClipping();
        configure_renderer_fn = entry->configure_renderer;
        status_text.clear();
        refreshRender();
    }

    void clearScene() {
        state.reset();
        configure_renderer_fn = nullptr;
        delete fltk_img; fltk_img = nullptr;
        redraw();
    }

    void refreshRender() {
        if (!state || !state->root) { redraw(); return; }
        /* If GL context creation has already failed permanently, skip the
         * render attempt and just redisplay the error message.  This avoids
         * repeated failed GLX operations on every user interaction (resize,
         * camera drag, scene reload) after the initial context failure. */
        if (gl_context_failed_) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
        r->clearBackgroundGradient();
        if (configure_renderer_fn) configure_renderer_fn(r);
        if (!r->render(state->root)) {
            /* Mark GL as permanently failed so we do not keep retrying.
             * This also prevents the CoinOffscreenGLCanvas::tilesizeroof
             * static from being reduced further on every interaction. */
            gl_context_failed_ = true;
            status_text = "System GL context unavailable"; redraw(); return;
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
        if (on_rendered) on_rendered();
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
        /* Decode incoming orientation and only call setValue() when the value
         * actually changed.  Unconditionally calling setValue() with the same
         * values bumps the camera node's uniqueId, which propagates to the
         * scene root and confuses the NanoRT BVH cache into thinking only the
         * camera moved (cameraOnlyMoved false positive → stale renders). */
        float qx=orient[0],qy=orient[1],qz=orient[2],qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; }
        else ax.normalize();
        SbRotation newRot(ax, angle);

        SbVec3f curPos = state->cam->position.getValue();
        if (fabsf(curPos[0]-pos[0]) > 1e-6f || fabsf(curPos[1]-pos[1]) > 1e-6f ||
                fabsf(curPos[2]-pos[2]) > 1e-6f)
            state->cam->position.setValue(pos[0],pos[1],pos[2]);
        if (!state->cam->orientation.getValue().equals(newRot, 1e-5f))
            state->cam->orientation.setValue(newRot);
        if (dist > 0.0f && fabsf(state->cam->focalDistance.getValue() - dist) > 1e-6f)
            state->cam->focalDistance.setValue(dist);
        refreshRender();
    }

    /* ---- FLTK overrides ---- */

    void draw() override {
        /* Fl_Box path: use parent-window coordinates x()/y(). */
        fl_rectf(x(), y(), w(), h(), FL_BLACK);
        if (fltk_img) {
            fltk_img->draw(x(), y(), w(), h(), 0, 0);
        } else {
            fl_color(FL_WHITE); fl_font(FL_HELVETICA, 14);
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
        /* Compute widget-local event coordinates.
         * CoinPanel is a plain Fl_Box; x()/y() must be subtracted from
         * Fl::event_x/y() to obtain local widget coordinates. */
        const int ev_x = Fl::event_x() - x();
        const int ev_y = Fl::event_y() - y();
        switch (event) {
        case FL_PUSH:
            take_focus();
            if (state) {
                state->dragging = true;
                state->drag_btn = Fl::event_button();
                state->last_x   = ev_x;
                state->last_y   = ev_y;
                SoMouseButtonEvent ev;
                ev.setButton(state->drag_btn==1 ? SoMouseButtonEvent::BUTTON1 :
                             state->drag_btn==2 ? SoMouseButtonEvent::BUTTON2 :
                                                  SoMouseButtonEvent::BUTTON3);
                ev.setState(SoButtonEvent::DOWN);
                ev.setPosition(SbVec2s((short)ev_x, (short)(h_-ev_y)));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                state->dragger_active = ha.isHandled();
            }
            return 1;
        case FL_RELEASE:
            if (state) {
                state->dragging = false;
                state->dragger_active = false;
                SoMouseButtonEvent ev;
                ev.setButton(Fl::event_button()==1 ? SoMouseButtonEvent::BUTTON1 :
                             Fl::event_button()==2 ? SoMouseButtonEvent::BUTTON2 :
                                                     SoMouseButtonEvent::BUTTON3);
                ev.setState(SoButtonEvent::UP);
                ev.setPosition(SbVec2s((short)ev_x, (short)(h_-ev_y)));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                notifyCameraChanged();
                refreshRender();
            }
            return 1;
        case FL_DRAG:
            if (state && state->dragging && state->cam) {
                int dx = ev_x - state->last_x, dy = ev_y - state->last_y;
                state->last_x = ev_x; state->last_y = ev_y;
                SoLocation2Event ev;
                ev.setPosition(SbVec2s((short)ev_x,(short)(h_-ev_y)));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                if (!state->dragger_active) {
                    if (state->drag_btn == 1) {
                        /* orbit – camera-local yaw and pitch (BRL-CAD style):
                         * symmetric axes, no world-up reference, no gimbal lock */
                        state->cam->orbitCamera(state->scene_center,
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
                }
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
                ev.setKey(fltkKeyToSo(Fl::event_key()));
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
                ev.setKey(fltkKeyToSo(Fl::event_key()));
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
    /* Set to true after the first GL context creation failure.  Prevents
     * repeated failed GLX operations (and their side effects on the static
     * CoinOffscreenGLCanvas::tilesizeroof) on every subsequent user
     * interaction such as resize, camera drag, or scene reload. */
    bool gl_context_failed_;
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
    std::function<void()>             on_rendered;

    explicit NanoRTPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~NanoRTPanel() {
        Fl::remove_timeout(doRefine, this);
        delete fltk_img;
    }

    void setScene(SoSeparator* r, SoPerspectiveCamera* c, bool nanort_supported = true) {
        root = r; cam = c; nanort_ok_ = nanort_supported; status_text.clear();
        /* Scene change invalidates coarse-render calibration and the BVH
         * geometry cache.  The cache reset is essential: Coin frees the old
         * scene nodes and the allocator may immediately recycle those
         * addresses for the new scene, so the new root/camera pointers can
         * appear identical to the old ones even though the geometry has
         * completely changed.  Without an explicit reset the stale BVH would
         * be reused for the new scene (e.g. a cube rendered as a sphere). */
        coarseRW_ = 0; coarseRH_ = 0; calFocalDist_ = 0.0f;
        s_nanort_mgr.resetCache();
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
        if (!coarse_) {
            /* Full-resolution render.  The coarse calibration is intentionally
             * preserved here so that the next drag re-uses the cached coarse
             * size without repeating the step-in hunt. */
            if (!doRender_(pw, ph, pw, ph)) { redraw(); return; }
        } else {
            /* Invalidate the cached coarse resolution when the camera focal
             * distance has changed by more than 2× since calibration was
             * measured.  A large zoom change alters the effective ray density
             * enough to make the old calibration unreliable. */
            if (coarseRW_ != 0 && calFocalDist_ > 0.0f && cam) {
                float fd = cam->focalDistance.getValue();
                if (fd < calFocalDist_ * 0.5f || fd > calFocalDist_ * 2.0f) {
                    coarseRW_ = 0; coarseRH_ = 0; stepInComplete_ = false;
                    calFocalDist_ = 0.0f;
                }
            }
            if (coarseRW_ == 0 || calPanelW_ != pw || calPanelH_ != ph ||
                       !stepInComplete_) {
                /* Step-in calibration needed or still in progress: run one round.
                 * timedStepIn_ returns true when the optimal level has been found,
                 * false when it ran out of per-call search budget and should be
                 * resumed next time. */
                stepInComplete_ = timedStepIn_(pw, ph);
            } else {
                /* Subsequent coarse renders: reuse calibrated size. */
                if (!doRender_(pw, ph, coarseRW_, coarseRH_)) { redraw(); return; }
            }
        }
        redraw();
        if (on_rendered) on_rendered();
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
        /* Decode incoming orientation once so we can equality-check before
         * calling setValue().  Skipping setValue() when the value has not
         * changed prevents spurious SoNode::uniqueId bumps that would make
         * the NanoRT BVH cache incorrectly infer a camera-only change
         * (cameraOnlyMoved = true) when the geometry has also changed (e.g.
         * a manipulator was just dragged).  Without this guard, calling
         * setCamera() with the same values during a manip drag caused the
         * NanoRT panel to skip the BVH rebuild and show a stale image. */
        float qx=orient[0], qy=orient[1], qz=orient[2], qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; } else ax.normalize();
        SbRotation newRot(ax, angle);

        SbVec3f curPos = cam->position.getValue();
        if (fabsf(curPos[0]-pos[0]) > 1e-6f || fabsf(curPos[1]-pos[1]) > 1e-6f ||
                fabsf(curPos[2]-pos[2]) > 1e-6f)
            cam->position.setValue(pos[0], pos[1], pos[2]);
        if (!cam->orientation.getValue().equals(newRot, 1e-5f))
            cam->orientation.setValue(newRot);
        if (dist > 0.0f && fabsf(cam->focalDistance.getValue() - dist) > 1e-6f)
            cam->focalDistance.setValue(dist);
        /* Camera is being moved (driven by sync from another panel or own drag):
         * switch to coarse mode for this render and schedule a full-resolution
         * refinement pass once the view has been stable for kRefineDelaySec. */
        coarse_ = true;
        Fl::remove_timeout(doRefine, this);
        Fl::add_timeout(kRefineDelaySec, doRefine, this);
        refreshRender();
    }

    /* Called by the cross-panel sync callback to re-render this panel after
     * another panel's camera or scene changed.  All panels share the same
     * camera pointer, so no camera field writes are needed here – doing them
     * would trigger spurious Coin3D notifications that bump the camera's
     * nodeId, causing the NanoRT BVH cache's "camera-only moved" heuristic to
     * incorrectly skip a geometry rebuild when the dragger also moved scene
     * objects.  Setting coarse mode and calling refreshRender() is sufficient
     * to show the already-updated shared scene. */
    void refreshFromSync() {
        if (!root || !nanort_ok_) return;
        coarse_ = true;
        Fl::remove_timeout(doRefine, this);
        Fl::add_timeout(kRefineDelaySec, doRefine, this);
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
        int h_ = h();
        switch (event) {
        case FL_PUSH: {
            take_focus();
            dragging_ = true;
            drag_btn_ = Fl::event_button();
            last_x_   = Fl::event_x() - x();
            last_y_   = Fl::event_y() - y();
            SoMouseButtonEvent ev;
            ev.setButton(drag_btn_==1 ? SoMouseButtonEvent::BUTTON1 :
                         drag_btn_==2 ? SoMouseButtonEvent::BUTTON2 :
                                        SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::DOWN);
            ev.setPosition(SbVec2s((short)last_x_, (short)(h_-last_y_)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            dragger_active_ = ha.isHandled();
            /* Entering a manip drag: invalidate coarse calibration so
             * timedStepIn_'s dragger warm-up can re-calibrate with
             * BVH rebuild overhead included in the timing. */
            if (dragger_active_) stepInComplete_ = false;
            return 1;
        }
        case FL_RELEASE: {
            bool was_dragger_active = dragger_active_;
            dragging_ = false;
            dragger_active_ = false;
            int rx = Fl::event_x()-x(), ry = Fl::event_y()-y();
            SoMouseButtonEvent ev;
            ev.setButton(Fl::event_button()==1 ? SoMouseButtonEvent::BUTTON1 :
                         Fl::event_button()==2 ? SoMouseButtonEvent::BUTTON2 :
                                                 SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::UP);
            ev.setPosition(SbVec2s((short)rx, (short)(h_-ry)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            /* View is now stable: cancel any pending coarse timer and render
             * at full resolution immediately. */
            Fl::remove_timeout(doRefine, this);
            coarse_ = false;
            /* After a dragger drag the coarse calibration was measured while
             * BVH rebuilds were happening on every frame.  That overhead made
             * the calibration too conservative (too low a resolution).  Reset
             * it here so the next drag or orbit re-calibrates from scratch,
             * measuring only pure raytrace cost and converging to the correct
             * resolution for the scene. */
            if (was_dragger_active) {
                coarseRW_ = 0; coarseRH_ = 0;
                stepInComplete_ = false; calFocalDist_ = 0.0f;
            }
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_DRAG: {
            if (!dragging_) return 1;
            int ex = Fl::event_x()-x(), ey = Fl::event_y()-y();
            int dx = ex - last_x_, dy = ey - last_y_;
            last_x_ = ex; last_y_ = ey;
            SoLocation2Event ev;
            ev.setPosition(SbVec2s((short)ex, (short)(h_-ey)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            if (!dragger_active_) {
                if (drag_btn_ == 1) {
                    /* orbit – camera-local yaw and pitch (BRL-CAD style):
                     * symmetric axes, no world-up reference, no gimbal lock */
                    cam->orbitCamera(scene_center_,
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
            }
            /* Render coarse during drag for speed; reset timer so refine
             * fires kRefineDelaySec after the last drag event. */
            coarse_ = true;
            Fl::remove_timeout(doRefine, this);
            Fl::add_timeout(kRefineDelaySec, doRefine, this);
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
    bool nanort_ok_      = true;
    bool dragging_       = false;
    bool dragger_active_ = false;  /* true when a scene dragger consumed FL_PUSH */
    bool coarse_         = false;  /* true → render at reduced resolution for speed */
    int  drag_btn_  = 0;
    int  last_x_    = 0;
    int  last_y_    = 0;

    /* Calibrated coarse render size (0 = uncalibrated / needs step-in). */
    int  coarseRW_       = 0;
    int  coarseRH_       = 0;
    /* Panel dimensions at which coarseRW_/RH_ were measured. */
    int  calPanelW_      = 0;
    int  calPanelH_      = 0;
    /* Camera focal distance at which coarseRW_/RH_ were measured
     * (0 = uncalibrated).  A large change in focal distance alters ray
     * density enough to warrant recalibration. */
    float calFocalDist_  = 0.0f;
    /* True once timedStepIn_ has converged to the optimal coarse level.
     * False while incremental refinement is still in progress. */
    bool stepInComplete_ = true;

    /* Maximum render time (ms) per coarse frame; targets ~10 fps for
     * interactive feel.  Step-in stops when a level reaches 75% of this. */
    static constexpr double kCoarseBudgetMs = 20.0;
    /* Fraction of kCoarseBudgetMs at which step-in stops doubling resolution.
     * 0.75 gives headroom so the chosen level reliably stays within budget. */
    static constexpr double kBudgetThreshold = 0.75;

    /* Delay (seconds) after the last camera change before doing a full-res
     * refinement render.  250 ms feels snappy without firing during fast drags. */
    static constexpr double kRefineDelaySec = 0.25;

    /* FLTK timer callback: view is stable — re-render at full resolution. */
    static void doRefine(void* data) {
        NanoRTPanel* p = static_cast<NanoRTPanel*>(data);
        if (!p->visible()) return;   /* panel hidden – skip background refine */
        p->coarse_ = false;
        p->refreshRender();
    }

    /* Render the scene at (rw×rh), upscale nearest-neighbour to (pw×ph),
     * and update fltk_img.  Returns false on render failure. */
    bool doRender_(int pw, int ph, int rw, int rh) {
        const float bg[3] = { 0.15f, 0.15f, 0.2f };
        const uint8_t bg_r = static_cast<uint8_t>(bg[0] * 255.0f + 0.5f);
        const uint8_t bg_g = static_cast<uint8_t>(bg[1] * 255.0f + 0.5f);
        const uint8_t bg_b = static_cast<uint8_t>(bg[2] * 255.0f + 0.5f);
        pixel_buf.resize((size_t)rw * rh * 4);
        for (size_t i = 0; i < (size_t)rw * rh; ++i) {
            pixel_buf[i*4+0] = bg_r;
            pixel_buf[i*4+1] = bg_g;
            pixel_buf[i*4+2] = bg_b;
            pixel_buf[i*4+3] = 255;
        }
        /* Inform the NanoRT renderer of the full display dimensions so that
         * line/point/cylinder proxy geometry is sized for the real panel
         * resolution, not the reduced coarse render grid. */
        s_nanort_mgr.setDisplayViewport((unsigned int)pw, (unsigned int)ph);
        SbBool ok = s_nanort_mgr.renderScene(root,
                                             (unsigned int)rw,
                                             (unsigned int)rh,
                                             pixel_buf.data(),
                                             4u, bg);
        if (!ok) { status_text = "NanoRT render failed"; return false; }

        /* Convert bottom-up RGBA → top-down full-res RGB for FLTK.
         * Nearest-neighbour upscale handles the coarse case; when rw==pw
         * and rh==ph it is a straight flip-and-pack. */
        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            int src_row = (rh - 1) - (row * rh / ph);
            const uint8_t* s_base = pixel_buf.data() + (size_t)src_row * rw * 4;
            uint8_t* d = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                const uint8_t* s = s_base + (col * rw / pw) * 4;
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d+=3;
            }
        }
        delete fltk_img;
        fltk_img = new Fl_RGB_Image(display_buf.data(), pw, ph, 3);
        return true;
    }

    /* Timed step-in calibration: find the coarsest render resolution that
     * approaches kCoarseBudgetMs.  On the first call (coarseRW_==0 or panel
     * size changed) search starts at 2 px wide; on subsequent calls it
     * resumes from the next level beyond the stored best answer so the
     * initial coarse frame appears immediately and quality improves each
     * time refreshRender is called.
     *
     * The per-call search budget mirrors kCoarseBudgetMs so that a single
     * call never blocks for longer than one frame.  Returns true when the
     * optimal level has been found, false if the per-call budget was
     * exhausted before convergence (caller should invoke again next frame).
     *
     * Both the fresh and resume paths perform an uncounted warm-up render
     * when a BVH rebuild is expected (fresh start, or resume with an active
     * scene dragger).  The warm-up absorbs the rebuild cost so the timed
     * search loop measures only pure raytracing time, preventing BVH
     * construction overhead from contaminating the calibration.
     *
     * Sets coarseRW_, coarseRH_, calPanelW_, calPanelH_ on every return. */
    bool timedStepIn_(int pw, int ph) {
        using clock = std::chrono::steady_clock;
        auto tStart = clock::now();

        /* Determine starting point:
         *   fresh  – panel size changed or no prior calibration: begin at 2 px.
         *   resume – continue from the level above the previous best answer. */
        bool fresh = (coarseRW_ == 0 || calPanelW_ != pw || calPanelH_ != ph);
        int crw    = fresh ? 2 : coarseRW_ * 2;
        int bestRW = fresh ? 2 : coarseRW_;
        int bestRH = fresh ? std::max(1, (2 * ph + pw / 2) / pw) : coarseRH_;

        /* On a fresh start the first renderScene() call may trigger a BVH
         * cache rebuild (on initial scene load or after a geometry change)
         * whose cost dominates the elapsed time and makes even the coarsest
         * resolution appear to exceed the per-frame budget.
         * Perform one uncounted warm-up render at the smallest level to
         * absorb any rebuild latency before the timed search loop begins.
         * The warm-up result is displayed immediately (doRender_ updates
         * fltk_img), giving the user a first coarse frame right away; the
         * timed loop then measures pure raytracing cost and converges to the
         * correct level.  tStart is reset after the warm-up so the search
         * budget is not consumed by cache-build latency.  crw is advanced so
         * the timed loop starts from the next level rather than repeating the
         * warm-up resolution. */
        if (fresh) {
            const int rw0 = crw < pw ? crw : pw;
            const int rh0 = rw0 < pw ? std::max(1, (rw0 * ph + pw / 2) / pw) : ph;
            if (doRender_(pw, ph, rw0, rh0)) {
                bestRW = rw0;
                bestRH = rh0;
            }
            tStart = clock::now();  /* reset search budget after cache warm-up */
            crw *= 2;               /* warm-up covered this level; start timed loop one step up */
        } else if (dragger_active_) {
            /* Resume warm-up for dynamic geometry: when a scene dragger is
             * actively changing geometry each frame, the BVH is rebuilt on
             * each renderScene() call.  Without a warm-up, that rebuild cost
             * leaks into the timed search loop and makes the calibration
             * converge to a resolution that is too coarse – the algorithm
             * thinks the scene is slow to raytrace, but the slowness is
             * actually BVH construction overhead.  Performing an uncounted
             * warm-up at the current best resolution absorbs the rebuild cost
             * before the timed loop begins, so the loop measures only pure
             * raytracing time and calibrates coarseRW_ correctly.
             * tStart is reset after the warm-up so the search budget only
             * covers the raytrace measurements. */
            const int rw0 = coarseRW_ < pw ? coarseRW_ : pw;
            const int rh0 = rw0 < pw ? std::max(1, (rw0 * ph + pw / 2) / pw) : ph;
            if (doRender_(pw, ph, rw0, rh0)) {
                bestRW = rw0;
                bestRH = rh0;
            }
            tStart = clock::now();  /* reset search budget: BVH rebuild cost excluded */
        }

        bool optimal = false;
        while (true) {
            const int rw = crw < pw ? crw : pw;
            /* Derive rh from rw to preserve the panel aspect ratio, so the
             * raytracing view frustum matches the display dimensions and the
             * upscaled coarse image is not distorted. */
            const int rh = rw < pw ? std::max(1, (rw * ph + pw / 2) / pw) : ph;
            auto t0 = clock::now();
            bool ok = doRender_(pw, ph, rw, rh);
            double ms = std::chrono::duration<double, std::milli>(
                            clock::now() - t0).count();
            if (!ok) break;
            /* Only record this resolution as "best" when it fits within the
             * per-frame budget.  This prevents the calibration from storing a
             * slow full-resolution level as the coarse target: when rw reaches
             * pw, the loop exits regardless (no higher level exists), but if
             * that final render was too slow we want to keep the last fast
             * level, not the newly exceeded one. */
            const bool withinBudget =
                ms < kCoarseBudgetMs * kBudgetThreshold;
            if (withinBudget) {
                bestRW = rw;
                bestRH = rh;
            }
            /* Stop when this level exceeds the per-frame budget or the full
             * panel size has been reached: optimal level found. */
            if (!withinBudget || rw >= pw || rh >= ph) {
                optimal = true;
                break;
            }
            /* Stop if cumulative search time this call is approaching the
             * budget: return false so caller retries next frame from bestRW. */
            double searchMs = std::chrono::duration<double, std::milli>(
                                  clock::now() - tStart).count();
            if (searchMs >= kCoarseBudgetMs * kBudgetThreshold)
                break;
            crw *= 2;
        }
        coarseRW_     = bestRW;
        coarseRH_     = bestRH;
        calPanelW_    = pw;
        calPanelH_    = ph;
        calFocalDist_ = cam ? cam->focalDistance.getValue() : 0.0f;
        return optimal;
    }

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
 * EmbreePanel  –  application-supplied Embree CPU raytracing panel (optional)
 *
 * Renders the same scene graph as CoinPanel but via SoEmbreeContextManager
 * called directly as a plain method call — completely independent of the
 * GL context manager singleton that was registered with SoDB::init().
 * This is the "application-supplied renderer" pattern: the viewer owns the
 * Embree renderer object and drives it without any Coin involvement beyond
 * the scene graph traversal inside renderScene() itself.
 *
 * Only one of OBOL_VIEWER_EMBREE or OBOL_VIEWER_NANORT can be active at a
 * time (CMake enforces mutual exclusion).  EmbreePanel is a structural mirror
 * of NanoRTPanel; the only differences are the backing context-manager type
 * and the diagnostic strings.
 * ======================================================================= */
#ifdef OBOL_VIEWER_EMBREE
class EmbreePanel : public Fl_Box {
public:
    /* The scene root to render – shared with the CoinPanel (same graph). */
    SoSeparator*         root       = nullptr;
    SoPerspectiveCamera* cam        = nullptr;
    std::string          label_text;
    std::string          status_text;

    std::vector<uint8_t> pixel_buf;   /* RGBA, bottom-up from renderScene() */
    std::vector<uint8_t> display_buf; /* RGB, top-down for FLTK */
    Fl_RGB_Image*        fltk_img  = nullptr;

    std::function<void(EmbreePanel*)> on_camera_changed;
    std::function<void()>             on_rendered;

    explicit EmbreePanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~EmbreePanel() {
        Fl::remove_timeout(doRefine, this);
        delete fltk_img;
    }

    void setScene(SoSeparator* r, SoPerspectiveCamera* c, bool embree_supported = true) {
        root = r; cam = c; embree_ok_ = embree_supported; status_text.clear();
        /* Scene change invalidates coarse-render calibration and the Embree
         * scene cache.  The cache reset is essential: Coin frees the old
         * scene nodes and the allocator may immediately recycle those
         * addresses for the new scene, so the new root/camera pointers can
         * appear identical to the old ones even though the geometry has
         * completely changed.  Without an explicit reset the stale BVH would
         * be reused for the new scene. */
        coarseRW_ = 0; coarseRH_ = 0; calFocalDist_ = 0.0f;
        s_embree_mgr.resetCache();
        if (!embree_ok_) {
            status_text = "Not supported (Embree)";
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
        if (!root || !embree_ok_) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);
        if (!coarse_) {
            if (!doRender_(pw, ph, pw, ph)) { redraw(); return; }
        } else {
            if (coarseRW_ != 0 && calFocalDist_ > 0.0f && cam) {
                float fd = cam->focalDistance.getValue();
                if (fd < calFocalDist_ * 0.5f || fd > calFocalDist_ * 2.0f) {
                    coarseRW_ = 0; coarseRH_ = 0; stepInComplete_ = false;
                    calFocalDist_ = 0.0f;
                }
            }
            if (coarseRW_ == 0 || calPanelW_ != pw || calPanelH_ != ph ||
                       !stepInComplete_) {
                stepInComplete_ = timedStepIn_(pw, ph);
            } else {
                if (!doRender_(pw, ph, coarseRW_, coarseRH_)) { redraw(); return; }
            }
        }
        redraw();
        if (on_rendered) on_rendered();
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
        float qx=orient[0], qy=orient[1], qz=orient[2], qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; } else ax.normalize();
        SbRotation newRot(ax, angle);

        SbVec3f curPos = cam->position.getValue();
        if (fabsf(curPos[0]-pos[0]) > 1e-6f || fabsf(curPos[1]-pos[1]) > 1e-6f ||
                fabsf(curPos[2]-pos[2]) > 1e-6f)
            cam->position.setValue(pos[0], pos[1], pos[2]);
        if (!cam->orientation.getValue().equals(newRot, 1e-5f))
            cam->orientation.setValue(newRot);
        if (dist > 0.0f && fabsf(cam->focalDistance.getValue() - dist) > 1e-6f)
            cam->focalDistance.setValue(dist);
        coarse_ = true;
        Fl::remove_timeout(doRefine, this);
        Fl::add_timeout(kRefineDelaySec, doRefine, this);
        refreshRender();
    }

    void refreshFromSync() {
        if (!root || !embree_ok_) return;
        coarse_ = true;
        Fl::remove_timeout(doRefine, this);
        Fl::add_timeout(kRefineDelaySec, doRefine, this);
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
        if (!root || !cam || !embree_ok_) return Fl_Box::handle(event);
        int h_ = h();
        switch (event) {
        case FL_PUSH: {
            take_focus();
            dragging_ = true;
            drag_btn_ = Fl::event_button();
            last_x_   = Fl::event_x() - x();
            last_y_   = Fl::event_y() - y();
            SoMouseButtonEvent ev;
            ev.setButton(drag_btn_==1 ? SoMouseButtonEvent::BUTTON1 :
                         drag_btn_==2 ? SoMouseButtonEvent::BUTTON2 :
                                        SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::DOWN);
            ev.setPosition(SbVec2s((short)last_x_, (short)(h_-last_y_)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            dragger_active_ = ha.isHandled();
            /* Entering a manip drag: invalidate coarse calibration so
             * timedStepIn_'s dragger warm-up can re-calibrate with
             * BVH rebuild overhead included in the timing. */
            if (dragger_active_) stepInComplete_ = false;
            return 1;
        }
        case FL_RELEASE: {
            bool was_dragger_active = dragger_active_;
            dragging_ = false;
            dragger_active_ = false;
            int rx = Fl::event_x()-x(), ry = Fl::event_y()-y();
            SoMouseButtonEvent ev;
            ev.setButton(Fl::event_button()==1 ? SoMouseButtonEvent::BUTTON1 :
                         Fl::event_button()==2 ? SoMouseButtonEvent::BUTTON2 :
                                                 SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::UP);
            ev.setPosition(SbVec2s((short)rx, (short)(h_-ry)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            Fl::remove_timeout(doRefine, this);
            coarse_ = false;
            if (was_dragger_active) {
                coarseRW_ = 0; coarseRH_ = 0;
                stepInComplete_ = false; calFocalDist_ = 0.0f;
            }
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_DRAG: {
            if (!dragging_) return 1;
            int ex = Fl::event_x()-x(), ey = Fl::event_y()-y();
            int dx = ex - last_x_, dy = ey - last_y_;
            last_x_ = ex; last_y_ = ey;
            SoLocation2Event ev;
            ev.setPosition(SbVec2s((short)ex, (short)(h_-ey)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            if (!dragger_active_) {
                if (drag_btn_ == 1) {
                    cam->orbitCamera(scene_center_,
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
            }
            coarse_ = true;
            Fl::remove_timeout(doRefine, this);
            Fl::add_timeout(kRefineDelaySec, doRefine, this);
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
    bool embree_ok_      = true;
    bool dragging_       = false;
    bool dragger_active_ = false;
    bool coarse_         = false;
    int  drag_btn_  = 0;
    int  last_x_    = 0;
    int  last_y_    = 0;

    int  coarseRW_       = 0;
    int  coarseRH_       = 0;
    int  calPanelW_      = 0;
    int  calPanelH_      = 0;
    float calFocalDist_  = 0.0f;
    bool stepInComplete_ = true;

    static constexpr double kCoarseBudgetMs  = 40.0;
    static constexpr double kBudgetThreshold = 0.75;
    static constexpr double kRefineDelaySec  = 0.25;

    static void doRefine(void* data) {
        EmbreePanel* p = static_cast<EmbreePanel*>(data);
        if (!p->visible()) return;   /* panel hidden – skip background refine */
        p->coarse_ = false;
        p->refreshRender();
    }

    bool doRender_(int pw, int ph, int rw, int rh) {
        const float bg[3] = { 0.15f, 0.15f, 0.2f };
        const uint8_t bg_r = static_cast<uint8_t>(bg[0] * 255.0f + 0.5f);
        const uint8_t bg_g = static_cast<uint8_t>(bg[1] * 255.0f + 0.5f);
        const uint8_t bg_b = static_cast<uint8_t>(bg[2] * 255.0f + 0.5f);
        pixel_buf.resize((size_t)rw * rh * 4);
        for (size_t i = 0; i < (size_t)rw * rh; ++i) {
            pixel_buf[i*4+0] = bg_r;
            pixel_buf[i*4+1] = bg_g;
            pixel_buf[i*4+2] = bg_b;
            pixel_buf[i*4+3] = 255;
        }
        s_embree_mgr.setDisplayViewport((unsigned int)pw, (unsigned int)ph);
        SbBool ok = s_embree_mgr.renderScene(root,
                                             (unsigned int)rw,
                                             (unsigned int)rh,
                                             pixel_buf.data(),
                                             4u, bg);
        if (!ok) { status_text = "Embree render failed"; return false; }

        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            int src_row = (rh - 1) - (row * rh / ph);
            const uint8_t* s_base = pixel_buf.data() + (size_t)src_row * rw * 4;
            uint8_t* d = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                const uint8_t* s = s_base + (col * rw / pw) * 4;
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d+=3;
            }
        }
        delete fltk_img;
        fltk_img = new Fl_RGB_Image(display_buf.data(), pw, ph, 3);
        return true;
    }

    bool timedStepIn_(int pw, int ph) {
        using clock = std::chrono::steady_clock;
        auto tStart = clock::now();

        bool fresh = (coarseRW_ == 0 || calPanelW_ != pw || calPanelH_ != ph);
        int crw    = fresh ? 2 : coarseRW_ * 2;
        int bestRW = fresh ? 2 : coarseRW_;
        int bestRH = fresh ? std::max(1, (2 * ph + pw / 2) / pw) : coarseRH_;

        if (fresh) {
            const int rw0 = crw < pw ? crw : pw;
            const int rh0 = rw0 < pw ? std::max(1, (rw0 * ph + pw / 2) / pw) : ph;
            if (doRender_(pw, ph, rw0, rh0)) {
                bestRW = rw0;
                bestRH = rh0;
            }
            tStart = clock::now();
            crw *= 2;
        } else if (dragger_active_) {
            const int rw0 = coarseRW_ < pw ? coarseRW_ : pw;
            const int rh0 = rw0 < pw ? std::max(1, (rw0 * ph + pw / 2) / pw) : ph;
            if (doRender_(pw, ph, rw0, rh0)) {
                bestRW = rw0;
                bestRH = rh0;
            }
            tStart = clock::now();
        }

        bool optimal = false;
        while (true) {
            const int rw = crw < pw ? crw : pw;
            const int rh = rw < pw ? std::max(1, (rw * ph + pw / 2) / pw) : ph;
            auto t0 = clock::now();
            bool ok = doRender_(pw, ph, rw, rh);
            double ms = std::chrono::duration<double, std::milli>(
                            clock::now() - t0).count();
            if (!ok) break;
            const bool withinBudget =
                ms < kCoarseBudgetMs * kBudgetThreshold;
            if (withinBudget) {
                bestRW = rw;
                bestRH = rh;
            }
            if (!withinBudget || rw >= pw || rh >= ph) {
                optimal = true;
                break;
            }
            double searchMs = std::chrono::duration<double, std::milli>(
                                  clock::now() - tStart).count();
            if (searchMs >= kCoarseBudgetMs * kBudgetThreshold)
                break;
            crw *= 2;
        }
        coarseRW_     = bestRW;
        coarseRH_     = bestRH;
        calPanelW_    = pw;
        calPanelH_    = ph;
        calFocalDist_ = cam ? cam->focalDistance.getValue() : 0.0f;
        return optimal;
    }

    void updateClipping_() {
        if (!cam) return;
        float d = cam->focalDistance.getValue();
        if (d < 1e-4f) d = 1e-4f;
        cam->nearDistance.setValue(d * 0.001f);
        cam->farDistance.setValue(d * 10000.0f);
    }

    void notifyCameraChanged() { if (on_camera_changed) on_camera_changed(this); }
};
#endif /* OBOL_VIEWER_EMBREE */


/* =========================================================================
 * VulkanPanel  –  application-supplied Vulkan rasterisation panel (optional)
 *
 * Renders the same scene graph as CoinPanel but via SoVulkanContextManager
 * called directly as a plain method call.  Lighting is pre-computed on the
 * CPU (Phong model, same formulas as NanoRT/Embree but without shadows); the
 * GPU performs vertex transformation, rasterisation, and depth testing.
 *
 * Unlike the CPU ray-tracers, Vulkan rendering is hardware-accelerated and
 * fast enough to render at full resolution on every frame, so the
 * coarse/refine calibration used by NanoRT and Embree is omitted here.
 * ======================================================================= */
#ifdef OBOL_VIEWER_VULKAN
class VulkanPanel : public Fl_Box {
public:
    SoSeparator*         root       = nullptr;
    SoPerspectiveCamera* cam        = nullptr;
    std::string          label_text;
    std::string          status_text;

    std::vector<uint8_t> pixel_buf;   /* RGBA from renderScene() */
    std::vector<uint8_t> display_buf; /* RGB, top-down for FLTK */
    Fl_RGB_Image*        fltk_img  = nullptr;

    std::function<void(VulkanPanel*)> on_camera_changed;
    std::function<void()>             on_rendered;

    explicit VulkanPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~VulkanPanel() { delete fltk_img; }

    void setScene(SoSeparator* r, SoPerspectiveCamera* c,
                  bool vulkan_supported = true)
    {
        root = r; cam = c; vulkan_ok_ = vulkan_supported;
        status_text.clear();
        s_vulkan_mgr.resetCache();
        if (!vulkan_ok_) {
            status_text = "Not supported (Vulkan)";
            delete fltk_img; fltk_img = nullptr;
            redraw(); return;
        }
        scene_center_ = SbVec3f(0,0,0);
        if (root) {
            SoGetBoundingBoxAction bba(
                SbViewportRegion(std::max(w(),1), std::max(h(),1)));
            bba.apply(root);
            SbBox3f bbox = bba.getBoundingBox();
            if (!bbox.isEmpty()) scene_center_ = bbox.getCenter();
        }
        refreshRender();
    }

    void refreshRender() {
        if (!root || !vulkan_ok_) { redraw(); return; }
        int pw = std::max(w(), 1);
        int ph = std::max(h(), 1);
        if (!doRender_(pw, ph)) { redraw(); return; }
        redraw();
        if (on_rendered) on_rendered();
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
        float qx=orient[0], qy=orient[1], qz=orient[2], qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; } else ax.normalize();
        SbRotation newRot(ax, angle);
        SbVec3f curPos = cam->position.getValue();
        if (fabsf(curPos[0]-pos[0]) > 1e-6f || fabsf(curPos[1]-pos[1]) > 1e-6f ||
                fabsf(curPos[2]-pos[2]) > 1e-6f)
            cam->position.setValue(pos[0], pos[1], pos[2]);
        if (!cam->orientation.getValue().equals(newRot, 1e-5f))
            cam->orientation.setValue(newRot);
        if (dist > 0.0f && fabsf(cam->focalDistance.getValue() - dist) > 1e-6f)
            cam->focalDistance.setValue(dist);
        refreshRender();
    }

    void refreshFromSync() {
        if (!root || !vulkan_ok_) return;
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
        if (!root || !cam || !vulkan_ok_) return Fl_Box::handle(event);
        int h_ = h();
        switch (event) {
        case FL_PUSH: {
            take_focus();
            dragging_ = true;
            drag_btn_ = Fl::event_button();
            last_x_   = Fl::event_x() - x();
            last_y_   = Fl::event_y() - y();
            SoMouseButtonEvent ev;
            ev.setButton(drag_btn_==1 ? SoMouseButtonEvent::BUTTON1 :
                         drag_btn_==2 ? SoMouseButtonEvent::BUTTON2 :
                                        SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::DOWN);
            ev.setPosition(SbVec2s((short)last_x_, (short)(h_-last_y_)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            return 1;
        }
        case FL_RELEASE: {
            dragging_ = false;
            int rx = Fl::event_x()-x(), ry = Fl::event_y()-y();
            SoMouseButtonEvent ev;
            ev.setButton(Fl::event_button()==1 ? SoMouseButtonEvent::BUTTON1 :
                         Fl::event_button()==2 ? SoMouseButtonEvent::BUTTON2 :
                                                 SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::UP);
            ev.setPosition(SbVec2s((short)rx, (short)(h_-ry)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_DRAG: {
            if (!dragging_) return 1;
            int ex = Fl::event_x()-x(), ey = Fl::event_y()-y();
            int dx = ex - last_x_, dy = ey - last_y_;
            last_x_ = ex; last_y_ = ey;
            SoLocation2Event ev;
            ev.setPosition(SbVec2s((short)ex, (short)(h_-ey)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            if (drag_btn_ == 1) {
                cam->orbitCamera(scene_center_,
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
            float dist = cam->focalDistance.getValue() *
                         (1.0f + (float)Fl::event_dy()*0.1f);
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
    bool vulkan_ok_  = true;
    bool dragging_   = false;
    int  drag_btn_   = 0;
    int  last_x_     = 0;
    int  last_y_     = 0;

    bool doRender_(int pw, int ph) {
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
        s_vulkan_mgr.setDisplayViewport((unsigned int)pw, (unsigned int)ph);
        SbBool ok = s_vulkan_mgr.renderScene(root,
                                             (unsigned int)pw,
                                             (unsigned int)ph,
                                             pixel_buf.data(),
                                             4u, bg);
        if (!ok) { status_text = "Vulkan render failed (no device?)"; return false; }
        status_text.clear();

        /* SoVulkanContextManager outputs pixels in bottom-to-top row order
         * because the OpenGL view-projection matrix and Vulkan's y-down NDC
         * convention cancel out, giving the same bottom-up layout that
         * SoOffscreenRenderer uses (see vulkan_context_manager.h notes).
         * Convert to top-down RGB for FLTK display here. */
        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = pixel_buf.data() + (size_t)(ph-1-row) * pw * 4;
            uint8_t* d = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; d+=3; s+=4;
            }
        }
        delete fltk_img;
        fltk_img = new Fl_RGB_Image(display_buf.data(), pw, ph, 3);
        return true;
    }

    void updateClipping_() {
        if (!cam) return;
        float d = cam->focalDistance.getValue();
        if (d < 1e-4f) d = 1e-4f;
        cam->nearDistance.setValue(d * 0.001f);
        cam->farDistance.setValue(d * 10000.0f);
    }

    void notifyCameraChanged() { if (on_camera_changed) on_camera_changed(this); }
};
#endif /* OBOL_VIEWER_VULKAN */


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
    std::function<void()>             on_rendered;
    std::function<void(SoOffscreenRenderer*)> configure_renderer_fn;

    explicit OSMesaPanel(int X, int Y, int W, int H, const char* lbl = "")
        : Fl_Box(X, Y, W, H, ""), label_text(lbl ? lbl : "")
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);

        /* Obtain an OSMesa-backed context manager from Obol.  No OSMesa
         * headers are needed here – the library handles the details. */
        osmesa_mgr_.reset(SoDB::createOSMesaContextManager());

        SbViewportRegion vp(W, H);
        if (osmesa_mgr_) {
            /* Create renderer pinned to the OSMesa backend – completely
             * independent of the global context manager used by CoinPanel. */
            renderer_.reset(new SoOffscreenRenderer(osmesa_mgr_.get(), vp));
        } else {
            status_text = "OSMesa not available in this build";
            return;
        }
        renderer_->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        renderer_->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
    }

    ~OSMesaPanel() { delete fltk_img; }

    void setScene(SoSeparator* r, SoPerspectiveCamera* c,
                  const std::function<void(SoOffscreenRenderer*)>& cfg = {}) {
        root = r; cam = c; status_text.clear();
        configure_renderer_fn = cfg;
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
        renderer_->clearBackgroundGradient();
        if (configure_renderer_fn) configure_renderer_fn(renderer_.get());

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
        if (on_rendered) on_rendered();
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
        /* Only call setValue() when the value has actually changed to avoid
         * spurious camera nodeId bumps (see NanoRTPanel::setCamera comment). */
        float qx=orient[0], qy=orient[1], qz=orient[2], qw=orient[3];
        float len = sqrtf(qx*qx+qy*qy+qz*qz+qw*qw);
        if (len > 1e-6f) { qx/=len; qy/=len; qz/=len; qw/=len; }
        float angle = 2.0f*acosf(qw);
        SbVec3f ax(qx,qy,qz);
        if (ax.length() < 1e-6f) { ax=SbVec3f(0,1,0); angle=0; } else ax.normalize();
        SbRotation newRot(ax, angle);

        SbVec3f curPos = cam->position.getValue();
        if (fabsf(curPos[0]-pos[0]) > 1e-6f || fabsf(curPos[1]-pos[1]) > 1e-6f ||
                fabsf(curPos[2]-pos[2]) > 1e-6f)
            cam->position.setValue(pos[0], pos[1], pos[2]);
        if (!cam->orientation.getValue().equals(newRot, 1e-5f))
            cam->orientation.setValue(newRot);
        if (dist > 0.0f && fabsf(cam->focalDistance.getValue() - dist) > 1e-6f)
            cam->focalDistance.setValue(dist);
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
        int h_ = h();
        switch (event) {
        case FL_PUSH: {
            take_focus();
            dragging_ = true;
            drag_btn_ = Fl::event_button();
            last_x_   = Fl::event_x() - x();
            last_y_   = Fl::event_y() - y();
            SoMouseButtonEvent ev;
            ev.setButton(drag_btn_==1 ? SoMouseButtonEvent::BUTTON1 :
                         drag_btn_==2 ? SoMouseButtonEvent::BUTTON2 :
                                        SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::DOWN);
            ev.setPosition(SbVec2s((short)last_x_, (short)(h_-last_y_)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            dragger_active_ = ha.isHandled();
            return 1;
        }
        case FL_RELEASE: {
            dragging_ = false;
            dragger_active_ = false;
            int rx = Fl::event_x()-x(), ry = Fl::event_y()-y();
            SoMouseButtonEvent ev;
            ev.setButton(Fl::event_button()==1 ? SoMouseButtonEvent::BUTTON1 :
                         Fl::event_button()==2 ? SoMouseButtonEvent::BUTTON2 :
                                                 SoMouseButtonEvent::BUTTON3);
            ev.setState(SoButtonEvent::UP);
            ev.setPosition(SbVec2s((short)rx, (short)(h_-ry)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            notifyCameraChanged(); refreshRender(); return 1;
        }
        case FL_DRAG: {
            if (!dragging_) return 1;
            int ex = Fl::event_x()-x(), ey = Fl::event_y()-y();
            int dx = ex - last_x_, dy = ey - last_y_;
            last_x_ = ex; last_y_ = ey;
            SoLocation2Event ev;
            ev.setPosition(SbVec2s((short)ex, (short)(h_-ey)));
            ev.setTime(SbTime::getTimeOfDay());
            SbViewportRegion vp(std::max(w(),1), std::max(h(),1));
            SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(root);
            if (!dragger_active_) {
                if (drag_btn_ == 1) {
                    /* orbit – camera-local yaw and pitch (BRL-CAD style):
                     * symmetric axes, no world-up reference, no gimbal lock */
                    cam->orbitCamera(scene_center_,
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
    bool dragging_        = false;
    bool dragger_active_  = false;  /* true when a scene dragger consumed FL_PUSH */
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
        /* Count only visible children so hidden panels don't occupy width. */
        int n = 0;
        for (int i = 0; i < children(); ++i)
            if (child(i)->visible()) ++n;
        if (n == 0) {
            Fl_Widget::resize(X, Y, W, H);
            init_sizes();  /* keep tile's internal size cache consistent */
            return;
        }
        /* Update our own bounding box without letting Fl_Tile redistribute. */
        Fl_Widget::resize(X, Y, W, H);
        /* Distribute width equally among visible children; push hidden ones
         * clearly off-screen (just past the right edge) at zero width so they
         * are invisible and do not interfere with the tile drag handles. */
        int cw  = W / n;
        int cx  = X;
        int vis = 0;
        for (int i = 0; i < children(); ++i) {
            if (!child(i)->visible()) {
                child(i)->resize(X + W, Y, 0, H);
                continue;
            }
            ++vis;
            int this_w = (vis == n) ? (X + W - cx) : cw;
            child(i)->resize(cx, Y, this_w, H);
            cx += this_w;
        }
        init_sizes();
    }
};

/* -------------------------------------------------------------------------
 * ViewerPanelOpts – initial panel visibility for ObolViewerWindow.
 * Populated by parseViewerArgs() from command-line flags.
 * Default: all compiled-in panels visible.
 * ------------------------------------------------------------------------- */
struct ViewerPanelOpts {
    bool show_osmesa = true;
    bool show_nrt    = true;
    bool show_embree = true;
    bool show_vulkan = true;
};


class ObolViewerWindow : public Fl_Double_Window {
    Fl_Hold_Browser*  browser_;
    CoinPanel*        coin_panel_;
    Fl_Button*        reload_btn_;
    Fl_Button*        save_btn_;
    Fl_Box*           status_bar_;         /* toolbar: shows current scene name */
    Fl_Box*           diff_bar_ = nullptr; /* bottom bar: shows live diff metrics */
    EqualTile*        tile_     = nullptr;
#ifdef OBOL_VIEWER_OSMESA_PANEL
    OSMesaPanel*      osmesa_panel_  = nullptr;
    Fl_Check_Button*  osmesa_toggle_ = nullptr;
    bool              osmesa_enabled_ = true;
#endif
#ifdef OBOL_VIEWER_NANORT
    NanoRTPanel*      nrt_panel_   = nullptr;
    Fl_Check_Button*  nrt_toggle_  = nullptr;
    bool              nrt_enabled_ = true;
#endif
#ifdef OBOL_VIEWER_EMBREE
    EmbreePanel*      emb_panel_   = nullptr;
    Fl_Check_Button*  emb_toggle_  = nullptr;
    bool              emb_enabled_ = true;
#endif
#ifdef OBOL_VIEWER_VULKAN
    VulkanPanel*      vk_panel_    = nullptr;
    Fl_Check_Button*  vk_toggle_   = nullptr;
    bool              vk_enabled_  = true;
#endif
    Fl_Check_Button*  sync_btn_  = nullptr;
    bool              syncing_   = false;
    std::string       current_scene_;

    static const int BROWSER_W = 220;
    static const int TOOLBAR_H = 32;
    static const int STATUS_H  = 48; /* tall enough for up to 3 comparison lines */

public:
    ObolViewerWindow(int W, int H, const ViewerPanelOpts& opts)
        : Fl_Double_Window(W, H, "Obol Scene Viewer")
    {
#ifdef OBOL_VIEWER_OSMESA_PANEL
        osmesa_enabled_ = opts.show_osmesa;
#endif
#ifdef OBOL_VIEWER_NANORT
        nrt_enabled_ = opts.show_nrt;
#endif
#ifdef OBOL_VIEWER_EMBREE
        emb_enabled_ = opts.show_embree;
#endif
#ifdef OBOL_VIEWER_VULKAN
        vk_enabled_ = opts.show_vulkan;
#endif
        begin();
        buildUI(W, H);
        end();
        resizable(this);
        /* Ensure all FLTK windows (including the hidden GL context window
         * created by FLTKContextManager) are hidden when the user closes the
         * main window, so that Fl::run() can return cleanly. */
        callback(closeCB);
    }

    void loadScene(const char* name) {
        coin_panel_->loadScene(name);
        current_scene_ = name;

        /* Look up raytracer compatibility flags from the unified test registry. */
        const ObolTest::TestEntry* entry =
            ObolTest::TestRegistry::instance().findTest(name);
        const bool nanort_ok = entry ? entry->nanort_ok : true;
        const bool embree_ok = entry ? entry->embree_ok : true;

        /* Load scene into each visible optional panel. */
#ifdef OBOL_VIEWER_OSMESA_PANEL
        if (osmesa_panel_ && osmesa_panel_->visible() &&
                coin_panel_->state && coin_panel_->state->root) {
            osmesa_panel_->setScene(coin_panel_->state->root,
                                    coin_panel_->state->cam,
                                    entry ? entry->configure_renderer : nullptr);
        }
#endif
#ifdef OBOL_VIEWER_NANORT
        if (nrt_panel_ && nrt_panel_->visible() &&
                coin_panel_->state && coin_panel_->state->root) {
            nrt_panel_->setScene(coin_panel_->state->root,
                                 coin_panel_->state->cam, nanort_ok);
        }
#endif
#ifdef OBOL_VIEWER_EMBREE
        if (emb_panel_ && emb_panel_->visible() &&
                coin_panel_->state && coin_panel_->state->root) {
            emb_panel_->setScene(coin_panel_->state->root,
                                 coin_panel_->state->cam, embree_ok);
        }
#endif
#ifdef OBOL_VIEWER_VULKAN
        if (vk_panel_ && vk_panel_->visible() &&
                coin_panel_->state && coin_panel_->state->root) {
            vk_panel_->setScene(coin_panel_->state->root,
                                coin_panel_->state->cam);
        }
#endif

        std::string s = "Scene: "; s += name;
        status_bar_->copy_label(s.c_str());
        updateDiffBar();
    }

private:
    /* ---- coin panel label, chosen at compile time ---- */
    /* Macro combinations used for the system-GL (CoinPanel) label:
     *   OBOL_BUILD_DUAL_GL: set when dual-GL build; OBOL_VIEWER_FLTK_GL is
     *     also set so that the system-GL panel uses the same working Fl_Gl_Window
     *     context path as single-GL builds.
     *   OBOL_OSMESA_BUILD:  set when pure-OSMesa; FLTK GL is never set.
     *   OBOL_VIEWER_FLTK_GL: set for portable (FLTKContextManager hidden window) context.
     * Conditions are ordered by precedence: dual-GL first, then OSMesa, then
     * FLTK GL, then the legacy GLX fallback. */
    static const char* coinLabel() {
#if defined(OBOL_BUILD_DUAL_GL)
        return "System GL";
#elif defined(OBOL_OSMESA_BUILD)
        return "OSMesa (headless)";
#else
        return "System GL";
#endif
    }

    void buildUI(int W, int H) {
        int content_h = H - TOOLBAR_H - STATUS_H;

        browser_ = new Fl_Hold_Browser(0, 0, BROWSER_W, content_h);
        browser_->textsize(12);
        browser_->callback(browserCB, this);
        {
            auto tests = getVisualTests();
            for (const auto* e : tests) {
                std::string label =
                    ObolTest::categoryToString(e->category) + "/" + e->name;
                browser_->add(label.c_str());
            }
        }

        /* Render area: EqualTile so panels resize uniformly.
         * All compiled-in panels are always created; runtime flags (set from
         * constructor opts) control initial visibility.  Hidden panels are
         * pushed off-screen with zero width by EqualTile::resize().  The tile
         * redistributes width equally among visible panels on every resize. */
        tile_ = new EqualTile(BROWSER_W, 0, W - BROWSER_W, content_h);
        {
            coin_panel_ = new CoinPanel(BROWSER_W, 0, W - BROWSER_W, content_h,
                                        coinLabel());
#ifdef OBOL_VIEWER_OSMESA_PANEL
            osmesa_panel_ = new OSMesaPanel(BROWSER_W, 0, W - BROWSER_W, content_h,
                                            "OSMesa (per-renderer backend)");
            if (!osmesa_enabled_) osmesa_panel_->hide();
#endif
#ifdef OBOL_VIEWER_NANORT
            nrt_panel_ = new NanoRTPanel(BROWSER_W, 0, W - BROWSER_W, content_h,
                                         "NanoRT (app-supplied renderer)");
            if (!nrt_enabled_) nrt_panel_->hide();
#endif
#ifdef OBOL_VIEWER_EMBREE
            emb_panel_ = new EmbreePanel(BROWSER_W, 0, W - BROWSER_W, content_h,
                                         "Embree (app-supplied renderer)");
            if (!emb_enabled_) emb_panel_->hide();
#endif
#ifdef OBOL_VIEWER_VULKAN
            vk_panel_ = new VulkanPanel(BROWSER_W, 0, W - BROWSER_W, content_h,
                                        "Vulkan (app-supplied renderer)");
            if (!vk_enabled_) vk_panel_->hide();
#endif
        }
        tile_->end();
        /* Redistribute width according to initial visibility. */
        tile_->resize(tile_->x(), tile_->y(), tile_->w(), tile_->h());

        /* Wire inter-panel callbacks once; they check visible() at call time. */
        setupCallbacks();

        /* Toolbar */
        Fl_Group* tb = new Fl_Group(0, content_h, W, TOOLBAR_H);
        tb->box(FL_UP_BOX);
        {
            int bx = 6, by = content_h+4, bh = TOOLBAR_H-8;
            reload_btn_ = new Fl_Button(bx, by, 70, bh, "Reload");
            reload_btn_->callback(reloadCB, this); bx += 76;
            save_btn_ = new Fl_Button(bx, by, 80, bh, "Save RGB...");
            save_btn_->callback(saveCB, this); bx += 86;
            /* Per-panel toggle checkboxes (only for compiled-in panels). */
#ifdef OBOL_VIEWER_OSMESA_PANEL
            osmesa_toggle_ = new Fl_Check_Button(bx, by, 75, bh, "OSMesa");
            osmesa_toggle_->value(osmesa_enabled_ ? 1 : 0);
            osmesa_toggle_->callback(osmesaToggleCB, this); bx += 81;
#endif
#ifdef OBOL_VIEWER_NANORT
            nrt_toggle_ = new Fl_Check_Button(bx, by, 72, bh, "NanoRT");
            nrt_toggle_->value(nrt_enabled_ ? 1 : 0);
            nrt_toggle_->callback(nrtToggleCB, this); bx += 78;
#endif
#ifdef OBOL_VIEWER_EMBREE
            emb_toggle_ = new Fl_Check_Button(bx, by, 70, bh, "Embree");
            emb_toggle_->value(emb_enabled_ ? 1 : 0);
            emb_toggle_->callback(embToggleCB, this); bx += 76;
#endif
#ifdef OBOL_VIEWER_VULKAN
            vk_toggle_ = new Fl_Check_Button(bx, by, 70, bh, "Vulkan");
            vk_toggle_->value(vk_enabled_ ? 1 : 0);
            vk_toggle_->callback(vkToggleCB, this); bx += 76;
#endif
#if defined(OBOL_VIEWER_OSMESA_PANEL) || defined(OBOL_VIEWER_NANORT) || defined(OBOL_VIEWER_EMBREE) || defined(OBOL_VIEWER_VULKAN)
            sync_btn_ = new Fl_Check_Button(bx, by, 74, bh, "Sync All");
            sync_btn_->value(1); bx += 80;
#endif
            status_bar_ = new Fl_Box(bx, by, W-bx-6, bh, "Ready");
            status_bar_->align(FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
            status_bar_->labelsize(12);
        }
        tb->end();

        diff_bar_ = new Fl_Box(0, content_h+TOOLBAR_H, W, STATUS_H, "");
        diff_bar_->box(FL_ENGRAVED_BOX);
        diff_bar_->align(FL_ALIGN_TOP|FL_ALIGN_LEFT|FL_ALIGN_INSIDE);
        diff_bar_->labelsize(11);
        diff_bar_->tooltip(
            "max_diff: maximum per-channel difference (0-255)\n"
            "RMSE: root mean square error across all channels\n"
            "rows_with_diff: % of rows containing any channel diff > 1");
    }

    /* Redistribute EqualTile widths after a panel is shown or hidden. */
    void relayoutPanels() {
        if (!tile_) return;
        tile_->resize(tile_->x(), tile_->y(), tile_->w(), tile_->h());
        redraw();
    }

    /* Wire all-to-all sync callbacks and post-render diff callbacks.
     * Called once from buildUI() after all panels are created.
     *
     * Camera-sync callbacks use refreshRender()/refreshFromSync() directly
     * (not setCamera()) to avoid spurious Coin3D nodeId bumps that would
     * trick the CPU-raytracing BVH cache into skipping geometry rebuilds.
     * They also check visible() so toggling a panel off pauses its sync. */
    void setupCallbacks() {
        coin_panel_->on_camera_changed = [this](CoinPanel* /*src*/) {
            if (syncing_ || !sync_btn_ || !sync_btn_->value()) return;
            syncing_ = true;
#ifdef OBOL_VIEWER_OSMESA_PANEL
            if (osmesa_panel_ && osmesa_panel_->visible())
                osmesa_panel_->refreshRender();
#endif
#ifdef OBOL_VIEWER_NANORT
            if (nrt_panel_ && nrt_panel_->visible())
                nrt_panel_->refreshFromSync();
#endif
#ifdef OBOL_VIEWER_EMBREE
            if (emb_panel_ && emb_panel_->visible())
                emb_panel_->refreshFromSync();
#endif
#ifdef OBOL_VIEWER_VULKAN
            if (vk_panel_ && vk_panel_->visible())
                vk_panel_->refreshFromSync();
#endif
            syncing_ = false;
        };
#ifdef OBOL_VIEWER_OSMESA_PANEL
        if (osmesa_panel_) {
            osmesa_panel_->on_camera_changed = [this](OSMesaPanel* /*src*/) {
                if (syncing_ || !sync_btn_ || !sync_btn_->value()) return;
                syncing_ = true;
                coin_panel_->refreshRender();
#  ifdef OBOL_VIEWER_NANORT
                if (nrt_panel_ && nrt_panel_->visible())
                    nrt_panel_->refreshFromSync();
#  endif
#  ifdef OBOL_VIEWER_EMBREE
                if (emb_panel_ && emb_panel_->visible())
                    emb_panel_->refreshFromSync();
#  endif
                syncing_ = false;
            };
        }
#endif
#ifdef OBOL_VIEWER_NANORT
        if (nrt_panel_) {
            nrt_panel_->on_camera_changed = [this](NanoRTPanel* /*src*/) {
                if (syncing_ || !sync_btn_ || !sync_btn_->value()) return;
                syncing_ = true;
                coin_panel_->refreshRender();
#  ifdef OBOL_VIEWER_OSMESA_PANEL
                if (osmesa_panel_ && osmesa_panel_->visible())
                    osmesa_panel_->refreshRender();
#  endif
#  ifdef OBOL_VIEWER_EMBREE
                if (emb_panel_ && emb_panel_->visible())
                    emb_panel_->refreshFromSync();
#  endif
                syncing_ = false;
            };
        }
#endif
#ifdef OBOL_VIEWER_EMBREE
        if (emb_panel_) {
            emb_panel_->on_camera_changed = [this](EmbreePanel* /*src*/) {
                if (syncing_ || !sync_btn_ || !sync_btn_->value()) return;
                syncing_ = true;
                coin_panel_->refreshRender();
#  ifdef OBOL_VIEWER_OSMESA_PANEL
                if (osmesa_panel_ && osmesa_panel_->visible())
                    osmesa_panel_->refreshRender();
#  endif
#  ifdef OBOL_VIEWER_NANORT
                if (nrt_panel_ && nrt_panel_->visible())
                    nrt_panel_->refreshFromSync();
#  endif
                syncing_ = false;
            };
        }
#endif
        /* Wire post-render callbacks so the diff bar updates automatically. */
        coin_panel_->on_rendered = [this]() { updateDiffBar(); };
#ifdef OBOL_VIEWER_OSMESA_PANEL
        if (osmesa_panel_)
            osmesa_panel_->on_rendered = [this]() { updateDiffBar(); };
#endif
#ifdef OBOL_VIEWER_NANORT
        if (nrt_panel_)
            nrt_panel_->on_rendered = [this]() { updateDiffBar(); };
#endif
#ifdef OBOL_VIEWER_EMBREE
        if (emb_panel_)
            emb_panel_->on_rendered = [this]() { updateDiffBar(); };
#endif
#ifdef OBOL_VIEWER_VULKAN
        if (vk_panel_) {
            vk_panel_->on_camera_changed = [this](VulkanPanel* /*src*/) {
                if (syncing_ || !sync_btn_ || !sync_btn_->value()) return;
                syncing_ = true;
                coin_panel_->refreshRender();
#  ifdef OBOL_VIEWER_OSMESA_PANEL
                if (osmesa_panel_ && osmesa_panel_->visible())
                    osmesa_panel_->refreshRender();
#  endif
#  ifdef OBOL_VIEWER_NANORT
                if (nrt_panel_ && nrt_panel_->visible())
                    nrt_panel_->refreshFromSync();
#  endif
#  ifdef OBOL_VIEWER_EMBREE
                if (emb_panel_ && emb_panel_->visible())
                    emb_panel_->refreshFromSync();
#  endif
                syncing_ = false;
            };
            vk_panel_->on_rendered = [this]() { updateDiffBar(); };
        }
#endif
    }

    static void closeCB(Fl_Widget*, void*) {
        /* Hide all FLTK windows so Fl::run() returns cleanly when the user
         * closes the main window.  Hiding all windows covers both the
         * FLTK_GL build (where CoinPanel is an Fl_Gl_Window subwindow) and
         * any fallback hidden context window that FLTKContextManager may
         * have created. */
        while (Fl_Window* w = Fl::first_window())
            w->hide();
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

    /* Panel toggle callbacks – show/hide a panel, redistribute tile widths,
     * load the current scene if needed, then refresh the diff bar. */
#ifdef OBOL_VIEWER_OSMESA_PANEL
    static void osmesaToggleCB(Fl_Widget* w, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        bool on = static_cast<Fl_Check_Button*>(w)->value() != 0;
        self->osmesa_enabled_ = on;
        if (on) {
            self->osmesa_panel_->show();
            self->relayoutPanels();
            /* Load scene into newly-visible panel. */
            if (!self->current_scene_.empty() &&
                    self->coin_panel_->state && self->coin_panel_->state->root) {
                const ObolTest::TestEntry* entry =
                    ObolTest::TestRegistry::instance().findTest(
                        self->current_scene_.c_str());
                self->osmesa_panel_->setScene(
                    self->coin_panel_->state->root,
                    self->coin_panel_->state->cam,
                    entry ? entry->configure_renderer : nullptr);
            }
        } else {
            self->osmesa_panel_->hide();
            self->relayoutPanels();
        }
        self->updateDiffBar();
    }
#endif
#ifdef OBOL_VIEWER_NANORT
    static void nrtToggleCB(Fl_Widget* w, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        bool on = static_cast<Fl_Check_Button*>(w)->value() != 0;
        self->nrt_enabled_ = on;
        if (on) {
            self->nrt_panel_->show();
            self->relayoutPanels();
            if (!self->current_scene_.empty() &&
                    self->coin_panel_->state && self->coin_panel_->state->root) {
                const ObolTest::TestEntry* entry =
                    ObolTest::TestRegistry::instance().findTest(
                        self->current_scene_.c_str());
                const bool nanort_ok = entry ? entry->nanort_ok : true;
                self->nrt_panel_->setScene(
                    self->coin_panel_->state->root,
                    self->coin_panel_->state->cam, nanort_ok);
            }
        } else {
            self->nrt_panel_->hide();
            self->relayoutPanels();
        }
        self->updateDiffBar();
    }
#endif
#ifdef OBOL_VIEWER_EMBREE
    static void embToggleCB(Fl_Widget* w, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        bool on = static_cast<Fl_Check_Button*>(w)->value() != 0;
        self->emb_enabled_ = on;
        if (on) {
            self->emb_panel_->show();
            self->relayoutPanels();
            if (!self->current_scene_.empty() &&
                    self->coin_panel_->state && self->coin_panel_->state->root) {
                const ObolTest::TestEntry* entry =
                    ObolTest::TestRegistry::instance().findTest(
                        self->current_scene_.c_str());
                const bool embree_ok = entry ? entry->embree_ok : true;
                self->emb_panel_->setScene(
                    self->coin_panel_->state->root,
                    self->coin_panel_->state->cam, embree_ok);
            }
        } else {
            self->emb_panel_->hide();
            self->relayoutPanels();
        }
        self->updateDiffBar();
    }
#endif
#ifdef OBOL_VIEWER_VULKAN
    static void vkToggleCB(Fl_Widget* w, void* data) {
        auto* self = static_cast<ObolViewerWindow*>(data);
        bool on = static_cast<Fl_Check_Button*>(w)->value() != 0;
        self->vk_enabled_ = on;
        if (on) {
            self->vk_panel_->show();
            self->relayoutPanels();
            if (!self->current_scene_.empty() &&
                    self->coin_panel_->state && self->coin_panel_->state->root) {
                self->vk_panel_->setScene(
                    self->coin_panel_->state->root,
                    self->coin_panel_->state->cam);
            }
        } else {
            self->vk_panel_->hide();
            self->relayoutPanels();
        }
        self->updateDiffBar();
    }
#endif

    /* Compute pixel-difference metrics across all visible rendered panels and
     * display them on the diff_bar_ status line at the bottom of the window.
     * Works for any number of compiled-in panels (including none). */
    void updateDiffBar() {
        if (!diff_bar_) return;

        struct PanelImg {
            const char*    label;
            const uint8_t* buf;   /* RGB top-down */
            int            pw, ph;
        };
        std::vector<PanelImg> panels;

        if (coin_panel_->visible() && !coin_panel_->display_buf.empty())
            panels.push_back({coin_panel_->label_text.c_str(),
                              coin_panel_->display_buf.data(),
                              coin_panel_->w(), coin_panel_->h()});
#ifdef OBOL_VIEWER_OSMESA_PANEL
        if (osmesa_panel_ && osmesa_panel_->visible() &&
                !osmesa_panel_->display_buf.empty())
            panels.push_back({osmesa_panel_->label_text.c_str(),
                              osmesa_panel_->display_buf.data(),
                              osmesa_panel_->w(), osmesa_panel_->h()});
#endif
#ifdef OBOL_VIEWER_NANORT
        if (nrt_panel_ && nrt_panel_->visible() &&
                !nrt_panel_->display_buf.empty())
            panels.push_back({nrt_panel_->label_text.c_str(),
                              nrt_panel_->display_buf.data(),
                              nrt_panel_->w(), nrt_panel_->h()});
#endif
#ifdef OBOL_VIEWER_EMBREE
        if (emb_panel_ && emb_panel_->visible() &&
                !emb_panel_->display_buf.empty())
            panels.push_back({emb_panel_->label_text.c_str(),
                              emb_panel_->display_buf.data(),
                              emb_panel_->w(), emb_panel_->h()});
#endif
#ifdef OBOL_VIEWER_VULKAN
        if (vk_panel_ && vk_panel_->visible() &&
                !vk_panel_->display_buf.empty())
            panels.push_back({vk_panel_->label_text.c_str(),
                              vk_panel_->display_buf.data(),
                              vk_panel_->w(), vk_panel_->h()});
#endif

        if (panels.size() < 2) {
            diff_bar_->copy_label("");
            diff_bar_->redraw();
            return;
        }

        /* Compare each pair and build a multi-line report (one pair per line).
         * Strip the parenthetical suffix from verbose label_text strings so
         * each line stays short enough to be read without truncation. */
        auto shortLabel = [](const std::string& lbl) -> std::string {
            size_t p = lbl.find(" (");
            return p != std::string::npos ? lbl.substr(0, p) : lbl;
        };

        std::string msg;
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
                if (!msg.empty()) msg += '\n';
                std::snprintf(line, sizeof(line),
                    "%s vs %s:  max_diff=%d  RMSE=%.2f  rows_with_diff=%.1f%%",
                    shortLabel(pa.label).c_str(), shortLabel(pb.label).c_str(),
                    max_diff, rmse, pct_diff);
                msg += line;
            }
        }

        diff_bar_->copy_label(msg.c_str());
        diff_bar_->redraw();
    }

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
        r->clearBackgroundGradient();
        if (self->coin_panel_->configure_renderer_fn)
            self->coin_panel_->configure_renderer_fn(r);
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
 * parseViewerArgs()
 *
 * Extracts obol_viewer-specific flags from argv and returns initial panel
 * visibility settings.  Custom flags are removed from the argv copy passed
 * to FLTK so FLTK's own argument handler does not see unknown options.
 *
 * Supported flags:
 *   --system-enable   (system GL panel – always visible, this is the base)
 *   --osmesa-enable   OSMesa panel visible at startup
 *   --nanort-enable   NanoRT panel visible at startup
 *   --embree-enable   Embree panel visible at startup
 *   --vulkan-enable   Vulkan panel visible at startup
 *
 * If no --*-enable flags are provided every compiled-in panel starts visible.
 * If one or more flags are provided only the named panels start visible.
 * ======================================================================= */
static ViewerPanelOpts parseViewerArgs(int argc, char** argv,
                                       int* fltk_argc_out,
                                       std::vector<char*>* fltk_argv_out)
{
    bool any_enable  = false;
    bool want_osmesa = false, want_nrt = false, want_embree = false;
    bool want_vulkan = false;

    fltk_argv_out->clear();
    fltk_argv_out->push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--system-enable") == 0) {
            any_enable = true;
            /* system GL is always visible; flag selects "only system" config */
        } else if (strcmp(argv[i], "--osmesa-enable") == 0) {
            any_enable = true; want_osmesa = true;
        } else if (strcmp(argv[i], "--nanort-enable") == 0) {
            any_enable = true; want_nrt = true;
        } else if (strcmp(argv[i], "--embree-enable") == 0) {
            any_enable = true; want_embree = true;
        } else if (strcmp(argv[i], "--vulkan-enable") == 0) {
            any_enable = true; want_vulkan = true;
        } else {
            fltk_argv_out->push_back(argv[i]);
        }
    }
    fltk_argv_out->push_back(nullptr);
    *fltk_argc_out = static_cast<int>(fltk_argv_out->size()) - 1;

    ViewerPanelOpts opts;
    if (any_enable) {
        opts.show_osmesa = want_osmesa;
        opts.show_nrt    = want_nrt;
        opts.show_embree = want_embree;
        opts.show_vulkan = want_vulkan;
    }
    return opts;
}

/* =========================================================================
 * main()
 * ======================================================================= */
int main(int argc, char** argv)
{
    /* Parse obol_viewer-specific flags before FLTK sees argv. */
    int              fltk_argc = 0;
    std::vector<char*> fltk_argv_storage;
    ViewerPanelOpts opts = parseViewerArgs(argc, argv, &fltk_argc,
                                           &fltk_argv_storage);
    char** fltk_argv = fltk_argv_storage.data();

    /* Initialise Obol using the same context manager pattern as the tests */
    initCoinHeadless();

    Fl::scheme("gtk+");
    /* Request a double-buffered RGB visual.  Fall back to single-buffer if
     * the display does not support a double-buffered OpenGL visual so that
     * the viewer still opens and functions (rendering uses SoOffscreenRenderer,
     * not a FLTK GL window). */
    if (!Fl::visual(FL_RGB | FL_DOUBLE))
        Fl::visual(FL_RGB);

    ObolViewerWindow* win = new ObolViewerWindow(1100, 700, opts);
    win->show(fltk_argc, fltk_argv);

    /* Wait for the main window to be fully exposed before loading scenes.
     * FLTKContextManager creates its hidden 1×1 GL context window lazily on
     * the first render request (inside loadScene → refreshRender → r->render).
     * The Fl::check() + XSync in FLTKContextManager::ensureWindow() correctly
     * initialises that hidden window without needing any explicit priming here. */
    win->wait_for_expose();

    /* Load the first visual scene automatically */
    {
        auto tests = getVisualTests();
        if (!tests.empty())
            win->loadScene(tests[0]->name.c_str());
    }

    return Fl::run();
}
