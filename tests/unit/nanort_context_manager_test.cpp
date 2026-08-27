#include <gtest/gtest.h>

#include <Obol/render/SoNanoRTContextManager.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

namespace {

class NanoRTComponents : public ::testing::TestWithParam<unsigned int> { };

struct SceneDeleter {
  void operator()(SoSeparator * root) const
  {
    if (root) root->unref();
  }
};

using ScenePtr = std::unique_ptr<SoSeparator, SceneDeleter>;

ScenePtr createScene(unsigned int width, unsigned int height)
{
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoPerspectiveCamera * camera = new SoPerspectiveCamera;
  root->addChild(camera);
  SoMaterial * material = new SoMaterial;
  material->emissiveColor.setValue(1.0f, 0.5f, 0.25f);
  root->addChild(material);
  root->addChild(new SoSphere);
  camera->viewAll(root, SbViewportRegion(static_cast<short>(width),
                                         static_cast<short>(height)));
  return ScenePtr(root);
}

TEST_P(NanoRTComponents, WritesOnlyTheRequestedPixelLayout)
{
  constexpr unsigned int width = 32;
  constexpr unsigned int height = 24;
  constexpr size_t guardBytes = 64;
  const unsigned int components = GetParam();
  const size_t imageBytes =
    static_cast<size_t>(width) * height * components;

  ScenePtr root = createScene(width, height);

  std::vector<unsigned char> storage(imageBytes + guardBytes, 0xA5);
  std::fill(storage.begin(), storage.begin() + imageBytes, 0);
  const float background[3] = {0.0f, 0.0f, 0.0f};
  SoNanoRTContextManager manager;
  ASSERT_TRUE(manager.renderScene(root.get(), width, height, storage.data(),
                                  components, background));

  EXPECT_TRUE(std::any_of(storage.begin(), storage.begin() + imageBytes,
                          [](unsigned char value) { return value != 0; }));
  EXPECT_TRUE(std::all_of(storage.begin() + imageBytes, storage.end(),
                          [](unsigned char value) { return value == 0xA5; }));
  if (components == 2 || components == 4) {
    bool wroteOpaqueHit = false;
    for (size_t pixel = 0; pixel < static_cast<size_t>(width) * height;
         ++pixel) {
      const unsigned char alpha =
        storage[pixel * components + components - 1];
      EXPECT_TRUE(alpha == 0 || alpha == 255);
      wroteOpaqueHit = wroteOpaqueHit || alpha == 255;
    }
    EXPECT_TRUE(wroteOpaqueHit);
  }
}

INSTANTIATE_TEST_SUITE_P(AllLayouts, NanoRTComponents,
                         ::testing::Values(1u, 2u, 3u, 4u));

TEST(NanoRTContextManager, RejectsInvalidBufferContracts)
{
  SoNanoRTContextManager manager;
  ScenePtr scene = createScene(1, 1);
  unsigned char pixel = 0;
  const float background[3] = {0.0f, 0.0f, 0.0f};
  const unsigned int tooLarge =
    static_cast<unsigned int>(std::numeric_limits<short>::max()) + 1u;
  EXPECT_FALSE(manager.renderScene(nullptr, 1, 1, &pixel, 3, background));
  EXPECT_FALSE(manager.renderScene(scene.get(), 1, 1, &pixel, 0, background));
  EXPECT_FALSE(manager.renderScene(scene.get(), 1, 1, &pixel, 5, background));
  EXPECT_FALSE(manager.renderScene(scene.get(), 0, 1, &pixel, 3, background));
  EXPECT_FALSE(manager.renderScene(scene.get(), 1, 0, &pixel, 3, background));
  EXPECT_FALSE(manager.renderScene(scene.get(), 1, 1, nullptr, 3, background));
  EXPECT_FALSE(manager.renderScene(scene.get(), 1, 1, &pixel, 3, nullptr));
  EXPECT_FALSE(manager.renderScene(scene.get(), tooLarge, 1, &pixel, 3,
                                   background));
  EXPECT_FALSE(manager.renderScene(scene.get(), 1, tooLarge, &pixel, 3,
                                   background));
}

TEST(NanoRTContextManager, RebuildsForRenderAndDisplayViewportChanges)
{
  ScenePtr scene = createScene(32, 24);
  SoNanoRTContextManager manager;
  const float background[3] = {0.0f, 0.0f, 0.0f};

  std::vector<unsigned char> first(16u * 12u * 3u, 0);
  manager.setDisplayViewport(32, 24);
  ASSERT_TRUE(manager.renderScene(scene.get(), 16, 12, first.data(), 3,
                                  background));
  EXPECT_TRUE(std::any_of(first.begin(), first.end(),
                          [](unsigned char value) { return value != 0; }));

  std::vector<unsigned char> second(24u * 18u * 3u, 0);
  manager.setDisplayViewport(48, 36);
  ASSERT_TRUE(manager.renderScene(scene.get(), 24, 18, second.data(), 3,
                                  background));
  EXPECT_TRUE(std::any_of(second.begin(), second.end(),
                          [](unsigned char value) { return value != 0; }));

  // Invalid full-resolution dimensions safely restore render-size tracking.
  manager.setDisplayViewport(
    static_cast<unsigned int>(std::numeric_limits<short>::max()) + 1u, 36);
  std::fill(second.begin(), second.end(), 0);
  EXPECT_TRUE(manager.renderScene(scene.get(), 24, 18, second.data(), 3,
                                  background));
}

} // namespace
