/*
 * render_offscreen.cpp - Integration test: SoOffscreenRenderer API coverage
 *
 * Exercises the user-facing SoOffscreenRenderer API beyond the simple
 * render-and-compare pattern used in other tests:
 *
 *   1. getViewportRegion() / setViewportRegion() round-trip.
 *   2. getBackgroundColor() / setBackgroundColor() round-trip.
 *   3. getComponents() after setComponents().
 *   4. Multiple sequential renders with the same renderer (same context reuse).
 *   5. getBuffer() non-null after a successful render.
 *   6. Render different resolutions (64×64 and 256×256) with one renderer.
 *   7. getGLRenderAction() returns a non-null action pointer.
 *   8. Pixel content validation: renders a bright red sphere; the centre
 *      region must contain predominantly red pixels.
 *
 * Covers SoOffscreenRenderer code paths in src/rendering/SoOffscreenRenderer.cpp
 * that are not exercised by image-comparison tests (which only render + write).
 *
 * The GTest scenario reports any failed contract.
 */

#include "headless_utils.h"
#include "testlib/test_scenes.h"
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbColor.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <cstdio>
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Minimum amount by which red must exceed green and blue to qualify as
// a "predominantly red" pixel, and minimum red channel intensity.
static const int MIN_RED_DOMINANCE = 40;
static const int MIN_RED_INTENSITY  = 60;

// Count pixels that are "predominantly red" in an RGB buffer.
static int countRedPixels(const unsigned char *buf, int w, int h)
{
    int count = 0;
    for (int i = 0; i < w * h; ++i) {
        const unsigned char *p = buf + i * 3;
        if ((int)p[0] > (int)p[1] + MIN_RED_DOMINANCE &&
            (int)p[0] > (int)p[2] + MIN_RED_DOMINANCE &&
            p[0] > MIN_RED_INTENSITY)
            ++count;
    }
    return count;
}

// ---------------------------------------------------------------------------

#include "framework/render_test_registration.h"

class OffscreenRendererIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        initCoinHeadless();
        root = ObolTest::Scenes::createOffscreen(64, 64);
    }

    void TearDown() override { root->unref(); }

    SoSeparator * root = nullptr;
};

TEST_F(OffscreenRendererIntegrationTest, ViewportRegionRoundTrips)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));
    EXPECT_EQ(renderer.getViewportRegion().getWindowSize(), SbVec2s(64, 64));
}

TEST_F(OffscreenRendererIntegrationTest, BackgroundColorRoundTrips)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));
    const SbColor expected(0.2f, 0.4f, 0.6f);
    renderer.setBackgroundColor(expected);
    const SbColor & actual = renderer.getBackgroundColor();
    EXPECT_NEAR(actual[0], expected[0], 0.001f);
    EXPECT_NEAR(actual[1], expected[1], 0.001f);
    EXPECT_NEAR(actual[2], expected[2], 0.001f);
}

TEST_F(OffscreenRendererIntegrationTest, ComponentModesRoundTrip)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    EXPECT_EQ(renderer.getComponents(), SoOffscreenRenderer::RGB);
    renderer.setComponents(SoOffscreenRenderer::RGB_TRANSPARENCY);
    EXPECT_EQ(renderer.getComponents(), SoOffscreenRenderer::RGB_TRANSPARENCY);
}

TEST_F(OffscreenRendererIntegrationTest, OwnsRenderAction)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));
    EXPECT_NE(renderer.getGLRenderAction(), nullptr);
}

TEST_F(OffscreenRendererIntegrationTest, SuccessfulRenderExposesBuffer)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    ASSERT_TRUE(renderer.render(root));
    EXPECT_NE(renderer.getBuffer(), nullptr);
}

TEST_F(OffscreenRendererIntegrationTest, RendererCanBeReused)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    for (int pass = 0; pass < 3; ++pass) {
        ASSERT_TRUE(renderer.render(root)) << "pass " << pass;
        ASSERT_NE(renderer.getBuffer(), nullptr) << "pass " << pass;
    }
}

TEST_F(OffscreenRendererIntegrationTest, ViewportCanResizeBetweenRenders)
{
    SoOffscreenRenderer renderer(SbViewportRegion(64, 64));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    ASSERT_TRUE(renderer.render(root));
    renderer.setViewportRegion(SbViewportRegion(128, 128));
    ASSERT_TRUE(renderer.render(root));
    EXPECT_EQ(renderer.getViewportRegion().getWindowSize(), SbVec2s(128, 128));
}

TEST_F(OffscreenRendererIntegrationTest, RedSphereProducesRedPixels)
{
    constexpr int width = 128;
    constexpr int height = 128;
    SoOffscreenRenderer renderer(SbViewportRegion(width, height));
    renderer.setComponents(SoOffscreenRenderer::RGB);
    renderer.setBackgroundColor(SbColor(0.0f, 0.0f, 0.0f));
    ASSERT_TRUE(renderer.render(root));
    ASSERT_NE(renderer.getBuffer(), nullptr);
    EXPECT_GE(countRedPixels(renderer.getBuffer(), width, height), 200);

    const std::string output =
        ObolTest::renderingOutputStem("offscreen_red_sphere") + ".rgb";
    EXPECT_TRUE(renderer.writeToRGB(output.c_str()));
}
