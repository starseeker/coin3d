#include <gtest/gtest.h>

#include <Obol/render/SoNanoRTContextManager.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSceneRendererParams.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTranslation.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>

namespace {

constexpr int width = 128;
constexpr int height = 96;

struct SceneDeleter {
    void operator()(SoSeparator * root) const
    {
        if (root) root->unref();
    }
};

using ScenePtr = std::unique_ptr<SoSeparator, SceneDeleter>;

void addShape(SoSeparator * root, SoShape * shape, float x,
              const SbColor & color)
{
    SoSeparator * branch = new SoSeparator;
    SoTranslation * translation = new SoTranslation;
    translation->translation.setValue(x, 0.0f, 0.0f);
    branch->addChild(translation);

    SoMaterial * material = new SoMaterial;
    material->diffuseColor.setValue(color);
    material->specularColor.setValue(0.6f, 0.6f, 0.6f);
    material->shininess.setValue(0.65f);
    branch->addChild(material);
    branch->addChild(shape);
    root->addChild(branch);
}

ScenePtr createNanoRTScene()
{
    SoSeparator * root = new SoSeparator;
    root->ref();

    SoPerspectiveCamera * camera = new SoPerspectiveCamera;
    camera->position.setValue(0.0f, 1.5f, 11.0f);
    root->addChild(camera);

    SoDirectionalLight * directional = new SoDirectionalLight;
    directional->direction.setValue(-0.5f, -0.8f, -1.0f);
    directional->intensity.setValue(0.8f);
    root->addChild(directional);

    SoPointLight * point = new SoPointLight;
    point->location.setValue(0.0f, 4.0f, 4.0f);
    point->color.setValue(0.7f, 0.8f, 1.0f);
    point->intensity.setValue(0.7f);
    root->addChild(point);

    SoSceneRendererParams * parameters = new SoSceneRendererParams;
    parameters->shadowsEnabled.setValue(TRUE);
    parameters->maxReflectionBounces.setValue(1);
    parameters->samplesPerPixel.setValue(2);
    parameters->ambientIntensity.setValue(0.2f);
    root->addChild(parameters);

    addShape(root, new SoSphere, -3.0f, SbColor(0.85f, 0.2f, 0.15f));
    addShape(root, new SoCube, -1.0f, SbColor(0.15f, 0.75f, 0.25f));
    addShape(root, new SoCone, 1.0f, SbColor(0.2f, 0.35f, 0.9f));
    addShape(root, new SoCylinder, 3.0f, SbColor(0.9f, 0.7f, 0.15f));

    camera->viewAll(root, SbViewportRegion(width, height));
    return ScenePtr(root);
}

unsigned char gradientChannel(float bottom, float top, int row)
{
    const float t = static_cast<float>(row) / static_cast<float>(height - 1);
    return static_cast<unsigned char>(
        (bottom * (1.0f - t) + top * t) * 255.0f);
}

TEST(NanoRTRendering, RendersCollectedSceneThroughPublicContextManager)
{
    ScenePtr scene = createNanoRTScene();
    SoNanoRTContextManager manager;
    SoOffscreenRenderer renderer(&manager, SbViewportRegion(width, height));
    renderer.setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);

    const SbColor bottom(0.02f, 0.03f, 0.06f);
    const SbColor top(0.18f, 0.25f, 0.4f);
    renderer.setBackgroundGradient(bottom, top);
    ASSERT_TRUE(renderer.render(scene.get()));

    const unsigned char * pixels = renderer.getBuffer();
    ASSERT_NE(pixels, nullptr);

    std::size_t geometryPixels = 0;
    bool sawOpaquePixel = false;
    for (int row = 0; row < height; ++row) {
        const unsigned char expected[3] = {
            gradientChannel(bottom[0], top[0], row),
            gradientChannel(bottom[1], top[1], row),
            gradientChannel(bottom[2], top[2], row)
        };
        for (int column = 0; column < width; ++column) {
            const std::size_t offset =
                static_cast<std::size_t>(row * width + column) * 4u;
            const int redDifference =
                std::abs(static_cast<int>(pixels[offset]) - expected[0]);
            const int greenDifference =
                std::abs(static_cast<int>(pixels[offset + 1]) - expected[1]);
            const int blueDifference =
                std::abs(static_cast<int>(pixels[offset + 2]) - expected[2]);
            if (std::max({redDifference, greenDifference, blueDifference}) > 5) {
                ++geometryPixels;
            }
            sawOpaquePixel = sawOpaquePixel || pixels[offset + 3] == 255;
            EXPECT_EQ(pixels[offset + 3], 255);
        }
    }

    EXPECT_GT(geometryPixels, static_cast<std::size_t>(width * height / 100));
    EXPECT_TRUE(sawOpaquePixel);
}

} // namespace
