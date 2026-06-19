/*
 * Headless version of Inventor Mentor example 15.3
 *
 * Original: attach/detach different Inventor manipulators to selected nodes.
 * Headless: application selection state displays v2 interaction overlays.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <cstdio>

namespace {

enum class Manipulator {
    None,
    SphereHandleBox,
    CubeTrackball,
    ConeTransformBox
};

constexpr float kObjectSpacing = 2.5f;

struct ObjectIds {
    obol::SceneObjectId cube = obol::InvalidSceneObjectId;
    obol::SceneObjectId sphere = obol::InvalidSceneObjectId;
    obol::SceneObjectId cone = obol::InvalidSceneObjectId;
};

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

obol::PerspectiveCamera viewAllCamera(const obol::Scene & scene)
{
    obol::ViewAllRequest request;
    request.viewportWidth = DEFAULT_WIDTH;
    request.viewportHeight = DEFAULT_HEIGHT;
    return obol::CameraFraming::viewAllPerspective(scene, request);
}

ObjectIds addBaseObjects(obol::Scene & scene, Manipulator active)
{
    ObjectIds ids;
    const obol::Material neutral = material(0.8f, 0.8f, 0.8f);
    const obol::Material cubeMat =
        active == Manipulator::CubeTrackball ? material(0.2f, 1.0f, 0.2f) : neutral;
    const obol::Material sphereMat =
        active == Manipulator::SphereHandleBox ? material(1.0f, 0.2f, 0.2f) : neutral;
    const obol::Material coneMat =
        active == Manipulator::ConeTransformBox ? material(0.2f, 0.2f, 1.0f) : neutral;

    ids.cube = scene.addPrimitive(obol::Primitive::Cube,
                                  cubeMat,
                                  transform(-kObjectSpacing, 0.0f, 0.0f));

    obol::PrimitiveOptions sphereOptions;
    sphereOptions.radius = 1.0f;
    ids.sphere = scene.addPrimitive(obol::Primitive::Sphere,
                                    sphereMat,
                                    transform(0.0f, 0.0f, 0.0f),
                                    sphereOptions);

    obol::PrimitiveOptions coneOptions;
    coneOptions.radius = 1.0f;
    coneOptions.height = 2.0f;
    ids.cone = scene.addPrimitive(obol::Primitive::Cone,
                                  coneMat,
                                  transform(kObjectSpacing, 0.0f, 0.0f),
                                  coneOptions);
    return ids;
}

void addManipulatorOverlay(obol::Scene & scene,
                           Manipulator active,
                           const ObjectIds & ids)
{
    if (active == Manipulator::SphereHandleBox) {
        obol::ManipulatorOverlay overlay;
        overlay.target = ids.sphere;
        overlay.kind = obol::ManipulatorOverlayKind::HandleBox;
        overlay.transform = transform(0.0f, 0.0f, 0.0f);
        overlay.boxHalfSize = {1.25f, 1.25f, 1.25f};
        overlay.lineWidth = 2.0f;
        overlay.material = material(1.0f, 0.2f, 0.2f);
        obol::TransformDragger::addManipulatorOverlay(scene, overlay);
    } else if (active == Manipulator::CubeTrackball) {
        obol::ManipulatorOverlay overlay;
        overlay.target = ids.cube;
        overlay.kind = obol::ManipulatorOverlayKind::Trackball;
        overlay.transform = transform(-kObjectSpacing, 0.0f, 0.0f);
        overlay.trackballRadius = 1.35f;
        overlay.lineWidth = 2.0f;
        overlay.segments = 48;
        overlay.material = material(0.2f, 1.0f, 0.2f);
        obol::TransformDragger::addManipulatorOverlay(scene, overlay);
    } else if (active == Manipulator::ConeTransformBox) {
        obol::ManipulatorOverlay overlay;
        overlay.target = ids.cone;
        overlay.kind = obol::ManipulatorOverlayKind::TransformBox;
        overlay.transform = transform(kObjectSpacing, 0.0f, 0.0f);
        overlay.boxHalfSize = {1.25f, 1.25f, 1.25f};
        overlay.lineWidth = 2.0f;
        overlay.material = material(0.92f, 0.92f, 0.88f);
        obol::TransformDragger::addManipulatorOverlay(scene, overlay);
    }
}

obol::Scene makeScene(Manipulator active)
{
    obol::Scene scene;
    scene.addDirectionalLight(obol::DirectionalLight{});
    const ObjectIds ids = addBaseObjects(scene, active);
    scene.setCamera(viewAllCamera(scene));
    addManipulatorOverlay(scene, active, ids);
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
