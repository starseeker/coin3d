/*
 * obol_qt_viewer.cpp  –  Qt scene viewer for Obol (direct library use)
 *
 * Architecture
 * ────────────
 * The viewer links directly against the Coin shared library and uses Obol's
 * public API to build and render scenes into off-screen pixel buffers
 * displayed via Qt.
 *
 * No bridge libraries or dlopen are involved.  The OpenGL context is provided
 * by QtContextManager (qt_context_manager.h) which uses Qt's QOpenGLContext
 * and QOffscreenSurface for cross-platform, portable GL context creation
 * without requiring Xvfb or a direct X11 connection.
 *
 * Layout
 * ──────
 *  ┌──────┬─────────────────────────────────┐
 *  │Scene │                                 │
 *  │brows.│        Coin GL panel            │
 *  │      │                                 │
 *  ├──────┴─────────────────────────────────┤
 *  │[Reload]  [Save RGB...]    Scene: xxx   │
 *  └─────────────────────────────────────────┘
 *
 * Camera interaction
 *   Left-drag  → orbit (quaternion trackball – no gimbal lock)
 *   Right-drag → dolly
 *   Scroll     → zoom
 *
 * Building
 * ────────
 * Enabled by -DOBOL_BUILD_QT_VIEWER=ON at CMake configure time.
 * Qt6 (Widgets + Gui) is required.
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

/* ---- Unified test registry (scene factories) ---- */
#include "testlib/test_registry.h"

/* ---- Qt context manager ---- */
#include "qt_context_manager.h"

/* ---- Qt ---- */
#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QListWidget>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QStatusBar>
#include <QToolBar>
#include <QAction>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <functional>

/* =========================================================================
 * Scene catalogue
 * ======================================================================= */

static std::vector<const ObolTest::TestEntry*> getVisualTests()
{
    std::vector<const ObolTest::TestEntry*> out;
    for (const auto& e : ObolTest::TestRegistry::instance().allTests())
        if (e.has_visual && e.create_scene)
            out.push_back(&e);
    return out;
}

/* =========================================================================
 * Qt → SoKeyboardEvent::Key translation
 *
 * Qt key codes for printable ASCII characters (0x20–0x7e) match
 * SoKeyboardEvent::Key values exactly.  Special keys need mapping.
 * ======================================================================= */
static SoKeyboardEvent::Key qtKeyToSo(int qt_key)
{
    /* Printable ASCII: Qt and Coin share the same values */
    if (qt_key >= 0x20 && qt_key <= 0x7e)
        return static_cast<SoKeyboardEvent::Key>(qt_key);

    /* Special keys */
    switch (qt_key) {
    case Qt::Key_Backspace:  return SoKeyboardEvent::BACKSPACE;
    case Qt::Key_Tab:        return SoKeyboardEvent::TAB;
    case Qt::Key_Return:     return SoKeyboardEvent::RETURN;
    case Qt::Key_Enter:      return SoKeyboardEvent::RETURN;
    case Qt::Key_Escape:     return SoKeyboardEvent::ESCAPE;
    case Qt::Key_Delete:     return SoKeyboardEvent::KEY_DELETE;
    case Qt::Key_Home:       return SoKeyboardEvent::HOME;
    case Qt::Key_End:        return SoKeyboardEvent::END;
    case Qt::Key_PageUp:     return SoKeyboardEvent::PAGE_UP;
    case Qt::Key_PageDown:   return SoKeyboardEvent::PAGE_DOWN;
    case Qt::Key_Left:       return SoKeyboardEvent::LEFT_ARROW;
    case Qt::Key_Right:      return SoKeyboardEvent::RIGHT_ARROW;
    case Qt::Key_Up:         return SoKeyboardEvent::UP_ARROW;
    case Qt::Key_Down:       return SoKeyboardEvent::DOWN_ARROW;
    case Qt::Key_Shift:      return SoKeyboardEvent::LEFT_SHIFT;
    case Qt::Key_Control:    return SoKeyboardEvent::LEFT_CONTROL;
    case Qt::Key_Alt:        return SoKeyboardEvent::LEFT_ALT;
    case Qt::Key_F1:         return SoKeyboardEvent::F1;
    case Qt::Key_F2:         return SoKeyboardEvent::F2;
    case Qt::Key_F3:         return SoKeyboardEvent::F3;
    case Qt::Key_F4:         return SoKeyboardEvent::F4;
    case Qt::Key_F5:         return SoKeyboardEvent::F5;
    case Qt::Key_F6:         return SoKeyboardEvent::F6;
    case Qt::Key_F7:         return SoKeyboardEvent::F7;
    case Qt::Key_F8:         return SoKeyboardEvent::F8;
    case Qt::Key_F9:         return SoKeyboardEvent::F9;
    case Qt::Key_F10:        return SoKeyboardEvent::F10;
    case Qt::Key_F11:        return SoKeyboardEvent::F11;
    case Qt::Key_F12:        return SoKeyboardEvent::F12;
    default:                 return SoKeyboardEvent::ANY;
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
    bool dragger_active = false;
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

    void computeCenter() {
        if (!root) return;
        SoGetBoundingBoxAction bba(SbViewportRegion(width, height));
        bba.apply(root);
        SbBox3f bbox = bba.getBoundingBox();
        if (!bbox.isEmpty())
            scene_center = bbox.getCenter();
    }

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
 * CoinPanel  –  Qt widget that renders a Coin scene
 * ======================================================================= */
class CoinPanel : public QWidget {
public:
    std::unique_ptr<SceneState> state;
    std::string label_text;
    std::string status_text;

    std::vector<uint8_t> display_buf; /* RGB top-down */
    QPixmap              pixmap;

    std::function<void()> on_rendered;
    std::function<void(SoOffscreenRenderer*)> configure_renderer_fn;

    explicit CoinPanel(QWidget* parent = nullptr, const char* lbl = "")
        : QWidget(parent), label_text(lbl ? lbl : ""),
          gl_context_failed_(false)
    {
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMinimumSize(200, 150);
    }

    void loadScene(const char* name) {
        const ObolTest::TestEntry* entry =
            ObolTest::TestRegistry::instance().findTest(name);
        if (!entry || !entry->create_scene) {
            status_text = std::string("Unknown scene: ") + name;
            update(); return;
        }

        state.reset(new SceneState);
        state->width  = std::max(width(), 1);
        state->height = std::max(height(), 1);
        state->root   = entry->create_scene(state->width, state->height);
        state->cam    = state->root ? SceneState::findCamera(state->root) : nullptr;
        state->computeCenter();
        state->updateClipping();
        configure_renderer_fn = entry->configure_renderer;
        status_text.clear();
        gl_context_failed_ = false;
        refreshRender();
    }

    void clearScene() {
        state.reset();
        configure_renderer_fn = nullptr;
        pixmap = QPixmap();
        display_buf.clear();
        update();
    }

    void refreshRender() {
        if (!state || !state->root) { update(); return; }
        if (gl_context_failed_) { update(); return; }

        int pw = std::max(width(), 1);
        int ph = std::max(height(), 1);
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
        r->clearBackgroundGradient();
        if (configure_renderer_fn) configure_renderer_fn(r);
        if (!r->render(state->root)) {
            gl_context_failed_ = true;
            status_text = "System GL context unavailable"; update(); return;
        }
        const unsigned char* src = r->getBuffer();
        if (!src) { status_text = "No buffer"; update(); return; }

        /* convert bottom-up RGBA → top-down RGB for QImage */
        display_buf.resize((size_t)pw * ph * 3);
        for (int row = 0; row < ph; ++row) {
            const uint8_t* s = src + (size_t)(ph-1-row) * pw * 4;
            uint8_t*       d = display_buf.data() + (size_t)row * pw * 3;
            for (int col = 0; col < pw; ++col) {
                d[0]=s[0]; d[1]=s[1]; d[2]=s[2]; s+=4; d+=3;
            }
        }

        /* Build a QPixmap from the RGB data for display */
        QImage img(display_buf.data(), pw, ph, pw * 3, QImage::Format_RGB888);
        pixmap = QPixmap::fromImage(img);
        update();
        if (on_rendered) on_rendered();
    }

protected:
    /* ---- Qt overrides ---- */

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), Qt::black);
        if (!pixmap.isNull()) {
            p.drawPixmap(0, 0, width(), height(), pixmap);
        } else {
            p.setPen(Qt::white);
            p.setFont(QFont("Helvetica", 14));
            p.drawText(rect(), Qt::AlignCenter, "(no scene)");
        }
        if (!label_text.empty()) {
            p.setPen(QColor(255, 255, 100));
            p.setFont(QFont("Helvetica", 12, QFont::Bold));
            p.drawText(6, 18, QString::fromStdString(label_text));
        }
        if (!status_text.empty()) {
            p.setPen(QColor(255, 80, 80));
            p.setFont(QFont("Helvetica", 12));
            p.drawText(6, height() - 6, QString::fromStdString(status_text));
        }
    }

    void mousePressEvent(QMouseEvent* ev) override {
        setFocus();
        if (!state || !state->root) return;
        state->dragging       = true;
        state->drag_btn       = ev->button() == Qt::LeftButton ? 1 :
                                ev->button() == Qt::MiddleButton ? 2 : 3;
        state->last_x = ev->position().x();
        state->last_y = ev->position().y();

        SoMouseButtonEvent soEv;
        soEv.setButton(state->drag_btn == 1 ? SoMouseButtonEvent::BUTTON1 :
                       state->drag_btn == 2 ? SoMouseButtonEvent::BUTTON2 :
                                              SoMouseButtonEvent::BUTTON3);
        soEv.setState(SoButtonEvent::DOWN);
        soEv.setPosition(SbVec2s((short)ev->position().x(),
                                  (short)(height() - ev->position().y())));
        soEv.setTime(SbTime::getTimeOfDay());
        SbViewportRegion vp(state->width, state->height);
        SoHandleEventAction ha(vp);
        ha.setEvent(&soEv);
        ha.apply(state->root);
        state->dragger_active = ha.isHandled();
    }

    void mouseReleaseEvent(QMouseEvent* ev) override {
        if (!state || !state->root) return;
        state->dragging       = false;
        state->dragger_active = false;

        SoMouseButtonEvent soEv;
        soEv.setButton(ev->button() == Qt::LeftButton  ? SoMouseButtonEvent::BUTTON1 :
                       ev->button() == Qt::MiddleButton ? SoMouseButtonEvent::BUTTON2 :
                                                          SoMouseButtonEvent::BUTTON3);
        soEv.setState(SoButtonEvent::UP);
        soEv.setPosition(SbVec2s((short)ev->position().x(),
                                  (short)(height() - ev->position().y())));
        soEv.setTime(SbTime::getTimeOfDay());
        SbViewportRegion vp(state->width, state->height);
        SoHandleEventAction ha(vp);
        ha.setEvent(&soEv);
        ha.apply(state->root);
        refreshRender();
    }

    void mouseMoveEvent(QMouseEvent* ev) override {
        if (!state || !state->dragging || !state->cam) return;
        int ex = (int)ev->position().x();
        int ey = (int)ev->position().y();
        int dx = ex - state->last_x;
        int dy = ey - state->last_y;
        state->last_x = ex;
        state->last_y = ey;

        SoLocation2Event soEv;
        soEv.setPosition(SbVec2s((short)ex, (short)(height() - ey)));
        soEv.setTime(SbTime::getTimeOfDay());
        SbViewportRegion vp(state->width, state->height);
        SoHandleEventAction ha(vp);
        ha.setEvent(&soEv);
        ha.apply(state->root);

        if (!state->dragger_active) {
            if (state->drag_btn == 1) {
                /* orbit – camera-local yaw and pitch (BRL-CAD style) */
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
        refreshRender();
    }

    void wheelEvent(QWheelEvent* ev) override {
        if (!state || !state->cam) return;
        float delta = (float)ev->angleDelta().y() / 120.0f;
        float dist = state->cam->focalDistance.getValue();
        dist *= (1.0f - delta * 0.1f);
        if (dist < 0.1f) dist = 0.1f;
        SbVec3f dir = state->cam->position.getValue() - state->scene_center;
        dir.normalize();
        state->cam->position.setValue(state->scene_center + dir * dist);
        state->cam->focalDistance.setValue(dist);
        state->updateClipping();
        refreshRender();
    }

    void keyPressEvent(QKeyEvent* ev) override {
        if (!state || !state->root) return;
        SoKeyboardEvent soEv;
        soEv.setKey(qtKeyToSo(ev->key()));
        soEv.setState(SoButtonEvent::DOWN);
        soEv.setTime(SbTime::getTimeOfDay());
        SbViewportRegion vp(state->width, state->height);
        SoHandleEventAction ha(vp);
        ha.setEvent(&soEv);
        ha.apply(state->root);
        refreshRender();
    }

    void keyReleaseEvent(QKeyEvent* ev) override {
        if (!state || !state->root) return;
        SoKeyboardEvent soEv;
        soEv.setKey(qtKeyToSo(ev->key()));
        soEv.setState(SoButtonEvent::UP);
        soEv.setTime(SbTime::getTimeOfDay());
        SbViewportRegion vp(state->width, state->height);
        SoHandleEventAction ha(vp);
        ha.setEvent(&soEv);
        ha.apply(state->root);
    }

    void resizeEvent(QResizeEvent*) override {
        if (state) { state->width = width(); state->height = height(); }
        refreshRender();
    }

private:
    bool gl_context_failed_;
};

/* =========================================================================
 * ObolQtViewerWindow  –  main Qt window
 * ======================================================================= */
class ObolQtViewerWindow : public QMainWindow {
public:
    explicit ObolQtViewerWindow(QWidget* parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("Obol Qt Scene Viewer");
        resize(1100, 700);
        buildUI();
    }

    void loadScene(const char* name) {
        coinPanel_->loadScene(name);
        statusLabel_->setText(QString("Scene: %1").arg(name));
    }

private:
    QListWidget* sceneList_   = nullptr;
    CoinPanel*   coinPanel_   = nullptr;
    QLabel*      statusLabel_ = nullptr;

    void buildUI() {
        /* Central splitter: scene browser | render panel */
        QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setHandleWidth(4);

        /* Scene browser */
        sceneList_ = new QListWidget(splitter);
        sceneList_->setMaximumWidth(240);
        sceneList_->setMinimumWidth(120);
        sceneList_->setTextElideMode(Qt::ElideRight);
        sceneList_->setFont(QFont("Monospace", 10));
        {
            auto tests = getVisualTests();
            for (const auto* e : tests) {
                std::string label =
                    ObolTest::categoryToString(e->category) + "/" + e->name;
                sceneList_->addItem(QString::fromStdString(label));
            }
        }

        /* Render panel */
        coinPanel_ = new CoinPanel(splitter, "Qt GL");
        splitter->addWidget(sceneList_);
        splitter->addWidget(coinPanel_);
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 1);

        setCentralWidget(splitter);

        /* Toolbar */
        QToolBar* toolbar = addToolBar("Tools");
        toolbar->setMovable(false);

        QPushButton* reloadBtn = new QPushButton("Reload");
        reloadBtn->setFixedHeight(26);
        QPushButton* saveBtn = new QPushButton("Save RGB...");
        saveBtn->setFixedHeight(26);
        statusLabel_ = new QLabel("Ready");
        statusLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        toolbar->addWidget(reloadBtn);
        toolbar->addWidget(saveBtn);
        toolbar->addSeparator();
        toolbar->addWidget(statusLabel_);

        /* Scene list selection */
        connect(sceneList_, &QListWidget::currentRowChanged,
                [this](int row) {
                    if (row < 0) return;
                    QString item = sceneList_->item(row)->text();
                    int slash = item.lastIndexOf('/');
                    QString name = slash >= 0 ? item.mid(slash + 1) : item;
                    loadScene(name.toUtf8().constData());
                });

        /* Reload button */
        connect(reloadBtn, &QPushButton::clicked, [this]() {
            int row = sceneList_->currentRow();
            if (row < 0) return;
            QString item = sceneList_->item(row)->text();
            int slash = item.lastIndexOf('/');
            QString name = slash >= 0 ? item.mid(slash + 1) : item;
            loadScene(name.toUtf8().constData());
        });

        /* Save button */
        connect(saveBtn, &QPushButton::clicked, [this]() { onSave(); });
    }

    void onSave() {
        if (!coinPanel_->state || !coinPanel_->state->root) {
            QMessageBox::information(this, "Obol Viewer", "No scene loaded.");
            return;
        }
        QString path = QFileDialog::getSaveFileName(
            this, "Save RGB", "scene.rgb", "SGI RGB (*.rgb)");
        if (path.isEmpty()) return;

        int pw = coinPanel_->width(), ph = coinPanel_->height();
        SoOffscreenRenderer* r = getRenderer(pw, ph);
        SbViewportRegion vp(pw, ph);
        r->setViewportRegion(vp);
        r->setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
        r->setBackgroundColor(SbColor(0.15f, 0.15f, 0.2f));
        r->clearBackgroundGradient();
        if (coinPanel_->configure_renderer_fn)
            coinPanel_->configure_renderer_fn(r);
        if (!r->render(coinPanel_->state->root)) {
            QMessageBox::warning(this, "Obol Viewer", "Render failed.");
            return;
        }
        /* Ensure .rgb extension */
        if (!path.endsWith(".rgb", Qt::CaseInsensitive)) {
            if (path.endsWith(".png", Qt::CaseInsensitive))
                path = path.left(path.length() - 4) + ".rgb";
            else
                path += ".rgb";
        }
        if (r->writeToRGB(path.toUtf8().constData()))
            QMessageBox::information(this, "Obol Viewer",
                                     QString("Saved to:\n%1").arg(path));
        else
            QMessageBox::warning(this, "Obol Viewer", "Failed to write file.");
    }
};

/* =========================================================================
 * main()
 * ======================================================================= */
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("Obol Qt Viewer");

    /* Initialise Obol using the Qt context manager.
     * QApplication must exist before initCoinHeadless() so that Qt's
     * platform plugin is loaded and QOffscreenSurface can be created. */
    initCoinHeadless();

    ObolQtViewerWindow win;
    win.show();

    /* Load the first visual scene automatically */
    {
        auto tests = getVisualTests();
        if (!tests.empty())
            win.loadScene(tests[0]->name.c_str());
    }

    return app.exec();
}
