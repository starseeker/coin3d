/*
 * Headless version of Inventor Mentor example 13.1
 *
 * Original: GlobalFlds - digital clock using realTime global field
 * Headless: app-owned global field values update v2 Text3D content
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>
#include <string>

namespace {

bool renderScene(obol::OffscreenRenderer & renderer,
                 obol::Scene & scene,
                 const char * filename)
{
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
}

obol::Scene makeScene(const char * text)
{
    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 0.0f, 8.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.65f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::Material material;
    material.baseColor = {1.0f, 0.0f, 0.0f, 1.0f};

    obol::Text3D label;
    label.text = text;
    label.fontSize = 0.35f;
    label.justification = obol::TextJustification::Center;
    scene.addText3D(label, material);
    return scene;
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::OffscreenRenderer renderer(backend, target);
    renderer.setBackgroundColor({0.0f, 0.0f, 0.0f, 1.0f});

    const char *baseFilename = (argc > 1) ? argv[1] : "13.1.GlobalFlds";
    char filename[256];

    const obol::Time values[] = {
        obol::Time::fromUnixSeconds(946684800.0),
        obol::Time::fromUnixSeconds(946688461.0),
        obol::Time::fromUnixSeconds(946692122.0)
    };
    const char *suffixes[] = {"time1", "time2", "time3"};

    for (int i = 0; i < 3; ++i) {
        const std::string text = values[i].formatUTC();
        obol::Scene scene = makeScene(text.c_str());
        printf("Reference realTime value %d: %s\n", i + 1, text.c_str());
        snprintf(filename, sizeof(filename), "%s_%s.rgb", baseFilename, suffixes[i]);
        if (!renderScene(renderer, scene, filename)) return 1;
    }

    return 0;
}
