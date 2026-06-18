/*
 * Headless version of Inventor Mentor example 15.3
 *
 * Original: attach/detach different Inventor manipulators to selected nodes.
 * Headless: application selection state displays v2 manipulator overlays.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cmath>
#include <cstdio>

namespace {

enum class Manipulator {
    None,
    SphereHandleBox,
    CubeTrackball,
    ConeTransformBox
};

constexpr float kPi = 3.14159265358979323846f;
constexpr float kObjectSpacing = 3.2f;

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    result.specular = {0.25f, 0.25f, 0.25f, 1.0f};
    result.shininess = 0.35f;
    return result;
}

obol::Transform transform(float x, float y, float z)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    return xf;
}

void addWireBox(obol::Scene & scene,
                const obol::Vec3 & center,
                const obol::Vec3 & halfSize,
                const obol::Material & mat)
{
    obol::Polyline box;
    box.lineWidth = 2.0f;
    const float x = halfSize.x;
    const float y = halfSize.y;
    const float z = halfSize.z;
    box.points = {
        {-x, -y, -z}, { x, -y, -z}, { x,  y, -z}, {-x,  y, -z},
        {-x, -y, -z}, {-x, -y,  z}, { x, -y,  z}, { x,  y,  z},
        {-x,  y,  z}, {-x, -y,  z}, { x, -y,  z}, { x, -y, -z},
        { x,  y, -z}, { x,  y,  z}, {-x,  y,  z}, {-x,  y, -z}
    };
    scene.addPolyline(box, mat, transform(center.x, center.y, center.z));
}

void addCircle(obol::Scene & scene,
               const obol::Vec3 & center,
               float radius,
               const obol::Vec3 & axis,
               const obol::Material & mat)
{
    obol::Polyline circle;
    circle.lineWidth = 2.0f;
    for (int i = 0; i <= 48; ++i) {
        const float angle = 2.0f * kPi * static_cast<float>(i) / 48.0f;
        if (axis.x != 0.0f) {
            circle.points.push_back({0.0f, radius * std::cos(angle), radius * std::sin(angle)});
        } else if (axis.y != 0.0f) {
            circle.points.push_back({radius * std::cos(angle), 0.0f, radius * std::sin(angle)});
        } else {
            circle.points.push_back({radius * std::cos(angle), radius * std::sin(angle), 0.0f});
        }
    }
    scene.addPolyline(circle, mat, transform(center.x, center.y, center.z));
}

void addBaseObjects(obol::Scene & scene, Manipulator active)
{
    const obol::Material neutral = material(0.8f, 0.8f, 0.8f);
    const obol::Material cubeMat =
        active == Manipulator::CubeTrackball ? material(0.2f, 1.0f, 0.2f) : neutral;
    const obol::Material sphereMat =
        active == Manipulator::SphereHandleBox ? material(1.0f, 0.2f, 0.2f) : neutral;
    const obol::Material coneMat =
        active == Manipulator::ConeTransformBox ? material(0.2f, 0.2f, 1.0f) : neutral;

    scene.addPrimitive(obol::Primitive::Cube, cubeMat, transform(-kObjectSpacing, 0.0f, 0.0f));

    obol::PrimitiveOptions sphereOptions;
    sphereOptions.radius = 1.0f;
    scene.addPrimitive(obol::Primitive::Sphere,
                       sphereMat,
                       transform(0.0f, 0.0f, 0.0f),
                       sphereOptions);

    obol::PrimitiveOptions coneOptions;
    coneOptions.radius = 1.0f;
    coneOptions.height = 2.0f;
    scene.addPrimitive(obol::Primitive::Cone,
                       coneMat,
                       transform(kObjectSpacing, 0.0f, 0.0f),
                       coneOptions);
}

void addManipulatorOverlay(obol::Scene & scene, Manipulator active)
{
    if (active == Manipulator::SphereHandleBox) {
        addWireBox(scene,
                   {0.0f, 0.0f, 0.0f},
                   {1.25f, 1.25f, 1.25f},
                   material(1.0f, 0.2f, 0.2f));
    } else if (active == Manipulator::CubeTrackball) {
        const obol::Material mat = material(0.2f, 1.0f, 0.2f);
        addCircle(scene, {-kObjectSpacing, 0.0f, 0.0f}, 1.35f, {1.0f, 0.0f, 0.0f}, mat);
        addCircle(scene, {-kObjectSpacing, 0.0f, 0.0f}, 1.35f, {0.0f, 1.0f, 0.0f}, mat);
        addCircle(scene, {-kObjectSpacing, 0.0f, 0.0f}, 1.35f, {0.0f, 0.0f, 1.0f}, mat);
    } else if (active == Manipulator::ConeTransformBox) {
        addWireBox(scene,
                   {kObjectSpacing, 0.0f, 0.0f},
                   {1.25f, 1.25f, 1.25f},
                   material(0.2f, 0.2f, 1.0f));
        addCircle(scene, {kObjectSpacing, 0.0f, 0.0f}, 1.55f, {0.0f, 0.0f, 1.0f}, material(0.2f, 0.2f, 1.0f));
    }
}

obol::Scene makeScene(Manipulator active)
{
    obol::Scene scene;
    obol::PerspectiveCamera camera;
    camera.position = {0.0f, 1.2f, 16.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.verticalFieldOfViewRadians = 0.55f;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});
    addBaseObjects(scene, active);
    addManipulatorOverlay(scene, active);
    return scene;
}

bool renderScene(obol::OffscreenRenderer & renderer,
                 Manipulator active,
                 const char * filename)
{
    obol::Scene scene = makeScene(active);
    const obol::FrameResult result = renderer.render(scene);
    return result.success && renderer.writeRGB(filename);
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

    const char *baseFilename = (argc > 1) ? argv[1] : "15.3.AttachManip";
    char filename[512];

    printf("\n=== Manipulator Attachment Demo [Obol v2] ===\n");

    snprintf(filename, sizeof(filename), "%s_frame00_initial.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::None, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::None, filename)) return 1;

    printf("Frame 1: Attaching HandleBox overlay to sphere\n");
    snprintf(filename, sizeof(filename), "%s_frame01_sphere_handlebox.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::SphereHandleBox, filename)) return 1;

    printf("Frame 2: Detaching manipulator from sphere\n");
    snprintf(filename, sizeof(filename), "%s_frame02_sphere_detached.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::None, filename)) return 1;

    printf("Frame 3: Attaching Trackball overlay to cube\n");
    snprintf(filename, sizeof(filename), "%s_frame03_cube_trackball.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::CubeTrackball, filename)) return 1;

    printf("Frame 4: Detaching manipulator from cube\n");
    snprintf(filename, sizeof(filename), "%s_frame04_cube_detached.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::None, filename)) return 1;

    printf("Frame 5: Attaching TransformBox overlay to cone\n");
    snprintf(filename, sizeof(filename), "%s_frame05_cone_transformbox.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::ConeTransformBox, filename)) return 1;

    printf("Frame 6: Detaching manipulator from cone\n");
    snprintf(filename, sizeof(filename), "%s_frame06_cone_detached.rgb", baseFilename);
    if (!renderScene(renderer, Manipulator::None, filename)) return 1;

    printf("Rendered 7 frames showing manipulator attachment/detachment.\n");
    return 0;
}
