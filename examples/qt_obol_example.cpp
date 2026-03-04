/* qt_obol_example.cpp — Qt/Obol example with render mode, stereo, context menu
 *
 * Demonstrates the full-featured QtObolWidget.  Shows a yellow cone and:
 *   - Right-click context menu with render mode / stereo / transparency submenu
 *   - Interaction mode: Alt-key cursor change (6 lines vs Quarter's 151)
 *   - viewAll via the context menu
 *
 * Build with:   cmake -DOBOL_BUILD_QT_EXAMPLE=ON ...
 *
 * Compare with Quarter's minimal.cpp:
 *
 *   Quarter:                           | Obol (this file):
 *   -----------------------------------+----------------------------------
 *   #include <Quarter/Quarter.h>       | (no separate init library)
 *   #include <Quarter/QuarterWidget.h> | #include "qt_obol_widget.h"
 *   Quarter::init();                   | (SoDB::init handles everything)
 *   QuarterWidget * viewer = new...    | QtObolWidget * viewer = new...
 *   viewer->setNavigationModeFile(...) | (not needed)
 *   viewer->setSceneGraph(root);       | viewer->setSceneGraph(root);
 *   // right-click menu automatic     | // right-click menu automatic
 *   app.exec();                        | app.exec();
 *   Quarter::clean();                  | SoDB::finish();
 */

#include <QApplication>
#include <QMainWindow>

#include <Inventor/SoDB.h>
#include <Inventor/nodekits/SoNodeKit.h>
#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbColor.h>

// Self-contained Qt/Obol integration header (no separate library needed).
#include "qt_obol_widget.h"

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);

    // ── 1. Initialise Obol ─────────────────────────────────────────────────
    // A QtObolContextManager is required by SoDB::init to suppress the
    // "Context manager is NULL" warning and to support SoOffscreenRenderer.
    // No share context is available before the widget is created, so nullptr
    // is passed here; the context manager will create its own GL contexts.
    QtObolContextManager ctxMgr;
    SoDB::init(&ctxMgr);
    SoNodeKit::init();
    SoInteraction::init();

    // ── 2. Build a simple scene graph ──────────────────────────────────────
    SoSeparator * root = new SoSeparator;
    root->ref();

    // Headlight-style directional light
    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    root->addChild(light);

    // Yellow cone (same as Quarter's minimal example)
    SoBaseColor * col = new SoBaseColor;
    col->rgb = SbColor(1.0f, 1.0f, 0.0f);
    root->addChild(col);

    root->addChild(new SoCone);

    // ── 3. Create the widget ───────────────────────────────────────────────
    QMainWindow * mainwin = new QMainWindow;
    mainwin->setWindowTitle("Qt/Obol minimal example");
    mainwin->resize(800, 600);

    QtObolWidget * viewer = new QtObolWidget(mainwin);
    mainwin->setCentralWidget(viewer);

    // Attach the scene (a camera is added automatically if none is present).
    viewer->setSceneGraph(root);

    // ── 4. Show and run ───────────────────────────────────────────────────
    mainwin->show();
    app.exec();

    // ── 5. Cleanup ────────────────────────────────────────────────────────
    root->unref();
    delete mainwin;
    SoDB::finish();

    return 0;
}
