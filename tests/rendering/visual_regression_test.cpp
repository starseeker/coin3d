#include "framework/image_assertions.h"
#include "framework/render_fixture.h"
#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SoQuadViewport.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoLOD.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSphere.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#ifndef OBOL_CONTROL_IMAGES_DIR
#error OBOL_CONTROL_IMAGES_DIR must identify the retained rendering references
#endif

namespace {

using SceneFactory = SoSeparator * (*)(int, int);

enum class RenderKind {
    Scene,
    QuadViewportLod
};

struct GoldenScene {
    const char * name;
    SceneFactory factory;
    const char * filename;
    int width;
    int height;
    SbColor background = SbColor(0.0f, 0.0f, 0.0f);
    bool gradient = false;
    bool swrast_reference = true;
    RenderKind render_kind = RenderKind::Scene;
};

bool renderQuadViewportLod(ObolTestSupport::RenderFixture & fixture,
                           std::vector<unsigned char> & pixels)
{
    SoSeparator * geometry = new SoSeparator;
    geometry->ref();

    SoDirectionalLight * light = new SoDirectionalLight;
    light->direction.setValue(-1.0f, -1.0f, -1.0f);
    geometry->addChild(light);

    SoLOD * lod = new SoLOD;
    lod->range.set1Value(0, 5.0f);
    lod->range.set1Value(1, 12.0f);

    struct Level {
        SbColor color;
        SoNode * shape;
    };
    const std::array<Level, 3> levels{{
        {SbColor(0.1f, 0.8f, 0.1f), new SoSphere},
        {SbColor(0.9f, 0.5f, 0.1f), new SoCube},
        {SbColor(0.8f, 0.1f, 0.1f), new SoCone},
    }};
    for (const Level & level : levels) {
        SoSeparator * separator = new SoSeparator;
        SoMaterial * material = new SoMaterial;
        material->diffuseColor.setValue(level.color);
        separator->addChild(material);
        separator->addChild(level.shape);
        lod->addChild(separator);
    }
    geometry->addChild(lod);

    SoQuadViewport viewports;
    viewports.setWindowSize(SbVec2s(512, 512));
    viewports.setSceneGraph(geometry);

    SoPerspectiveCamera * near_camera = new SoPerspectiveCamera;
    near_camera->position.setValue(0.0f, 0.0f, 3.0f);
    near_camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    near_camera->nearDistance.setValue(0.1f);
    near_camera->farDistance.setValue(8.0f);
    viewports.setCamera(SoQuadViewport::TOP_LEFT, near_camera);

    SoPerspectiveCamera * medium_camera = new SoPerspectiveCamera;
    medium_camera->position.setValue(0.0f, 0.0f, 8.0f);
    medium_camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    medium_camera->nearDistance.setValue(0.1f);
    medium_camera->farDistance.setValue(13.0f);
    viewports.setCamera(SoQuadViewport::TOP_RIGHT, medium_camera);

    SoPerspectiveCamera * far_camera = new SoPerspectiveCamera;
    far_camera->position.setValue(0.0f, 0.0f, 20.0f);
    far_camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    far_camera->nearDistance.setValue(0.1f);
    far_camera->farDistance.setValue(25.0f);
    viewports.setCamera(SoQuadViewport::BOTTOM_LEFT, far_camera);

    SoOrthographicCamera * side_camera = new SoOrthographicCamera;
    side_camera->position.setValue(5.0f, 0.0f, 5.0f);
    side_camera->pointAt(SbVec3f(0.0f, 0.0f, 0.0f));
    side_camera->height.setValue(4.0f);
    side_camera->nearDistance.setValue(0.1f);
    side_camera->farDistance.setValue(20.0f);
    viewports.setCamera(SoQuadViewport::BOTTOM_RIGHT, side_camera);

    for (int quad = 0; quad < SoQuadViewport::NUM_QUADS; ++quad)
        viewports.setBackgroundColor(quad, SbColor(0.0f, 0.0f, 0.0f));
    viewports.setBorderWidth(4);
    viewports.setBorderColor(SbColor(1.0f, 1.0f, 1.0f));

    pixels.resize(512u * 512u * 3u);
    const bool rendered = viewports.renderComposite(
        fixture.renderer(), pixels.data(), pixels.size());
    geometry->unref();
    return rendered;
}

void compareSceneToReference(const GoldenScene & golden)
{
    ObolTestSupport::RenderFixture fixture(
        golden.width, golden.height, golden.background);
    ASSERT_TRUE(fixture.available());
    if (!golden.swrast_reference &&
        std::strcmp(fixture.backendName(), "swrast") == 0) {
        GTEST_SKIP() << "retained reference requires native OpenGL";
    }
    if (golden.gradient) {
        fixture.setBackgroundGradient(SbColor(0.05f, 0.05f, 0.20f),
                                      SbColor(0.20f, 0.35f, 0.60f));
    }

    std::vector<unsigned char> rendered_pixels;
    if (golden.render_kind == RenderKind::QuadViewportLod) {
        ASSERT_TRUE(renderQuadViewportLod(fixture, rendered_pixels));
    }
    else {
        ASSERT_NE(golden.factory, nullptr);
        auto scene = ObolTestSupport::makeScene(golden.factory, fixture);
        ASSERT_TRUE(fixture.render(scene.root()));
        rendered_pixels = fixture.pixels();
    }

    std::string error;
    const std::string path =
        std::string(OBOL_CONTROL_IMAGES_DIR) + "/" + golden.filename;
    const auto expected = ObolTestSupport::loadRgbPng(path, &error);
    ASSERT_TRUE(expected.has_value()) << error;

    ObolTestSupport::RgbImage actual;
    actual.width = static_cast<unsigned int>(fixture.width());
    actual.height = static_cast<unsigned int>(fixture.height());
    actual.pixels.resize(rendered_pixels.size());
    // SoOffscreenRenderer exposes OpenGL's bottom-up row order, while PNG
    // files are decoded top-down. Normalize before comparing; the legacy
    // SGI-to-PNG control generator performed the same vertical flip.
    const std::size_t row_bytes = static_cast<std::size_t>(actual.width) * 3;
    for (unsigned int y = 0; y < actual.height; ++y) {
        const auto source = rendered_pixels.begin() +
            static_cast<std::ptrdiff_t>(actual.height - 1 - y) *
                static_cast<std::ptrdiff_t>(row_bytes);
        std::copy_n(source, row_bytes,
                    actual.pixels.begin() +
                        static_cast<std::ptrdiff_t>(y) *
                            static_cast<std::ptrdiff_t>(row_bytes));
    }

    const ObolTestSupport::ImageComparison comparison =
        ObolTestSupport::compareRgb(actual, *expected);
    // Retained references were produced by system GLX. Permit normal driver
    // rasterization variation while still rejecting structural, material,
    // camera, texture, and lighting regressions. Exact differing-pixel counts
    // are not meaningful across drivers: a one-value background rounding
    // difference can touch the full image, so RMS is the visual gate.
    const ObolTestSupport::ImageTolerance tolerance{
        static_cast<std::size_t>(actual.width) * actual.height, 255, 15.0
    };
    EXPECT_TRUE(ObolTestSupport::isWithinTolerance(comparison, tolerance))
        << fixture.backendName() << ": "
        << ObolTestSupport::describeComparison(comparison);
}

const std::array<GoldenScene, 36> retained_scenes{{
    {"AlphaTest", ObolTest::Scenes::createAlphaTest,
     "render_alpha_test_control.png", 256, 256},
    {"Arb8EditCycle", ObolTest::Scenes::createArb8EditCycle,
     "render_arb8_edit_cycle_control.png", 800, 600,
     SbColor(0.12f, 0.12f, 0.14f)},
    {"ArrayMultipleCopy", ObolTest::Scenes::createArrayMultipleCopy,
     "render_array_multiple_copy_control.png", 512, 512},
    {"AsciiText", ObolTest::Scenes::createAsciiText,
     "render_ascii_text_control.png", 256, 256},
    {"BumpMap", ObolTest::Scenes::createBumpMap,
     "render_bump_map_control.png", 256, 256,
     SbColor(0.0f, 0.0f, 0.0f), false, false},
    {"Cameras", ObolTest::Scenes::createCameras,
     "render_cameras_control.png", 800, 600},
    {"ColoredCube", ObolTest::Scenes::createColoredCube,
     "render_colored_cube_control.png", 800, 600},
    {"Coordinates", ObolTest::Scenes::createCoordinates,
     "render_coordinates_control.png", 800, 600},
    {"DepthBuffer", ObolTest::Scenes::createDepthBuffer,
     "render_depth_buffer_control.png", 256, 256},
    {"DrawStyle", ObolTest::Scenes::createDrawStyle,
     "render_drawstyle_control.png", 800, 600},
    {"Environment", ObolTest::Scenes::createEnvironment,
     "render_environment_control.png", 256, 256},
    {"Gradient", ObolTest::Scenes::createGradient,
     "render_gradient_control.png", 800, 600,
     SbColor(0.0f, 0.0f, 0.0f), true},
    {"HudDemo", ObolTest::Scenes::createHUD,
     "render_hud_demo_control.png", 800, 600},
    {"HudNo3D", ObolTest::Scenes::createHUDNo3D,
     "render_hud_no3d_control.png", 800, 600,
     SbColor(0.05f, 0.05f, 0.10f)},
    {"HudOverlay", ObolTest::Scenes::createHUDOverlay,
     "render_hud_overlay_control.png", 800, 600,
     SbColor(0.08f, 0.08f, 0.12f)},
    {"IndexedFaceSet", ObolTest::Scenes::createIndexedFaceSet,
     "render_indexed_face_set_control.png", 512, 512},
    {"IndexedLineSet", ObolTest::Scenes::createIndexedLineSet,
     "render_indexed_line_set_control.png", 256, 256},
    {"IosevkaText2", ObolTest::Scenes::createIosevkaText2,
     "render_iosevka_text2_control.png", 800, 600},
    {"IosevkaText3", ObolTest::Scenes::createIosevkaText3,
     "render_iosevka_text3_control.png", 800, 600},
    {"Lighting", ObolTest::Scenes::createLighting,
     "render_lighting_control.png", 800, 600},
    {"Lod", ObolTest::Scenes::createLOD,
     "render_lod_control.png", 800, 600},
    {"PointSet", ObolTest::Scenes::createPointSet,
     "render_point_set_control.png", 256, 256},
    {"ProceduralShape", ObolTest::Scenes::createProceduralShape,
     "render_procedural_shape_control.png", 800, 600},
    {"QuadMesh", ObolTest::Scenes::createQuadMesh,
     "render_quad_mesh_control.png", 400, 400},
    {"QuadViewportLod", nullptr,
     "render_quad_viewport_lod_control.png", 512, 512,
     SbColor(0.0f, 0.0f, 0.0f), false, true,
     RenderKind::QuadViewportLod},
    {"Scene", ObolTest::Scenes::createScene,
     "render_scene_control.png", 800, 600},
    {"SceneTexture", ObolTest::Scenes::createSceneTexture,
     "render_scene_texture_control.png", 256, 256},
    {"Shadow", ObolTest::Scenes::createShadow,
     "render_shadow_control.png", 256, 256,
     SbColor(0.0f, 0.0f, 0.0f), false, false},
    {"ShapeHints", ObolTest::Scenes::createShapeHints,
     "render_shape_hints_control.png", 256, 256},
    {"Text2", ObolTest::Scenes::createText2,
     "render_text2_control.png", 800, 600},
    {"Text3", ObolTest::Scenes::createText3,
     "render_text3_control.png", 800, 600},
    {"TextDemo", ObolTest::Scenes::createTextDemo,
     "render_text_demo_control.png", 800, 600},
    {"Texture3", ObolTest::Scenes::createTexture3,
     "render_texture3_control.png", 256, 256,
     SbColor(0.0f, 0.0f, 0.0f), false, false},
    {"Texture", ObolTest::Scenes::createTexture,
     "render_texture_control.png", 800, 600},
    {"TextureTransform", ObolTest::Scenes::createTextureTransform,
     "render_texture_transform_control.png", 512, 256},
    {"ViewportScene", ObolTest::Scenes::createViewportScene,
     "render_viewport_scene_control.png", 256, 256},
}};

class VisualRegression : public testing::TestWithParam<GoldenScene> {};

} // namespace

TEST_P(VisualRegression, MatchesRetainedReference)
{
    compareSceneToReference(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    Retained,
    VisualRegression,
    testing::ValuesIn(retained_scenes),
    [](const testing::TestParamInfo<GoldenScene> & info) {
        return info.param.name;
    });
