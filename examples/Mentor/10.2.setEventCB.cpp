/*
 *
 *  Copyright (C) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  Further, this software is distributed without any warranty that it is
 *  free of the rightful claim of any third person regarding infringement
 *  or the like.  Any license provided herein, whether implied or
 *  otherwise, applies only to this software file.  Patent licenses, if
 *  any, provided herein do not apply to combinations of this program with
 *  other software, or any other product whatsoever.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 *  Mountain View, CA  94043, or:
 *
 *  http://www.sgi.com
 *
 *  For further information regarding this notice, see:
 *
 *  http://oss.sgi.com/projects/GenInfo/NoticeExplan/
 *
 */

/*
 * Headless version of Inventor Mentor example 10.2
 *
 * Original: setEventCB - RenderArea event callback (Xt-specific)
 * Headless: Demonstrates generic event translation with Obol v2 scene state
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRotationAngle = kPi / 60.0f;

enum class Button {
    Left,
    Middle,
    Right
};

struct AppEvent {
    Button button = Button::Left;
    bool pressed = true;
    int x = 0;
    int y = 0;
};

struct AppState {
    obol::Scene scene;
    obol::PerspectiveCamera camera;
    obol::PointCloud points;
    obol::SceneObjectId pointObject = obol::InvalidSceneObjectId;
    bool rotating = false;
};

obol::Material material(float r, float g, float b, bool unlit = false)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.unlit = unlit;
    return result;
}

obol::Transform translation(float x, float y, float z)
{
    obol::Transform result;
    result.translation = {x, y, z};
    return result;
}

obol::Vec3 projectMouseToPlane(const obol::PerspectiveCamera & camera,
                               int mousex,
                               int mousey)
{
    const float ndcX =
        2.0f * static_cast<float>(mousex) / static_cast<float>(DEFAULT_WIDTH) - 1.0f;
    const float ndcY =
        1.0f - 2.0f * static_cast<float>(mousey) / static_cast<float>(DEFAULT_HEIGHT);
    const float distance = camera.position.z;
    const float halfHeight = std::tan(camera.verticalFieldOfViewRadians * 0.5f) * distance;
    const float halfWidth =
        halfHeight * static_cast<float>(DEFAULT_WIDTH) / static_cast<float>(DEFAULT_HEIGHT);
    return {ndcX * halfWidth, ndcY * halfHeight, 0.0f};
}

void rotateCamera(AppState & state)
{
    const float c = std::cos(kRotationAngle);
    const float s = std::sin(kRotationAngle);
    const obol::Vec3 pos = state.camera.position;
    state.camera.position = {
        pos.x * c + pos.z * s,
        pos.y,
        -pos.x * s + pos.z * c
    };
    state.scene.setCamera(state.camera);
}

bool handleEvent(AppState & state, const AppEvent & event)
{
    if (event.button == Button::Left && event.pressed) {
        printf("LEFT button pressed at (%d, %d) - adding point\n", event.x, event.y);
        state.points.points.push_back(projectMouseToPlane(state.camera, event.x, event.y));
        state.scene.setObjectPointCloud(state.pointObject, state.points);
        return true;
    }
    if (event.button == Button::Middle) {
        state.rotating = event.pressed;
        printf("%s - %s rotation\n",
               event.pressed ? "MIDDLE button pressed" : "MIDDLE button released",
               event.pressed ? "starting" : "stopping");
        return true;
    }
    if (event.button == Button::Right && event.pressed) {
        printf("RIGHT button pressed - clearing points\n");
        state.points.points.clear();
        state.scene.setObjectPointCloud(state.pointObject, state.points);
        return true;
    }
    return false;
}

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

} // namespace

int main(int argc, char **argv)
{
    printf("=== Mentor Example 10.2: RenderArea Event Callback ===\n");
    printf("This demonstrates toolkit-agnostic event translation pattern\n");
    printf("\nOriginal used Xt-specific XButtonEvent, XMotionEvent\n");
    printf("This version keeps events application-owned and updates Obol v2 state\n\n");

    initCoinHeadless();

    AppState app;
    app.camera.position = {0.0f, 0.0f, 4.0f};
    app.camera.target = {0.0f, 0.0f, 0.0f};
    app.camera.nearDistance = 1.0f;
    app.camera.farDistance = 7.0f;
    app.camera.verticalFieldOfViewRadians = kPi / 3.0f;
    app.scene.setCamera(app.camera);

    obol::PrimitiveOptions sphereOptions;
    sphereOptions.radius = 1.5f;
    app.scene.addPrimitive(obol::Primitive::Sphere,
                           material(0.4f, 0.6f, 0.8f, true),
                           translation(0.0f, 0.0f, -2.0f),
                           sphereOptions);

    app.points.pointSize = 6.0f;
    app.pointObject =
        app.scene.addPointCloud(app.points, material(1.0f, 1.0f, 0.0f));

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    printf("Setting event callback - events will go to app handler\n");
    printf("\n=== Simulating user interactions ===\n\n");

    const char *baseFilename = (argc > 1) ? argv[1] : "10.2.setEventCB";
    char filename[512];

    printf("--- State 1: Initial empty scene ---\n");
    snprintf(filename, sizeof(filename), "%s_initial.rgb", baseFilename);
    if (!renderScene(renderer, app.scene, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, app.scene, filename)) return 1;

    printf("\n--- Simulating LEFT button clicks to add points ---\n");
    static const int clickCoords[][2] = {
        {400, 300}, {250, 200}, {550, 200}, {250, 400}, {550, 400},
        {150, 300}, {650, 300}, {400, 150}, {400, 450},
        {300, 250}, {500, 250}, {300, 350}
    };
    static const int numClicks = 12;

    for (int i = 0; i < numClicks; i++) {
        handleEvent(app, AppEvent{Button::Left, true, clickCoords[i][0], clickCoords[i][1]});
    }

    printf("--- State 2: After adding %d points ---\n", numClicks);
    snprintf(filename, sizeof(filename), "%s_points.rgb", baseFilename);
    if (!renderScene(renderer, app.scene, filename)) return 1;

    printf("\n--- Simulating MIDDLE button for rotation ---\n");
    handleEvent(app, AppEvent{Button::Middle, true, 400, 300});

    printf("Processing application timer ticks for rotation...\n");
    for (int i = 0; i < 10; i++) {
        if (app.rotating) rotateCamera(app);
    }

    printf("--- State 3: After camera rotation ---\n");
    snprintf(filename, sizeof(filename), "%s_rotated.rgb", baseFilename);
    if (!renderScene(renderer, app.scene, filename)) return 1;

    handleEvent(app, AppEvent{Button::Middle, false, 400, 300});

    printf("\n--- Simulating RIGHT button to clear points ---\n");
    handleEvent(app, AppEvent{Button::Right, true, 400, 300});

    printf("--- State 4: After clearing points ---\n");
    snprintf(filename, sizeof(filename), "%s_cleared.rgb", baseFilename);
    if (!renderScene(renderer, app.scene, filename)) return 1;

    printf("\n=== Summary ===\n");
    printf("Generated 4 images showing event-driven interaction\n");
    printf("\nKey architectural insight:\n");
    printf("Event translation is a GENERIC pattern that works with ANY toolkit.\n");
    printf("\nToolkit responsibilities:\n");
    printf("  1. Capture native events (X11 XEvent, Win32 MSG, etc.)\n");
    printf("  2. Translate to app event data (position, button, state)\n");
    printf("  3. Send to application callback, then update Obol v2 scene state\n");
    printf("  4. Trigger redraw if event was handled\n");
    printf("\nObol v2 responsibilities:\n");
    printf("  - Maintain backend-neutral scene data\n");
    printf("  - Render updated scene state through the selected backend\n");
    printf("  - Avoid requiring backend event or node callback types\n");
    printf("\nThis exact pattern works with:\n");
    printf("  - X11/Xt: XEvent -> app event\n");
    printf("  - Qt: QMouseEvent -> app event\n");
    printf("  - FLTK: Fl_Event -> app event\n");
    printf("  - Win32: MSG -> app event\n");
    printf("  - Web: JavaScript Event -> app event\n");
    printf("  - Custom/mock: Generic struct -> app event\n");

    return 0;
}
