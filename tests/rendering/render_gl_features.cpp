#include "framework/scene_test_utils.h"
#include "testlib/test_scenes.h"

#include <gtest/gtest.h>

#include <Inventor/SbImage.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/misc/SoGLBigImage.h>
#include <Inventor/misc/SoGLDriverDatabase.h>
#include <Inventor/misc/SoGLImage.h>
#include <Inventor/SoType.h>

#include <vector>

TEST(RenderSceneFactories, GLImageApisAndFeatureSceneRemainUsable)
{
    SoGLDriverDatabase::init();

    SoGLImage * image = new SoGLImage;
    ASSERT_EQ(image->getTypeId(), SoGLImage::getClassTypeId());
    EXPECT_TRUE(image->isOfType(SoGLImage::getClassTypeId()));

    constexpr int texture_size = 64;
    std::vector<unsigned char> rgb(texture_size * texture_size * 3);
    for (int y = 0; y < texture_size; ++y) {
        for (int x = 0; x < texture_size; ++x) {
            const bool red = (((x / 8) + (y / 8)) & 1) == 0;
            unsigned char * p = rgb.data() + (y * texture_size + x) * 3;
            p[0] = red ? 220 : 240;
            p[1] = red ? 0 : 240;
            p[2] = red ? 0 : 240;
        }
    }
    image->setData(rgb.data(), SbVec2s(texture_size, texture_size), 3,
                   SoGLImage::REPEAT, SoGLImage::REPEAT, 0.5f);
    const SbImage * stored = image->getImage();
    ASSERT_NE(stored, nullptr);
    SbVec3s size;
    int components = 0;
    stored->getValue(size, components);
    EXPECT_EQ(size[0], texture_size);
    EXPECT_EQ(size[1], texture_size);
    EXPECT_EQ(components, 3);
    EXPECT_GE(image->getQuality(), 0.0f);
    EXPECT_LE(image->getQuality(), 1.0f);
    image->unref(nullptr);

    EXPECT_NE(SoGLBigImage::getClassTypeId(), SoType::badType());
    const int previous_limit = SoGLBigImage::setChangeLimit(50);
    EXPECT_EQ(SoGLBigImage::setChangeLimit(previous_limit), 50);

    ObolTestSupport::RenderFixture fixture(256, 256);
    ASSERT_TRUE(fixture.available());
    auto scene = ObolTestSupport::makeScene(
        ObolTest::Scenes::createGLFeatures, fixture);
    ASSERT_TRUE(fixture.render(scene.root()));
    EXPECT_GT(fixture.nonBackgroundPixels(30), 256u * 256u / 8u);
}
