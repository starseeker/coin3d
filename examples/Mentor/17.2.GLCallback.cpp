/*
 * Headless version of Inventor Mentor example 17.2
 *
 * Original: custom OpenGL callback draws a floor under Inventor geometry.
 * Headless: v2 backend-native OpenGL callback draws the floor.
 */

#include "headless_utils.h"
#include <Obol/Obol.h>

#include <Inventor/gl.h>

#include <cmath>
#include <cstdio>

namespace {

float floorObj[81][3];

obol::Material material(float r, float g, float b)
{
    obol::Material result;
    result.baseColor = {r, g, b, 1.0f};
    return result;
}

obol::Transform transform(float x, float y, float z)
{
    obol::Transform xf;
    xf.translation = {x, y, z};
    return xf;
}

void buildFloor()
{
    int a = 0;
    for (float i = -5.0f; i <= 5.0f; i += 1.25f) {
        for (float j = -5.0f; j <= 5.0f; j += 1.25f, ++a) {
            floorObj[a][0] = j;
            floorObj[a][1] = 0.0f;
            floorObj[a][2] = i;
        }
    }
}

void drawFloor()
{
    int i;

    glBegin(GL_LINES);
    for (i = 0; i < 4; ++i) {
        glVertex3fv(floorObj[i * 18]);
        glVertex3fv(floorObj[(i * 18) + 8]);
        glVertex3fv(floorObj[(i * 18) + 17]);
        glVertex3fv(floorObj[(i * 18) + 9]);
    }
    glVertex3fv(floorObj[i * 18]);
    glVertex3fv(floorObj[(i * 18) + 8]);
    glEnd();

    glBegin(GL_LINES);
    for (i = 0; i < 4; ++i) {
        glVertex3fv(floorObj[i * 2]);
        glVertex3fv(floorObj[(i * 2) + 72]);
        glVertex3fv(floorObj[(i * 2) + 73]);
        glVertex3fv(floorObj[(i * 2) + 1]);
    }
    glVertex3fv(floorObj[i * 2]);
    glVertex3fv(floorObj[(i * 2) + 72]);
    glEnd();
}

void drawOpenGLFloor(void *, const obol::OpenGLCallbackContext &)
{
    glPushMatrix();
    glTranslatef(0.0f, -3.0f, 0.0f);
    glColor3f(0.0f, 0.7f, 0.0f);
    glLineWidth(2.0f);
    glDisable(GL_LIGHTING);
    drawFloor();
    glEnable(GL_LIGHTING);
    glLineWidth(1.0f);
    glPopMatrix();
}

obol::Scene makeScene(const obol::PerspectiveCamera & camera)
{
    obol::Scene scene;
    scene.setCamera(camera);
    scene.addDirectionalLight(obol::DirectionalLight{});

    obol::OpenGLCallback floorCallback;
    floorCallback.draw = drawOpenGLFloor;
    floorCallback.label = "floor-grid";
    scene.addOpenGLCallback(floorCallback);

    scene.addPrimitive(obol::Primitive::Cube,
                       material(1.0f, 0.0f, 0.0f),
                       transform(-2.0f, -2.0f, 0.0f));

    obol::PrimitiveOptions sphere;
    sphere.radius = 1.0f;
    scene.addPrimitive(obol::Primitive::Sphere,
                       material(0.0f, 0.0f, 1.0f),
                       transform(2.0f, -2.0f, 0.0f),
                       sphere);

    return scene;
}

bool renderView(obol::Renderer & renderer,
                const obol::PerspectiveCamera & camera,
                const obol::RenderTarget & target,
                const char * filename)
{
    obol::Scene scene = makeScene(camera);
    obol::FrameRequest request;
    request.scene = &scene;
    request.target = target;
    request.background = {0.8f, 0.8f, 0.8f, 1.0f};
    const obol::FrameResult result = renderer.render(request);
    return result.success && renderer.writeRGB(filename);
}

obol::Vec3 add(const obol::Vec3 & lhs, const obol::Vec3 & rhs)
{
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

obol::Vec3 scaled(const obol::Vec3 & value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

obol::Vec3 rotatedAroundAxis(const obol::Vec3 & value,
                             const obol::Vec3 & axis,
                             float radians)
{
    const float axisLength =
        std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
    if (axisLength <= 0.0f || radians == 0.0f) return value;

    const obol::Vec3 unitAxis = {axis.x / axisLength,
                                axis.y / axisLength,
                                axis.z / axisLength};
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float dot =
        unitAxis.x * value.x + unitAxis.y * value.y + unitAxis.z * value.z;
    const obol::Vec3 cross = {
        unitAxis.y * value.z - unitAxis.z * value.y,
        unitAxis.z * value.x - unitAxis.x * value.z,
        unitAxis.x * value.y - unitAxis.y * value.x,
    };

    return add(add(scaled(value, cosine), scaled(cross, sine)),
               scaled(unitAxis, dot * (1.0f - cosine)));
}

obol::PerspectiveCamera cameraWithOrientation(const obol::Vec3 & position,
                                              const obol::Vec3 & axis,
                                              float angleRadians)
{
    const obol::Vec3 forward =
        rotatedAroundAxis({0.0f, 0.0f, -1.0f}, axis, angleRadians);
    const obol::Vec3 up =
        rotatedAroundAxis({0.0f, 1.0f, 0.0f}, axis, angleRadians);

    obol::PerspectiveCamera camera;
    camera.position = position;
    camera.target = add(position, scaled(forward, 5.0f));
    camera.up = up;
    camera.verticalFieldOfViewRadians = 1.57079632679f;
    camera.nearDistance = 2.0f;
    camera.farDistance = 12.0f;
    return camera;
}

} // namespace

int main(int argc, char **argv)
{
    initCoinHeadless();
    buildFloor();

    obol::ContextManagerBackend backend(getCoinHeadlessContextManager(),
                                        obol::RenderBackendKind::OpenGL2SWRast,
                                        "headless-context");
    obol::RenderTarget target;
    target.width = DEFAULT_WIDTH;
    target.height = DEFAULT_HEIGHT;
    target.pixelFormat = obol::PixelFormat::RGB;
    obol::Renderer renderer(backend);

    const char *baseFilename = (argc > 1) ? argv[1] : "17.2.GLCallback";
    char filename[512];

    printf("Rendering scene with v2 backend-native OpenGL callback for floor...\n");

    obol::PerspectiveCamera camera =
        cameraWithOrientation({0.0f, 0.0f, 5.0f},
                              {0.0f, 1.0f, 0.0f},
                              0.0f);
    snprintf(filename, sizeof(filename), "%s_00_default.rgb", baseFilename);
    if (!renderView(renderer, camera, target, filename)) return 1;
    snprintf(filename, sizeof(filename), "%s.rgb", baseFilename);
    if (!renderView(renderer, camera, target, filename)) return 1;

    camera = cameraWithOrientation({-3.0f, 2.0f, 5.0f},
                                   {0.0f, 1.0f, 0.0f},
                                   0.3f);
    snprintf(filename, sizeof(filename), "%s_01_angle1.rgb", baseFilename);
    if (!renderView(renderer, camera, target, filename)) return 1;

    camera = cameraWithOrientation({3.0f, 2.0f, 5.0f},
                                   {0.0f, 1.0f, 0.0f},
                                   -0.3f);
    snprintf(filename, sizeof(filename), "%s_02_angle2.rgb", baseFilename);
    if (!renderView(renderer, camera, target, filename)) return 1;

    camera = cameraWithOrientation({0.0f, 4.0f, 5.0f},
                                   {1.0f, 0.0f, 0.0f},
                                   -0.4f);
    snprintf(filename, sizeof(filename), "%s_03_top.rgb", baseFilename);
    if (!renderView(renderer, camera, target, filename)) return 1;

    printf("Done! Rendered 4 views showing OpenGL callback integration.\n");
    return 0;
}
