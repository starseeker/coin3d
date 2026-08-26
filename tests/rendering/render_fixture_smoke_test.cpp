#include "framework/render_fixture.h"

#include <gtest/gtest.h>

#include <Inventor/SoInteraction.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>

namespace {

class Scene final {
public:
    Scene()
    {
        root_ = new SoSeparator;
        root_->ref();

        auto * camera = new SoPerspectiveCamera;
        camera->position.setValue(0.0f, 0.0f, 4.0f);
        root_->addChild(camera);
        root_->addChild(new SoDirectionalLight);

        auto * material = new SoMaterial;
        material->diffuseColor.setValue(0.9f, 0.1f, 0.1f);
        root_->addChild(material);
        root_->addChild(new SoSphere);
    }

    ~Scene() { root_->unref(); }

    SoSeparator * root() const { return root_; }

private:
    SoSeparator * root_;
};

TEST(RenderFixture, RendersVisibleGeometryWithoutGlobalBackendMutation)
{
    Scene scene;
    ObolTestSupport::RenderFixture fixture(128, 96);

    ASSERT_TRUE(fixture.available()) << "The requested render backend is unavailable";
    ASSERT_TRUE(fixture.render(scene.root()));
    ASSERT_EQ(fixture.pixels().size(), 128u * 96u * 3u);
    EXPECT_GT(fixture.nonBackgroundPixels(), 1000u);
}

} // namespace
