/*
 * obol_minimalist_viewer.cpp  –  Obol scene browser using BasicFLTKContextManager
 *
 * Overview
 * ────────
 * obol_minimalist_viewer is a counterpart to obol_viewer that deliberately
 * uses the most limited GL context Obol can be given: a plain FLTK 1×1
 * hidden window with no Pbuffers, no FBConfig selection and no per-context
 * sharing (BasicFLTKContextManager).
 *
 * PURPOSE
 * ───────
 * Iterating through every scene in the testlib catalogue with this viewer
 * tells you immediately which scenes crash, produce wrong output, or emit
 * graceful-degradation warnings under a "basic" application-provided context.
 * This is the same class of context that a naive application hands to Obol
 * when it creates its own Fl_Gl_Window and doesn't set up Pbuffers.
 *
 * WHAT IT SHOWS
 * ─────────────
 * • A full scene browser (all testlib scenes, same as obol_viewer).
 * • A single rendering panel powered by BasicFLTKContextManager.
 * • A "Context Capabilities" strip at the bottom listing what the context
 *   provides: GL version, vendor/renderer, and each major feature toggle
 *   (FBO, VBO, texture objects, multitexture, 3-D textures, GLSL, …).
 *   Features shown in red are MISSING; they will trigger Obol's new
 *   graceful-degradation warnings instead of crashing.
 * • A status bar that shows any warning emitted by Obol during the last
 *   render (so you can see "FBO not available" etc. without reading the
 *   terminal).
 *
 * Camera interaction (same as obol_viewer):
 *   Left-drag  → orbit
 *   Right-drag → dolly
 *   Scroll     → zoom
 *
 * Building
 * ────────
 * Enabled automatically when OBOL_BUILD_VIEWER=ON and FLTK is available.
 * Add -DOBOL_BUILD_MINIMALIST_VIEWER=ON at cmake configure time to force-
 * enable it independent of OBOL_BUILD_VIEWER.
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

/* ---- Testlib scene registry ---- */
#include "testlib/test_registry.h"

/* ---- BasicFLTKContextManager ---- */
#include "utils/fltk_context_manager.h"

/* ---- FLTK ---- */
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <FL/Fl_File_Chooser.H>

#include <memory>
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <cstring>

/* =========================================================================
 * Singleton BasicFLTKContextManager (registered with SoDB::init)
 * ========================================================================= */
static BasicFLTKContextManager* s_basic_mgr = nullptr;

/* =========================================================================
 * Scene helpers (identical to obol_viewer)
 * ========================================================================= */

static std::vector<const ObolTest::TestEntry*> getVisualTests()
{
    std::vector<const ObolTest::TestEntry*> out;
    for (const auto& e : ObolTest::TestRegistry::instance().allTests())
        if (e.has_visual && e.create_scene)
            out.push_back(&e);
    return out;
}

static SoKeyboardEvent::Key fltkKeyToSo(int fltk_key)
{
    if (fltk_key >= 0x20 && fltk_key <= 0x7e)
        return static_cast<SoKeyboardEvent::Key>(fltk_key);
    switch (fltk_key) {
    case FL_BackSpace:  return SoKeyboardEvent::BACKSPACE;
    case FL_Tab:        return SoKeyboardEvent::TAB;
    case FL_Enter:      return SoKeyboardEvent::RETURN;
    case FL_Escape:     return SoKeyboardEvent::ESCAPE;
    case FL_Delete:     return SoKeyboardEvent::KEY_DELETE;
    case FL_Home:       return SoKeyboardEvent::HOME;
    case FL_End:        return SoKeyboardEvent::END;
    case FL_Page_Up:    return SoKeyboardEvent::PAGE_UP;
    case FL_Page_Down:  return SoKeyboardEvent::PAGE_DOWN;
    case FL_Left:       return SoKeyboardEvent::LEFT_ARROW;
    case FL_Right:      return SoKeyboardEvent::RIGHT_ARROW;
    case FL_Up:         return SoKeyboardEvent::UP_ARROW;
    case FL_Down:       return SoKeyboardEvent::DOWN_ARROW;
    case FL_Shift_L:    return SoKeyboardEvent::LEFT_SHIFT;
    case FL_Shift_R:    return SoKeyboardEvent::RIGHT_SHIFT;
    case FL_Control_L:  return SoKeyboardEvent::LEFT_CONTROL;
    case FL_Control_R:  return SoKeyboardEvent::RIGHT_CONTROL;
    case FL_Alt_L:      return SoKeyboardEvent::LEFT_ALT;
    case FL_Alt_R:      return SoKeyboardEvent::RIGHT_ALT;
    case FL_F + 1:      return SoKeyboardEvent::F1;
    case FL_F + 2:      return SoKeyboardEvent::F2;
    case FL_F + 3:      return SoKeyboardEvent::F3;
    case FL_F + 4:      return SoKeyboardEvent::F4;
    case FL_F + 5:      return SoKeyboardEvent::F5;
    case FL_F + 6:      return SoKeyboardEvent::F6;
    case FL_F + 7:      return SoKeyboardEvent::F7;
    case FL_F + 8:      return SoKeyboardEvent::F8;
    case FL_F + 9:      return SoKeyboardEvent::F9;
    case FL_F + 10:     return SoKeyboardEvent::F10;
    case FL_F + 11:     return SoKeyboardEvent::F11;
    case FL_F + 12:     return SoKeyboardEvent::F12;
    default:            return SoKeyboardEvent::ANY;
    }
}

struct SceneState {
    SoSeparator*         root         = nullptr;
    SoPerspectiveCamera* cam          = nullptr;
    SbVec3f              scene_center = SbVec3f(0,0,0);
    int width = 800; int height = 600;
    bool dragging = false; bool dragger_active = false;
    int drag_btn = 0; int last_x = 0; int last_y = 0;

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

    void computeCenter() {
        if (!root) return;
        SoGetBoundingBoxAction bba(SbViewportRegion(width, height));
        bba.apply(root);
        SbBox3f bbox = bba.getBoundingBox();
        if (!bbox.isEmpty()) scene_center = bbox.getCenter();
    }

    void updateClipping() {
        if (!cam) return;
        float d = cam->focalDistance.getValue();
        if (d < 1e-4f) d = 1e-4f;
        cam->nearDistance.setValue(d * 0.001f);
        cam->farDistance.setValue(d * 10000.0f);
    }
};

/* Shared off-screen renderer — uses BasicFLTKContextManager. */
static SoOffscreenRenderer* s_renderer = nullptr;
static SoOffscreenRenderer* getRenderer(int w, int h) {
    if (!s_renderer) {
        SbViewportRegion vp(w, h);
        s_renderer = new SoOffscreenRenderer(s_basic_mgr, vp);
    }
    return s_renderer;
}

/* =========================================================================
 * CapabilitiesStrip  –  bottom bar showing what the basic context provides
 * ========================================================================= */
class CapabilitiesStrip : public Fl_Box {
public:
    explicit CapabilitiesStrip(int X, int Y, int W, int H)
        : Fl_Box(X, Y, W, H, "")
    {
        box(FL_FLAT_BOX);
        color(fl_rgb_color(30, 30, 30));
    }

    /* Call after the first render so the GL context exists. */
    void refresh() {
        if (!s_basic_mgr) return;
        caps_ = s_basic_mgr->queryCapabilities();
        rebuildLabel();
        redraw();
    }

    void setLastWarning(const std::string& w) {
        last_warning_ = w;
        rebuildLabel();
        redraw();
    }

    void draw() override {
        Fl_Box::draw();
        /* Draw two rows of capability badges. */
        if (!caps_.initialized) {
            fl_color(fl_rgb_color(180, 180, 180));
            fl_font(FL_HELVETICA, 11);
            fl_draw("(GL context not yet initialised — load a scene)", x()+6, y()+12);
            return;
        }
        int cx = x() + 6;
        int cy = y() + 13;
        fl_font(FL_HELVETICA_BOLD, 11);

        /* Row 1: version/vendor */
        std::string header = "GL " + caps_.version
                           + "  |  " + caps_.vendor
                           + "  /  " + caps_.renderer;
        fl_color(fl_rgb_color(220, 220, 160));
        fl_draw(header.c_str(), cx, cy);

        /* Row 2: feature badges */
        cy += 16;
        fl_font(FL_HELVETICA, 11);
        struct Badge { const char* label; bool ok; };
        Badge badges[] = {
            { "FBO",         caps_.has_fbo        },
            { "VBO",         caps_.has_vbo        },
            { "TexObj",      caps_.has_texobj      },
            { "MultiTex",    caps_.has_multitex    },
            { "3DTex",       caps_.has_3dtex       },
            { "Compressed",  caps_.has_compressed  },
            { "GLSL",        caps_.has_glsl        },
            { "VertexArr",   caps_.has_vertex_arr  },
        };
        for (const auto& b : badges) {
            fl_color(b.ok ? fl_rgb_color(80, 200, 80) : fl_rgb_color(220, 60, 60));
            std::string txt = std::string(b.ok ? "✓" : "✗") + b.label;
            fl_draw(txt.c_str(), cx, cy);
            cx += (int)fl_width(txt.c_str()) + 14;
        }

        /* Row 3: last warning (if any) */
        if (!last_warning_.empty()) {
            fl_font(FL_HELVETICA, 10);
            fl_color(fl_rgb_color(255, 160, 60));
            fl_draw(("⚠ " + last_warning_).c_str(), x()+6, y()+h()-4);
        }
    }

private:
    BasicFLTKContextManager::GLCapabilities caps_;
    std::string last_warning_;

    void rebuildLabel() {}  /* Layout is done entirely in draw(). */
};

/* =========================================================================
 * MinimalistPanel  –  rendering panel (identical logic to CoinPanel in
 *                     obol_viewer, but standalone and with warning capture)
 * ========================================================================= */
class MinimalistPanel : public Fl_Box {
public:
    std::unique_ptr<SceneState> state;
    std::function<void(SoOffscreenRenderer*)> configure_renderer_fn;
    std::function<void()> on_rendered;          ///< fired after each render
    std::string last_warning;                   ///< last Obol diagnostic

    explicit MinimalistPanel(int X, int Y, int W, int H)
        : Fl_Box(X, Y, W, H, "")
        , fltk_img_(nullptr)
        , gl_failed_(false)
    {
        box(FL_FLAT_BOX);
        color(FL_BLACK);
    }

    ~MinimalistPanel() { delete fltk_img_; }

    void loadScene(const char* name) {
        const ObolTest::TestEntry* entry =
            ObolTest::TestRegistry::instance().findTest(name);
        if (!entry || !entry->create_scene) {
            status_ = std::string("Unknown scene: ") + name;
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
        status_.clear();
        last_warning.clear();
        gl_failed_ = false;
        refreshRender();
    }

    void clearScene() {
        state.reset();
        configure_renderer_fn = nullptr;
        delete fltk_img_; fltk_img_ = nullptr;
        redraw();
    }

    void refreshRender() {
        if (!state || !state->root) { redraw(); return; }
        if (gl_failed_) { redraw(); return; }

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
            gl_failed_ = true;
            status_ = "GL context unavailable";
            redraw(); return;
        }

        const unsigned char* src = r->getBuffer();
        if (!src) { status_ = "No buffer"; redraw(); return; }

        /* bottom-up RGBA → top-down RGB */
        display_buf_.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = src + (size_t)(ph-1-row) * pw * 4;
            uint8_t*       d = display_buf_.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; s+=4; d+=3;
            }
        }
        delete fltk_img_;
        fltk_img_ = new Fl_RGB_Image(display_buf_.data(), pw, ph, 3);
        status_.clear();
        redraw();
        if (on_rendered) on_rendered();
    }

    void draw() override {
        fl_rectf(x(), y(), w(), h(), FL_BLACK);
        if (fltk_img_) {
            fltk_img_->draw(x(), y(), w(), h(), 0, 0);
        } else {
            fl_color(FL_WHITE); fl_font(FL_HELVETICA, 14);
            fl_draw("(no scene)", x()+w()/2-40, y()+h()/2);
        }
        if (!status_.empty()) {
            fl_color(fl_rgb_color(255, 80, 80)); fl_font(FL_HELVETICA, 12);
            fl_draw(status_.c_str(), x()+6, y()+h()-6);
        }
    }

    int handle(int event) override {
        if (!state || !state->root) return Fl_Box::handle(event);
        int h_ = h();
        const int ev_x = Fl::event_x() - x();
        const int ev_y = Fl::event_y() - y();
        switch (event) {
        case FL_PUSH:
            take_focus();
            if (state) {
                state->dragging   = true;
                state->drag_btn   = Fl::event_button();
                state->last_x     = ev_x;
                state->last_y     = ev_y;
                SoMouseButtonEvent ev;
                ev.setButton(state->drag_btn==1 ? SoMouseButtonEvent::BUTTON1 :
                             state->drag_btn==2 ? SoMouseButtonEvent::BUTTON2 :
                                                  SoMouseButtonEvent::BUTTON3);
                ev.setState(SoButtonEvent::DOWN);
                ev.setPosition(SbVec2s((short)ev_x,(short)(h_-ev_y)));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                state->dragger_active = ha.isHandled();
            }
            return 1;
        case FL_RELEASE:
            if (state) {
                state->dragging = false; state->dragger_active = false;
                SoMouseButtonEvent ev;
                ev.setButton(Fl::event_button()==1 ? SoMouseButtonEvent::BUTTON1 :
                             Fl::event_button()==2 ? SoMouseButtonEvent::BUTTON2 :
                                                     SoMouseButtonEvent::BUTTON3);
                ev.setState(SoButtonEvent::UP);
                ev.setPosition(SbVec2s((short)ev_x,(short)(h_-ev_y)));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                refreshRender();
            }
            return 1;
        case FL_DRAG:
            if (state && state->dragging && state->cam) {
                int dx = ev_x-state->last_x, dy = ev_y-state->last_y;
                state->last_x = ev_x; state->last_y = ev_y;
                SoLocation2Event ev;
                ev.setPosition(SbVec2s((short)ev_x,(short)(h_-ev_y)));
                ev.setTime(SbTime::getTimeOfDay());
                SbViewportRegion vp(state->width, state->height);
                SoHandleEventAction ha(vp); ha.setEvent(&ev); ha.apply(state->root);
                if (!state->dragger_active) {
                    if (state->drag_btn == 1) {
                        state->cam->orbitCamera(state->scene_center,
                                                (float)dx, (float)dy,
                                                0.01f * (180.0f / static_cast<float>(M_PI)));
                    } else if (state->drag_btn == 3) {
                        float dist = state->cam->focalDistance.getValue();
                        dist *= (1.0f + dy * 0.01f);
                        if (dist < 0.1f) dist = 0.1f;
                        SbVec3f dir = state->cam->position.getValue() - state->scene_center;
                        dir.normalize();
                        state->cam->position.setValue(state->scene_center + dir*dist);
                        state->cam->focalDistance.setValue(dist);
                        state->updateClipping();
                    }
                }
                refreshRender();
            }
            return 1;
        case FL_MOUSEWHEEL: {
            if (state && state->cam) {
                float delta = -(float)Fl::event_dy();
                float dist  = state->cam->focalDistance.getValue();
                dist *= (1.0f - delta * 0.1f);
                if (dist < 0.1f) dist = 0.1f;
                SbVec3f dir = state->cam->position.getValue() - state->scene_center;
                dir.normalize();
                state->cam->position.setValue(state->scene_center + dir*dist);
                state->cam->focalDistance.setValue(dist);
                state->updateClipping();
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
    std::vector<uint8_t> display_buf_;
    Fl_RGB_Image*        fltk_img_;
    std::string          status_;
    bool                 gl_failed_;
};

/* =========================================================================
 * MinimalistViewerWindow
 * =========================================================================
 *
 * Layout
 * ──────
 *  ┌──────────────┬──────────────────────────────────────────┐
 *  │ Scene browser│  MinimalistPanel (rendering)             │
 *  │ (Fl_Browser) │  BasicFLTKContextManager                 │
 *  │              │                                          │
 *  ├──────────────┴──────────────────────────────────────────┤
 *  │ [Reload]  [Save RGB]                                    │
 *  ├─────────────────────────────────────────────────────────┤
 *  │ CapabilitiesStrip (GL version, feature badges, warning) │
 *  └─────────────────────────────────────────────────────────┘
 */
class MinimalistViewerWindow : public Fl_Double_Window {
public:
    static constexpr int BROWSER_W  = 220;
    static constexpr int TOOLBAR_H  = 30;
    static constexpr int CAPS_H     = 56;
    /* Extra message row height (only visible when a degradation warning fires) */
    static constexpr int WARN_H     = 0; /* baked into CAPS_H */

    MinimalistViewerWindow(int W, int H)
        : Fl_Double_Window(W, H, "Obol Minimalist Viewer  [BasicFLTKContextManager]")
    {
        buildUI(W, H);
        end();
        resizable(render_panel_);
    }

    void loadScene(const char* name) {
        current_scene_ = name ? name : "";
        render_panel_->loadScene(name);
        /* After first render the GL context exists — refresh capabilities. */
        caps_strip_->refresh();
    }

private:
    Fl_Browser*        browser_      = nullptr;
    MinimalistPanel*   render_panel_ = nullptr;
    CapabilitiesStrip* caps_strip_   = nullptr;
    std::string        current_scene_;

    void buildUI(int W, int H) {
        /* ---- content area (everything above caps strip) ---- */
        const int content_h = H - TOOLBAR_H - CAPS_H;

        /* Scene browser */
        browser_ = new Fl_Browser(0, 0, BROWSER_W, content_h);
        browser_->type(FL_HOLD_BROWSER);
        browser_->callback(browserCB, this);
        browser_->color(fl_rgb_color(40,40,50));
        browser_->textcolor(fl_rgb_color(210,210,210));
        browser_->textsize(12);

        /* Populate browser with category/name entries. */
        for (const auto* e : getVisualTests()) {
            std::string label =
                "@b" + ObolTest::categoryToString(e->category) + "/@." + e->name;
            browser_->add(label.c_str(), (void*)e);
        }

        /* Rendering panel */
        render_panel_ = new MinimalistPanel(BROWSER_W, 0, W-BROWSER_W, content_h);
        render_panel_->on_rendered = [this]() {
            /* After every render: query capabilities (idempotent after first). */
            caps_strip_->refresh();
        };

        /* ---- Toolbar ---- */
        Fl_Group* toolbar = new Fl_Group(0, content_h, W, TOOLBAR_H);
        toolbar->box(FL_FLAT_BOX);
        toolbar->color(fl_rgb_color(50, 50, 60));
        {
            Fl_Button* reload = new Fl_Button(4, content_h+2, 80, 26, "Reload");
            reload->callback(reloadCB, this);
            reload->labelsize(12);

            Fl_Button* save = new Fl_Button(90, content_h+2, 90, 26, "Save RGB");
            save->callback(saveCB, this);
            save->labelsize(12);

            /* Context info button — prints capabilities to stdout. */
            Fl_Button* info = new Fl_Button(188, content_h+2, 100, 26, "Print Caps");
            info->callback(printCapsCB, this);
            info->labelsize(12);
        }
        toolbar->end();

        /* ---- Capabilities strip ---- */
        caps_strip_ = new CapabilitiesStrip(0, content_h+TOOLBAR_H, W, CAPS_H);
    }

    /* ---- Callbacks ---- */

    static void browserCB(Fl_Widget*, void* data) {
        auto* self = static_cast<MinimalistViewerWindow*>(data);
        int line = self->browser_->value();
        if (!line) return;
        const ObolTest::TestEntry* e =
            static_cast<const ObolTest::TestEntry*>(self->browser_->data(line));
        if (e) self->loadScene(e->name.c_str());
    }

    static void reloadCB(Fl_Widget*, void* data) {
        auto* self = static_cast<MinimalistViewerWindow*>(data);
        if (!self->current_scene_.empty())
            self->loadScene(self->current_scene_.c_str());
    }

    static void saveCB(Fl_Widget*, void* data) {
        auto* self = static_cast<MinimalistViewerWindow*>(data);
        if (!self->render_panel_->state || !self->render_panel_->state->root) {
            fl_message("No scene loaded."); return;
        }
        const char* path = fl_file_chooser("Save RGB", "*.rgb", "scene.rgb");
        if (!path) return;
        int pw = self->render_panel_->w(), ph = self->render_panel_->h();
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
        r->clearBackgroundGradient();
        if (self->render_panel_->configure_renderer_fn)
            self->render_panel_->configure_renderer_fn(r);
        if (!r->render(self->render_panel_->state->root)) {
            fl_message("Render failed."); return;
        }
        if (r->writeToRGB(path))
            fl_message("Saved to:\n%s", path);
        else
            fl_message("Failed to write file.");
    }

    static void printCapsCB(Fl_Widget*, void* data) {
        auto* self = static_cast<MinimalistViewerWindow*>(data);
        if (!s_basic_mgr) { printf("BasicFLTKContextManager not set.\n"); return; }
        const auto& c = s_basic_mgr->queryCapabilities();
        if (!c.initialized) { printf("GL context not yet initialised.\n"); return; }
        printf("\n=== BasicFLTKContextManager GL Capabilities ===\n");
        printf("  Version:    %s\n", c.version.c_str());
        printf("  Vendor:     %s\n", c.vendor.c_str());
        printf("  Renderer:   %s\n", c.renderer.c_str());
        printf("  FBO:        %s\n", c.has_fbo        ? "YES" : "NO  ← SoSceneTexture2/shadows need this");
        printf("  VBO:        %s\n", c.has_vbo        ? "YES" : "NO  ← vertex-buffer acceleration");
        printf("  TexObj:     %s\n", c.has_texobj     ? "YES" : "NO  ← textures (GL >= 1.1 required)");
        printf("  MultiTex:   %s\n", c.has_multitex   ? "YES" : "NO  ← multi-texture units");
        printf("  3DTex:      %s\n", c.has_3dtex      ? "YES" : "NO  ← SoTexture3 needs this");
        printf("  Compressed: %s\n", c.has_compressed ? "YES" : "NO");
        printf("  GLSL:       %s\n", c.has_glsl       ? "YES" : "NO  ← SoShaderProgram needs this");
        printf("  VertexArr:  %s\n", c.has_vertex_arr ? "YES" : "NO  ← vertex arrays (GL >= 1.1)");
        printf("=================================================\n\n");
        fflush(stdout);
    }
};

/* =========================================================================
 * main
 * ========================================================================= */
int main(int argc, char** argv)
{
    /* Create BasicFLTKContextManager and register it as the Obol context. */
    BasicFLTKContextManager basicMgr;
    s_basic_mgr = &basicMgr;

    SoDB::init(&basicMgr);
    SoNodeKit::init();
    SoInteraction::init();

    Fl::scheme("gtk+");
    if (!Fl::visual(FL_RGB | FL_DOUBLE))
        Fl::visual(FL_RGB);

    MinimalistViewerWindow* win = new MinimalistViewerWindow(1000, 680);
    win->show(argc, argv);
    win->wait_for_expose();

    /* Auto-load the first scene. */
    auto tests = getVisualTests();
    if (!tests.empty())
        win->loadScene(tests[0]->name.c_str());

    return Fl::run();
}
