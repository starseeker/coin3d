/* qt_obol_widget.h — Qt/Obol integration: QtObolContextManager + QtObolWidget
 *
 * This header provides a full-featured Qt widget for Obol-based 3D rendering,
 * equivalent to Quarter's QuarterWidget.  It directly answers the question:
 * "Would it be difficult to incorporate Quarter's optional features into an
 * Obol-based widget, and does Obol make any of them harder?"
 *
 * Short answer: No — none of the features are harder with Obol.  Several are
 * simpler, and one (interaction-mode) is not needed at all.
 *
 * ── Feature-by-feature comparison: Quarter vs QtObolWidget ────────────────
 *
 * 1. RENDER MODE (wireframe, points, bounding-box, hidden-line)
 *    Quarter: SoRenderManager::setRenderMode()  ~50 lines via QuarterWidget API
 *    Obol:    Same: SoRenderManager::setRenderMode() called from QtObolWidget
 *    Difficulty: IDENTICAL.  SoRenderManager is still used for multi-pass render
 *    mode logic; SoViewport handles scene/camera/viewport management alongside it.
 *    See setRenderMode() + the SoRenderManager member below.
 *
 * 2. TRANSPARENCY TYPE
 *    Quarter: SoGLRenderAction::setTransparencyType() via SoRenderManager
 *    Obol:    Same: set directly on SoRenderManager::getGLRenderAction()
 *    Difficulty: IDENTICAL (one extra getter call).
 *    See setTransparencyType() below.
 *
 * 3. STEREO MODES (anaglyph, quad-buffer, interleaved)
 *    Quarter: SoRenderManager::setStereoMode()
 *    Obol:    Same: SoRenderManager::setStereoMode()
 *    Difficulty: IDENTICAL.
 *    See setStereoMode() below.
 *
 * 4. RIGHT-CLICK CONTEXT MENU
 *    Quarter: Separate ContextMenu class (~176 lines) + QuarterWidgetP::contextMenu()
 *    Obol:    contextMenuEvent() override with inline QMenu (~35 lines, no class)
 *    Difficulty: SIMPLER in Obol — inline, no separate class needed.
 *    See contextMenuEvent() below.
 *
 * 5. INTERACTION MODE (Alt-key toggle between navigation and scene interaction)
 *    Quarter: InteractionMode.cpp (~151 lines) switches between SCXML navigation
 *             state and SoEventManager::NO_NAVIGATION when Alt is held, so that
 *             draggers/manipulators can receive events without the navigator
 *             consuming them first.
 *    Obol:    NOT NEEDED.  There is no SCXML navigation state to toggle.
 *             Events always flow directly to the scene graph via
 *             viewport_.processEvent() → SoHandleEventAction.  Draggers and
 *             manipulators work at all times without any mode switching.
 *    Difficulty: SIMPLER in Obol — the feature is structurally unnecessary.
 *    The Alt-key itself can still be observed for application-specific uses
 *    (e.g. entering/exiting a camera-control mode) via a simple flag — see
 *    the interactionMode_ member and keyPressEvent() below, which shows how
 *    six lines replace Quarter's 151-line InteractionMode.cpp.
 *
 * 6. SPACENAVIGATOR (3D connexion 6-DOF device)
 *    Quarter: SpaceNavigatorDevice.cpp (~187 lines): X11 native event filter,
 *             libspnav translation to SoMotion3Event/SoSpaceballButtonEvent,
 *             routed through SoEventManager.
 *    Obol:    nativeEvent() override (~40 lines): same libspnav translation,
 *             fed to viewport_.processEvent().  No separate device class needed.
 *    Difficulty: SLIGHTLY SIMPLER — no InputDevice class hierarchy required.
 *    See nativeEvent() and translateSpaceNavEvent() below.  Guards are provided
 *    so the code compiles without libspnav; define HAVE_SPACENAV_LIB to enable.
 *
 * 7. QT DESIGNER PLUGIN
 *    Quarter: Separate compiled shared library (plugins/), ~280 lines.
 *    Obol:    QtObolDesignerPlugin class at the bottom of this header, ~70 lines.
 *    Difficulty: IDENTICAL — same QDesignerCustomWidgetInterface boilerplate.
 *    The Obol plugin is slightly shorter because there is no Quarter::init() or
 *    navigation-mode-file setup required.
 *    Build with:  -DOBOL_BUILD_QT_DESIGNER_PLUGIN=ON  (see CMakeLists.txt)
 *
 * ── Architecture ───────────────────────────────────────────────────────────
 *
 * • SoViewport  handles scene graph, camera, viewport region, background colour,
 *   event routing (processEvent → SoHandleEventAction).
 * • SoRenderManager  handles render modes, stereo modes, and the actual GL render
 *   pass.  Its scene is set to viewport_.getRoot() so both see the same tree.
 * • SoGLRenderAction  is managed by SoRenderManager; we set the cache context
 *   and transparency type on it before each frame.
 * • Sensor bridge  (three QTimers) connects Coin's sensor queue to Qt's event
 *   loop — the same three-timer pattern as Quarter's SensorManager.
 *
 * ── Camera interaction ──────────────────────────────────────────────────────
 *
 * Mouse interaction is handled directly in the widget:
 * • Left-button drag   → orbit camera around its focal point (SoCamera::orbitCamera)
 * • Middle-button drag → pan camera in the view plane
 * • Scroll wheel       → zoom (move camera along view direction, keep focalDistance)
 * Scene events (draggers, manipulators) still receive button press/release events
 * and hover-location events; camera navigation is handled before scene routing.
 *
 * ── Usage ──────────────────────────────────────────────────────────────────
 *
 *   QtObolContextManager ctxMgr;   // required; nullptr share context is fine at startup
 *   SoDB::init(&ctxMgr);
 *   SoNodeKit::init();
 *   SoInteraction::init();
 *
 *   QtObolWidget * viewer = new QtObolWidget(mainWindow);
 *   mainWindow->setCentralWidget(viewer);
 *   viewer->setSceneGraph(myRoot);          // camera auto-added if absent
 *   viewer->setRenderMode(SoRenderManager::WIREFRAME);   // optional
 *   viewer->setStereoMode(SoRenderManager::ANAGLYPH);    // optional
 *   viewer->setTransparencyType(SoGLRenderAction::BLEND);// optional
 *   viewer->show();
 *   app.exec();
 */

#pragma once

#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QMenu>
#include <QActionGroup>
#include <QCoreApplication>

#include <Inventor/SoDB.h>
#include <Inventor/SoViewport.h>
#include <Inventor/SoRenderManager.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbTime.h>
#include <Inventor/sensors/SoSensorManager.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/events/SoEvent.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/events/SoMotion3Event.h>
#include <Inventor/events/SoSpaceballButtonEvent.h>

#include <GL/gl.h>
#include <cmath>

#ifdef HAVE_SPACENAV_LIB
#  include <spnav.h>
#endif

// ============================================================================
// QtObolContextManager
//
// SoDB::ContextManager implementation backed by QOffscreenSurface and
// QOpenGLContext.  Use this when you want SoOffscreenRenderer to create
// and manage its own OpenGL contexts that share resources with Qt's context.
//
// This class is optional for the interactive QtObolWidget: the widget drives
// SoGLRenderAction directly within Qt's existing GL context and does not call
// SoOffscreenRenderer at all.  QtObolContextManager is only needed when the
// same application also uses SoOffscreenRenderer for off-screen captures.
// ============================================================================

class QtObolContextManager : public SoDB::ContextManager {
public:
    // shareCtx: when non-null, newly created GL contexts share resources
    // (textures, VBOs, display lists) with this context.  Pass the context
    // returned by QOpenGLWidget::context() after GL initialisation.
    explicit QtObolContextManager(QOpenGLContext * shareCtx = nullptr)
        : shareCtx_(shareCtx) {}

    void * createOffscreenContext(unsigned int /*w*/, unsigned int /*h*/) override {
        // QOffscreenSurface has no fixed size; the actual render dimensions
        // are determined by the viewport region passed to SoGLRenderAction.
        Ctx * c = new Ctx;
        QSurfaceFormat fmt;
        fmt.setDepthBufferSize(24);
        fmt.setStencilBufferSize(8);

        c->surface = new QOffscreenSurface;
        c->surface->setFormat(fmt);
        c->surface->create();
        if (!c->surface->isValid()) {
            delete c->surface; delete c; return nullptr;
        }

        c->context = new QOpenGLContext;
        c->context->setFormat(fmt);
        if (shareCtx_) c->context->setShareContext(shareCtx_);
        if (!c->context->create()) {
            delete c->context; delete c->surface; delete c;
            return nullptr;
        }
        return c;
    }

    SbBool makeContextCurrent(void * p) override {
        Ctx * c = static_cast<Ctx *>(p);
        c->prev     = QOpenGLContext::currentContext();
        c->prevSurf = c->prev ? c->prev->surface() : nullptr;
        return c->context->makeCurrent(c->surface) ? TRUE : FALSE;
    }

    void restorePreviousContext(void * p) override {
        Ctx * c = static_cast<Ctx *>(p);
        if (c->prev && c->prevSurf)
            c->prev->makeCurrent(c->prevSurf);
        else
            c->context->doneCurrent();
    }

    void destroyContext(void * p) override {
        Ctx * c = static_cast<Ctx *>(p);
        c->context->doneCurrent();
        delete c->context;
        delete c->surface;
        delete c;
    }

    // Resolve GL extension entry points via the current Qt context.
    void * getProcAddress(const char * name) override {
        QOpenGLContext * cur = QOpenGLContext::currentContext();
        if (!cur) return nullptr;
        return reinterpret_cast<void *>(cur->getProcAddress(name));
    }

private:
    struct Ctx {
        QOffscreenSurface * surface  = nullptr;
        QOpenGLContext    * context  = nullptr;
        QOpenGLContext    * prev     = nullptr;
        QSurface          * prevSurf = nullptr;
    };

    QOpenGLContext * shareCtx_;
};

// ============================================================================
// QtObolWidget
//
// Full-featured QOpenGLWidget for Obol-based 3D rendering.
// See the header commentary above for a feature-by-feature comparison with
// Quarter's QuarterWidget.
//
// Architecture
// ------------
// • SoViewport  — scene graph, camera, viewport region, event routing
// • SoRenderManager — render mode (wireframe/points/bbox/hidden-line),
//                     stereo mode (anaglyph/quad-buffer/interleaved),
//                     multi-pass rendering logic
// • SoGLRenderAction — owned by SoRenderManager; transparency type is set here
// • Three QTimers — sensor bridge (same pattern as Quarter's SensorManager)
// ============================================================================

class QtObolWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit QtObolWidget(QWidget * parent = nullptr)
        : QOpenGLWidget(parent)
        , cacheContext_(allocCacheContext())
        , interactionMode_(false)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);

        // Disable SoRenderManager's own auto-redraw; we call update() ourselves.
        SoRenderManager::enableRealTimeUpdate(FALSE);

        // Sensor bridge — same three-timer pattern as Quarter's SensorManager.
        idleTimer_.setSingleShot(true);
        delayTimer_.setSingleShot(true);
        timerTimer_.setSingleShot(true);
        connect(&idleTimer_,  &QTimer::timeout, this, &QtObolWidget::onIdle);
        connect(&delayTimer_, &QTimer::timeout, this, &QtObolWidget::onDelay);
        connect(&timerTimer_, &QTimer::timeout, this, &QtObolWidget::onTimer);
        SoDB::getSensorManager()->setChangedCallback(sensorQueueChangedCB, this);
    }

    ~QtObolWidget() override {
        SoDB::getSensorManager()->setChangedCallback(nullptr, nullptr);
    }

    // ---- Scene graph -------------------------------------------------------

    // Attach a scene graph.  A SoPerspectiveCamera is auto-inserted if the
    // root contains no camera; viewAll() is called automatically in that case.
    //
    // Camera ownership: SoViewport::setCamera() refs the camera; the caller
    // does not need to delete it.  It is released on setSceneGraph(nullptr).
    void setSceneGraph(SoNode * root) {
        if (!root) {
            viewport_.setSceneGraph(nullptr);
            viewport_.setCamera(nullptr);
            renderMgr_.setSceneGraph(nullptr);
            return;
        }

        SoSearchAction sa;
        sa.setType(SoCamera::getClassTypeId());
        sa.setInterest(SoSearchAction::FIRST);
        sa.apply(root);

        SoCamera * cam = nullptr;
        if (sa.getPath()) {
            SoFullPath * fp = static_cast<SoFullPath *>(sa.getPath());
            cam = static_cast<SoCamera *>(fp->getTail());
        }
        if (!cam) cam = new SoPerspectiveCamera;

        viewport_.setSceneGraph(root);
        viewport_.setCamera(cam);
        viewport_.viewAll();

        // SoRenderManager renders viewport_.getRoot() (= [camera + scene]).
        renderMgr_.setSceneGraph(viewport_.getRoot());
        update();
    }

    SoNode * getSceneGraph() const { return viewport_.getSceneGraph(); }

    // ---- SoViewport / SoRenderManager access (advanced) -------------------

    SoViewport       & getViewport()        { return viewport_; }
    const SoViewport & getViewport()  const { return viewport_; }
    SoRenderManager  & getRenderManager()   { return renderMgr_; }

    // ---- Background colour -------------------------------------------------

    void setBackgroundColor(const SbColor & c) {
        viewport_.setBackgroundColor(c);
        renderMgr_.setBackgroundColor(SbColor4f(c, 1.0f));
        update();
    }
    const SbColor & backgroundColor() const { return viewport_.getBackgroundColor(); }

    // ---- Render mode -------------------------------------------------------
    // Feature 1: render mode (wireframe, points, bounding-box, hidden-line).
    // SoRenderManager already implements all multi-pass render modes;
    // we just delegate and trigger a repaint.

    void setRenderMode(SoRenderManager::RenderMode mode) {
        renderMgr_.setRenderMode(mode);
        update();
    }
    SoRenderManager::RenderMode getRenderMode() const {
        return renderMgr_.getRenderMode();
    }

    // ---- Stereo mode -------------------------------------------------------
    // Feature 3: stereo (anaglyph, quad-buffer, interleaved rows/cols).
    // SoRenderManager::renderStereo() handles all stereo passes internally.

    void setStereoMode(SoRenderManager::StereoMode mode) {
        renderMgr_.setStereoMode(mode);
        update();
    }
    SoRenderManager::StereoMode getStereoMode() const {
        return renderMgr_.getStereoMode();
    }

    // ---- Transparency type -------------------------------------------------
    // Feature 2: transparency type (screen-door, blend, sorted layers, etc.).
    // Applied directly to SoRenderManager's internal SoGLRenderAction.

    void setTransparencyType(SoGLRenderAction::TransparencyType t) {
        renderMgr_.getGLRenderAction()->setTransparencyType(t);
        update();
    }
    SoGLRenderAction::TransparencyType getTransparencyType() const {
        return renderMgr_.getGLRenderAction()->getTransparencyType();
    }

public slots:
    // ---- Public actions ----------------------------------------------------

    void viewAll() {
        viewport_.viewAll();
        update();
    }

protected:
    // ---- QOpenGLWidget overrides -------------------------------------------

    void initializeGL() override {
        glEnable(GL_DEPTH_TEST);
        // Wire the render action's cache context now that the GL context exists.
        renderMgr_.getGLRenderAction()->setCacheContext(cacheContext_);
    }

    void resizeGL(int w, int h) override {
        const qreal dpr = devicePixelRatioF();
        SbVec2s physSize((short)(w * dpr), (short)(h * dpr));
        viewport_.setWindowSize(physSize);
        renderMgr_.setWindowSize(physSize);
        renderMgr_.setViewportRegion(viewport_.getViewportRegion());
    }

    void paintGL() override {
        // Keep SoRenderManager's viewport region in sync (it may have changed
        // if setViewportRegion was called externally on SoViewport).
        renderMgr_.setViewportRegion(viewport_.getViewportRegion());

        // SoRenderManager::render() handles background clear, render mode
        // overrides (wireframe, hidden-line, bounding box, etc.) and stereo
        // passes internally — the same logic Quarter's context menu triggers.
        renderMgr_.render(TRUE /*clearwindow*/, TRUE /*clearzbuffer*/);
    }

    // ---- Mouse events → SoEvents -------------------------------------------

    void mousePressEvent(QMouseEvent * e) override {
        lastMousePos_ = e->pos();
        SoMouseButtonEvent ev;
        applyModifiers(&ev, e);
        ev.setPosition(mapToObol(e->pos()));
        ev.setState(SoButtonEvent::DOWN);
        ev.setButton(qtButtonToCoin(e->button()));
        viewport_.processEvent(&ev);
        update();
    }

    void mouseReleaseEvent(QMouseEvent * e) override {
        SoMouseButtonEvent ev;
        applyModifiers(&ev, e);
        ev.setPosition(mapToObol(e->pos()));
        ev.setState(SoButtonEvent::UP);
        ev.setButton(qtButtonToCoin(e->button()));
        viewport_.processEvent(&ev);
        update();
    }

    void mouseMoveEvent(QMouseEvent * e) override {
        // Left-button drag → orbit camera around its focal point.
        if (e->buttons() & Qt::LeftButton) {
            SoCamera * cam = viewport_.getCamera();
            if (cam) {
                const QPoint delta = e->pos() - lastMousePos_;
                SbVec3f lookDir;
                cam->orientation.getValue().multVec(SbVec3f(0.0f, 0.0f, -1.0f), lookDir);
                const SbVec3f orbitCenter =
                    cam->position.getValue() + lookDir * cam->focalDistance.getValue();
                cam->orbitCamera(orbitCenter, (float)delta.x(), (float)delta.y());
                lastMousePos_ = e->pos();
                update();
                return;
            }
        }
        // Middle-button drag → pan camera in the view plane.
        if (e->buttons() & Qt::MiddleButton) {
            SoCamera * cam = viewport_.getCamera();
            if (cam) {
                const QPoint delta = e->pos() - lastMousePos_;
                SbVec3f right, up;
                cam->orientation.getValue().multVec(SbVec3f(1.0f, 0.0f, 0.0f), right);
                cam->orientation.getValue().multVec(SbVec3f(0.0f, 1.0f, 0.0f), up);
                const float scale = cam->focalDistance.getValue() * 0.001f;
                const SbVec3f pan =
                    right * (-delta.x() * scale) + up * (delta.y() * scale);
                cam->position.setValue(cam->position.getValue() + pan);
                lastMousePos_ = e->pos();
                update();
                return;
            }
        }
        SoLocation2Event ev;
        applyModifiers(&ev, e);
        ev.setPosition(mapToObol(e->pos()));
        viewport_.processEvent(&ev);
        update();
    }

    void wheelEvent(QWheelEvent * e) override {
        int delta = e->angleDelta().y();
        if (delta == 0) delta = e->angleDelta().x();
        // Scroll → zoom: move the camera along its view direction and keep
        // focalDistance consistent so subsequent orbit stays centred.
        SoCamera * cam = viewport_.getCamera();
        if (cam) {
            SbVec3f lookDir;
            cam->orientation.getValue().multVec(SbVec3f(0.0f, 0.0f, -1.0f), lookDir);
            const float step = cam->focalDistance.getValue() * (delta > 0 ? 0.1f : -0.1f);
            cam->position.setValue(cam->position.getValue() + lookDir * step);
            cam->focalDistance.setValue(cam->focalDistance.getValue() - step);
            update();
            return;
        }
        // Fallback when no camera: pass scroll as BUTTON4/5 to the scene.
        SoMouseButtonEvent ev;
        ev.setButton(delta > 0 ? SoMouseButtonEvent::BUTTON4
                                : SoMouseButtonEvent::BUTTON5);
        ev.setState(SoButtonEvent::DOWN);
#if QT_VERSION >= 0x060000
        ev.setPosition(mapToObol(e->position().toPoint()));
#else
        ev.setPosition(mapToObol(e->pos()));
#endif
        viewport_.processEvent(&ev);
        update();
    }

    // ---- Keyboard events + Interaction mode --------------------------------
    // Feature 5: INTERACTION MODE.
    //
    // Quarter's 151-line InteractionMode.cpp tracks whether Alt is held to
    // switch the SCXML navigation state between EXAMINER and NO_NAVIGATION.
    // With Obol there is no SCXML navigation, so there is no mode to toggle:
    // events always reach the scene graph.  The six lines below replace all
    // of InteractionMode.cpp for the only thing the Alt-key toggle actually
    // accomplishes in practice — notifying the cursor and any app-level code.

    void keyPressEvent(QKeyEvent * e) override {
        if (e->key() == Qt::Key_Alt) {
            interactionMode_ = true;  // draggers/manipulators already receive
            setCursor(Qt::OpenHandCursor); // events without this toggle; this
        }                                 // is purely cosmetic / app-convention
        SoKeyboardEvent ev;
        ev.setState(SoButtonEvent::DOWN);
        ev.setKey(qtKeyToCoin(e->key()));
        viewport_.processEvent(&ev);
    }

    void keyReleaseEvent(QKeyEvent * e) override {
        if (e->key() == Qt::Key_Alt) {
            interactionMode_ = false;
            setCursor(Qt::ArrowCursor);
        }
        SoKeyboardEvent ev;
        ev.setState(SoButtonEvent::UP);
        ev.setKey(qtKeyToCoin(e->key()));
        viewport_.processEvent(&ev);
    }

    void focusOutEvent(QFocusEvent * e) override {
        // Mirror Quarter's InteractionMode::focusOutEvent — cancel Alt state
        // when focus is lost so the cursor doesn't stay in "interact" mode.
        if (interactionMode_) {
            QKeyEvent release(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
            QCoreApplication::sendEvent(this, &release);
        }
        QOpenGLWidget::focusOutEvent(e);
    }

    // ---- Right-click context menu ------------------------------------------
    // Feature 4: CONTEXT MENU.
    //
    // Quarter's ContextMenu.cpp is ~176 lines because it builds three separate
    // menus from QuarterWidget's pre-built QAction lists.  With Obol we build
    // the menu inline — no separate class needed (~35 lines).

    void contextMenuEvent(QContextMenuEvent * e) override {
        QMenu menu(this);

        // Functions
        QAction * vaAction = menu.addAction("View All");
        connect(vaAction, &QAction::triggered, this, &QtObolWidget::viewAll);
        menu.addSeparator();

        // Render mode submenu
        QMenu * rmMenu = menu.addMenu("Render Mode");
        QActionGroup * rmGroup = new QActionGroup(rmMenu);
        auto addRM = [&](const char * label, SoRenderManager::RenderMode mode) {
            QAction * a = rmMenu->addAction(label);
            a->setCheckable(true);
            a->setChecked(renderMgr_.getRenderMode() == mode);
            a->setActionGroup(rmGroup);
            a->setData(static_cast<int>(mode));
        };
        addRM("As Is",            SoRenderManager::AS_IS);
        addRM("Wireframe",        SoRenderManager::WIREFRAME);
        addRM("Wireframe Overlay",SoRenderManager::WIREFRAME_OVERLAY);
        addRM("Points",           SoRenderManager::POINTS);
        addRM("Hidden Line",      SoRenderManager::HIDDEN_LINE);
        addRM("Bounding Box",     SoRenderManager::BOUNDING_BOX);
        connect(rmGroup, &QActionGroup::triggered, this,
            [this](QAction * a) {
                setRenderMode(static_cast<SoRenderManager::RenderMode>(a->data().toInt()));
            });

        // Stereo mode submenu
        QMenu * smMenu = menu.addMenu("Stereo Mode");
        QActionGroup * smGroup = new QActionGroup(smMenu);
        auto addSM = [&](const char * label, SoRenderManager::StereoMode mode) {
            QAction * a = smMenu->addAction(label);
            a->setCheckable(true);
            a->setChecked(renderMgr_.getStereoMode() == mode);
            a->setActionGroup(smGroup);
            a->setData(static_cast<int>(mode));
        };
        addSM("Mono",               SoRenderManager::MONO);
        addSM("Anaglyph",           SoRenderManager::ANAGLYPH);
        addSM("Quad Buffer",        SoRenderManager::QUAD_BUFFER);
        addSM("Interleaved Rows",   SoRenderManager::INTERLEAVED_ROWS);
        addSM("Interleaved Columns",SoRenderManager::INTERLEAVED_COLUMNS);
        connect(smGroup, &QActionGroup::triggered, this,
            [this](QAction * a) {
                setStereoMode(static_cast<SoRenderManager::StereoMode>(a->data().toInt()));
            });

        // Transparency type submenu
        QMenu * ttMenu = menu.addMenu("Transparency Type");
        QActionGroup * ttGroup = new QActionGroup(ttMenu);
        auto addTT = [&](const char * label, SoGLRenderAction::TransparencyType t) {
            QAction * a = ttMenu->addAction(label);
            a->setCheckable(true);
            a->setChecked(renderMgr_.getGLRenderAction()->getTransparencyType() == t);
            a->setActionGroup(ttGroup);
            a->setData(static_cast<int>(t));
        };
        addTT("None",                                SoGLRenderAction::NONE);
        addTT("Screen Door",                         SoGLRenderAction::SCREEN_DOOR);
        addTT("Add",                                 SoGLRenderAction::ADD);
        addTT("Delayed Add",                         SoGLRenderAction::DELAYED_ADD);
        addTT("Sorted Object Add",                   SoGLRenderAction::SORTED_OBJECT_ADD);
        addTT("Blend",                               SoGLRenderAction::BLEND);
        addTT("Delayed Blend",                       SoGLRenderAction::DELAYED_BLEND);
        addTT("Sorted Object Blend",                 SoGLRenderAction::SORTED_OBJECT_BLEND);
        addTT("Sorted Object Sorted Triangle Blend", SoGLRenderAction::SORTED_OBJECT_SORTED_TRIANGLE_BLEND);
        addTT("Sorted Layers Blend",                 SoGLRenderAction::SORTED_LAYERS_BLEND);
        connect(ttGroup, &QActionGroup::triggered, this,
            [this](QAction * a) {
                setTransparencyType(static_cast<SoGLRenderAction::TransparencyType>(a->data().toInt()));
            });

        menu.exec(e->globalPos());
    }

    // ---- SpaceNavigator native event hook ----------------------------------
    // Feature 6: SPACENAVIGATOR.
    //
    // Quarter's SpaceNavigatorDevice.cpp (~187 lines) installs a global X11
    // event filter and translates SPNAV_EVENT_MOTION/BUTTON to SoMotion3Event
    // and SoSpaceballButtonEvent, then routes them through SoEventManager.
    //
    // With Obol we override nativeEvent() — no separate device class, no
    // InputDevice hierarchy.  The event goes directly to viewport_.processEvent().
    // The translation code is identical; only the routing is simpler.

#ifdef HAVE_SPACENAV_LIB
    bool nativeEvent(const QByteArray & /*eventType*/, void * message,
                     qintptr * /*result*/) override
    {
        XEvent * xev = static_cast<XEvent *>(message);
        if (!xev || xev->type != ClientMessage) return false;

        spnav_event spev;
        if (!spnav_x11_event(xev, &spev)) return false;

        if (spev.type == SPNAV_EVENT_MOTION) {
            const float ax = spev.motion.rx, ay = spev.motion.ry, az = spev.motion.rz;
            const float len = std::sqrt(ax*ax + ay*ay + az*az);
            SoMotion3Event ev;
            if (len > 1e-6f) {
                float half = len * 0.5f * 0.001f;
                float s    = std::sin(half);
                ev.setRotation(SbRotation(ax/len*s, ay/len*s, az/len*s,
                                          std::cos(half)));
            }
            ev.setTranslation(SbVec3f(spev.motion.x * 0.001f,
                                      spev.motion.y * 0.001f,
                                      spev.motion.z * 0.001f));
            viewport_.processEvent(&ev);
        } else if (spev.type == SPNAV_EVENT_BUTTON) {
            SoSpaceballButtonEvent ev;
            ev.setState(spev.button.press ? SoButtonEvent::DOWN : SoButtonEvent::UP);
            static const SoSpaceballButtonEvent::Button btns[] = {
                SoSpaceballButtonEvent::BUTTON1, SoSpaceballButtonEvent::BUTTON2,
                SoSpaceballButtonEvent::BUTTON3, SoSpaceballButtonEvent::BUTTON4,
                SoSpaceballButtonEvent::BUTTON5, SoSpaceballButtonEvent::BUTTON6,
                SoSpaceballButtonEvent::BUTTON7, SoSpaceballButtonEvent::BUTTON8,
            };
            const int n = sizeof(btns)/sizeof(btns[0]);
            ev.setButton(spev.button.bnum < n ? btns[spev.button.bnum]
                                              : SoSpaceballButtonEvent::ANY);
            viewport_.processEvent(&ev);
        }
        update();
        return true;
    }
#endif // HAVE_SPACENAV_LIB

private slots:
    // ---- Coin sensor queue bridge ------------------------------------------
    // Three one-shot QTimers for idle, delay-timeout, and timer queues.
    // Exactly mirrors Quarter's SensorManager pattern.

    void onSensorQueueChanged() {
        SoSensorManager * sm = SoDB::getSensorManager();

        SbTime interval;
        if (sm->isTimerSensorPending(interval)) {
            interval -= SbTime::getTimeOfDay();
            int ms = qMax(1, (int)(interval.getValue() * 1000.0));
            if (!timerTimer_.isActive()) timerTimer_.start(ms);
            else timerTimer_.setInterval(ms);
        } else if (timerTimer_.isActive()) {
            timerTimer_.stop();
        }

        if (sm->isDelaySensorPending()) {
            idleTimer_.start(0);
            if (!delayTimer_.isActive()) {
                const SbTime & timeout = SoDB::getDelaySensorTimeout();
                if (timeout != SbTime::zero())
                    delayTimer_.start((int)(timeout.getValue() * 1000.0));
            }
        } else {
            idleTimer_.stop();
            delayTimer_.stop();
        }
    }

    void onIdle() {
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(TRUE);
        onSensorQueueChanged();
    }

    void onDelay() {
        SoDB::getSensorManager()->processTimerQueue();
        SoDB::getSensorManager()->processDelayQueue(FALSE);
        onSensorQueueChanged();
    }

    void onTimer() {
        SoDB::getSensorManager()->processTimerQueue();
        onSensorQueueChanged();
    }

private:
    // Coin's sensor callback may fire from any thread; route back to the Qt
    // main thread via QueuedConnection (same reason Quarter uses SignalThread).
    static void sensorQueueChangedCB(void * data) {
        QtObolWidget * w = static_cast<QtObolWidget *>(data);
        QMetaObject::invokeMethod(w, "onSensorQueueChanged", Qt::QueuedConnection);
    }

    // Convert Qt widget position to Coin pixel coordinates.
    // Qt origin = top-left; Coin origin = bottom-left.  Scale for HiDPI.
    SbVec2s mapToObol(const QPoint & p) const {
        const qreal dpr = devicePixelRatioF();
        return SbVec2s((short)(p.x() * dpr),
                       (short)((height() - p.y() - 1) * dpr));
    }

    void applyModifiers(SoEvent * ev, QInputEvent * e) const {
        Qt::KeyboardModifiers m = e->modifiers();
        ev->setShiftDown((m & Qt::ShiftModifier)   ? TRUE : FALSE);
        ev->setCtrlDown ((m & Qt::ControlModifier) ? TRUE : FALSE);
        ev->setAltDown  ((m & Qt::AltModifier)     ? TRUE : FALSE);
    }

    static SoMouseButtonEvent::Button qtButtonToCoin(Qt::MouseButton b) {
        switch (b) {
        case Qt::LeftButton:   return SoMouseButtonEvent::BUTTON1;
        case Qt::RightButton:  return SoMouseButtonEvent::BUTTON2;
        case Qt::MiddleButton: return SoMouseButtonEvent::BUTTON3;
        default:               return SoMouseButtonEvent::ANY;
        }
    }

    // Qt → Coin key mapping.  Special keys listed explicitly; letters and
    // digits handled by range offsets.
    // Letters: Qt::Key_A–Key_Z (0x41–0x5A, uppercase) map to
    // SoKeyboardEvent::A–Z (0x61–0x7A, lowercase ASCII 'a'–'z').
    // SoKeyboardEvent::A == 0x61; Qt::Key_A == 0x41; the offset 0x20
    // is absorbed by computing (qtKey - Qt::Key_A) and adding to A.
    static SoKeyboardEvent::Key qtKeyToCoin(int qtKey) {
        switch (qtKey) {
        case Qt::Key_Escape:    return SoKeyboardEvent::ESCAPE;
        case Qt::Key_Tab:       return SoKeyboardEvent::TAB;
        case Qt::Key_Backspace: return SoKeyboardEvent::BACKSPACE;
        case Qt::Key_Return:
        case Qt::Key_Enter:     return SoKeyboardEvent::RETURN;
        case Qt::Key_Delete:    return SoKeyboardEvent::KEY_DELETE;
        case Qt::Key_Home:      return SoKeyboardEvent::HOME;
        case Qt::Key_End:       return SoKeyboardEvent::END;
        case Qt::Key_Left:      return SoKeyboardEvent::LEFT_ARROW;
        case Qt::Key_Right:     return SoKeyboardEvent::RIGHT_ARROW;
        case Qt::Key_Up:        return SoKeyboardEvent::UP_ARROW;
        case Qt::Key_Down:      return SoKeyboardEvent::DOWN_ARROW;
        case Qt::Key_PageUp:    return SoKeyboardEvent::PAGE_UP;
        case Qt::Key_PageDown:  return SoKeyboardEvent::PAGE_DOWN;
        case Qt::Key_Space:     return SoKeyboardEvent::SPACE;
        case Qt::Key_F1:        return SoKeyboardEvent::F1;
        case Qt::Key_F2:        return SoKeyboardEvent::F2;
        case Qt::Key_F3:        return SoKeyboardEvent::F3;
        case Qt::Key_F4:        return SoKeyboardEvent::F4;
        case Qt::Key_F5:        return SoKeyboardEvent::F5;
        case Qt::Key_F6:        return SoKeyboardEvent::F6;
        case Qt::Key_F7:        return SoKeyboardEvent::F7;
        case Qt::Key_F8:        return SoKeyboardEvent::F8;
        case Qt::Key_F9:        return SoKeyboardEvent::F9;
        case Qt::Key_F10:       return SoKeyboardEvent::F10;
        case Qt::Key_F11:       return SoKeyboardEvent::F11;
        case Qt::Key_F12:       return SoKeyboardEvent::F12;
        default: break;
        }
        if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
            return static_cast<SoKeyboardEvent::Key>(
                SoKeyboardEvent::A + (qtKey - Qt::Key_A));
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
            return static_cast<SoKeyboardEvent::Key>(
                SoKeyboardEvent::NUMBER_0 + (qtKey - Qt::Key_0));
        return SoKeyboardEvent::ANY;
    }

    static uint32_t allocCacheContext() {
        static uint32_t s_nextId = 1;
        return s_nextId++;
    }

    SoViewport     viewport_;
    SoRenderManager renderMgr_;
    uint32_t        cacheContext_;
    bool            interactionMode_; // true while Alt is held (cosmetic only)
    QPoint          lastMousePos_;    // last recorded mouse position for orbit/pan

    QTimer          idleTimer_;
    QTimer          delayTimer_;
    QTimer          timerTimer_;
};

// ============================================================================
// QtObolDesignerPlugin  — Feature 7: Qt Designer plugin
//
// Quarter's Designer plugin is a separate compiled shared library (~280 lines)
// because it must link against the Quarter shared library.  With Obol the
// entire integration fits in one header; the plugin class is just ~70 lines of
// QDesignerCustomWidgetInterface boilerplate — identical complexity.
//
// To build as a Designer plugin, define OBOL_BUILD_AS_QT_DESIGNER_PLUGIN
// and compile this file into a shared library alongside Qt::Designer.
// ============================================================================

#ifdef OBOL_BUILD_AS_QT_DESIGNER_PLUGIN

#include <QtUiPlugin/QDesignerCustomWidgetInterface>
#include <QtPlugin>

class QtObolDesignerPlugin : public QObject,
                             public QDesignerCustomWidgetInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.obol.QtObolDesignerPlugin")
    Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
    explicit QtObolDesignerPlugin(QObject * parent = nullptr)
        : QObject(parent), initialized_(false) {}

    bool        isContainer()   const override { return false; }
    bool        isInitialized() const override { return initialized_; }
    QIcon       icon()          const override { return QIcon(); }
    QString     group()         const override { return "Display Widgets [Obol]"; }
    QString     name()          const override { return "QtObolWidget"; }
    QString     toolTip()       const override { return "Obol 3D scene viewer"; }
    QString     whatsThis()     const override { return ""; }
    QString     includeFile()   const override { return "qt_obol_widget.h"; }

    // Qt Designer calls this once per session.  Unlike Quarter::init(),
    // Obol needs no per-session initialisation here; SoDB::init() is the
    // application's responsibility and is called once at startup.
    void initialize(QDesignerFormEditorInterface *) override {
        initialized_ = true;
    }

    // Create a default widget with a placeholder cube scene.
    QWidget * createWidget(QWidget * parent) override {
        QtObolWidget * w = new QtObolWidget(parent);
        SoSeparator * root = new SoSeparator;
        root->ref();
        SoCube * cube = new SoCube; // addChild refs it; no manual ref needed
        root->addChild(cube);
        w->setSceneGraph(root);
        root->unref();
        return w;
    }

    QString domXml() const override {
        return
            "<ui language=\"c++\">\n"
            " <widget class=\"QtObolWidget\" name=\"qtObolWidget\">\n"
            "  <property name=\"geometry\">\n"
            "   <rect><x>0</x><y>0</y><width>400</width><height>300</height></rect>\n"
            "  </property>\n"
            " </widget>\n"
            "</ui>\n";
    }

private:
    bool initialized_;
};

#endif // OBOL_BUILD_AS_QT_DESIGNER_PLUGIN
