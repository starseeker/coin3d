#include "framework/render_fixture.h"

#include <Obol/cad/SoCADAssembly.h>

#include <gtest/gtest.h>

#include <Inventor/SbViewportRegion.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointSet.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTexture2.h>

#include <array>
#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

SoSeparator *
buildConcurrentScene(int threadIndex)
{
  SoSeparator * root = new SoSeparator;
  root->ref();

  SoPerspectiveCamera * camera = new SoPerspectiveCamera;
  root->addChild(camera);
  root->addChild(new SoDirectionalLight);

  SoMaterial * material = new SoMaterial;
  material->diffuseColor.setValue(0.2f + 0.1f * threadIndex, 0.5f, 0.8f);
  root->addChild(material);

  std::array<unsigned char, 8 * 8 * 3> pixels{};
  for (std::size_t index = 0; index < pixels.size(); index += 3) {
    pixels[index] = static_cast<unsigned char>(40 + threadIndex * 30);
    pixels[index + 1] = 180;
    pixels[index + 2] = 220;
  }
  SoTexture2 * texture = new SoTexture2;
  texture->image.setValue(SbVec2s(8, 8), 3, pixels.data());
  root->addChild(texture);
  root->addChild(new SoSphere);

  SoCoordinate3 * points = new SoCoordinate3;
  points->point.set1Value(0, SbVec3f(-1.0f, -1.0f, 0.0f));
  points->point.set1Value(1, SbVec3f(1.0f, -1.0f, 0.0f));
  points->point.set1Value(2, SbVec3f(0.0f, 1.0f, 0.0f));
  root->addChild(points);
  SoPointSet * pointset = new SoPointSet;
  pointset->numPoints = 3;
  root->addChild(pointset);

  camera->viewAll(root, SbViewportRegion(96, 96));
  return root;
}

SoSeparator *
buildSharedCadScene()
{
  SoCADAssembly::initClass();

  SoSeparator * root = new SoSeparator;
  root->ref();

  SoOrthographicCamera * camera = new SoOrthographicCamera;
  camera->position.setValue(0.0f, 0.0f, 5.0f);
  camera->nearDistance.setValue(0.1f);
  camera->farDistance.setValue(20.0f);
  camera->height.setValue(4.0f);
  root->addChild(camera);
  root->addChild(new SoDirectionalLight);

  SoCADAssembly * assembly = new SoCADAssembly;
  root->addChild(assembly);
  const Obol::PartId part = Obol::CadIdBuilder::partId(
      "concurrent-shared-cad-part");
  Obol::PartGeometryBuilder geometry;
  geometry.wire.emplace();
  geometry.wire->segmentPoints = {
      SbVec3f(-1.0f, -1.0f, 0.0f), SbVec3f(1.0f, 1.0f, 0.0f),
      SbVec3f(-1.0f, 1.0f, 0.0f), SbVec3f(1.0f, -1.0f, 0.0f)};
  geometry.wire->bounds = SbBox3f(
      SbVec3f(-1.0f, -1.0f, 0.0f), SbVec3f(1.0f, 1.0f, 0.0f));
  geometry.shaded.emplace();
  geometry.shaded->positions = {
      SbVec3f(-1.0f, -1.0f, 0.0f),
      SbVec3f(1.0f, -1.0f, 0.0f),
      SbVec3f(0.0f, 1.0f, 0.0f)};
  geometry.shaded->indices = {0u, 1u, 2u};
  geometry.shaded->bounds = geometry.wire->bounds;
  const Obol::CadGeometryAdmission admitted =
      Obol::cadAdmitPartGeometry(std::move(geometry));
  if (!admitted || !assembly->upsertParts({{part, admitted.geometry}})) {
    root->unref();
    return nullptr;
  }
  Obol::InstanceRecord instance;
  instance.part = part;
  instance.parent = Obol::CadIdBuilder::rootInstance();
  instance.childName = "shared";
  if (!assembly->upsertInstanceAuto(instance)) {
    root->unref();
    return nullptr;
  }
  return root;
}

} // namespace

TEST(ConcurrentRendering, IndependentSoftwareContextsRenderSimultaneously)
{
  constexpr int threadCount = 4;
  constexpr int iterations = 20;
  std::atomic<int> ready{0};
  std::atomic<bool> failed{false};
  std::mutex messageMutex;
  std::string failureMessage;

  auto fail = [&](const std::string & message) {
    failed.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(messageMutex);
    if (failureMessage.empty()) failureMessage = message;
  };

  std::vector<std::thread> workers;
  workers.reserve(threadCount);
  for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
    workers.emplace_back([&, threadIndex] {
      ObolTestSupport::RenderFixture fixture(96, 96);
      if (!fixture.available()) {
        fail("software render fixture is unavailable");
        ready.fetch_add(1, std::memory_order_release);
        return;
      }
      SoSeparator * root = buildConcurrentScene(threadIndex);

      ready.fetch_add(1, std::memory_order_release);
      while (ready.load(std::memory_order_acquire) < threadCount &&
             !failed.load(std::memory_order_relaxed)) { }

      for (int iteration = 0; iteration < iterations &&
           !failed.load(std::memory_order_relaxed); ++iteration) {
        if (!fixture.render(root) || fixture.pixels().empty() ||
            fixture.nonBackgroundPixels() == 0) {
          std::ostringstream message;
          message << "thread " << threadIndex << " failed render iteration "
                  << iteration;
          fail(message.str());
        }
      }
      root->unref();
    });
  }
  for (std::thread & worker : workers) worker.join();

  EXPECT_FALSE(failed.load(std::memory_order_relaxed)) << failureMessage;
}

TEST(ConcurrentRendering, SharedCadAssemblySupportsConcurrentPicking)
{
  constexpr int threadCount = 4;
  constexpr int iterations = 100;
  SoSeparator * root = buildSharedCadScene();
  ASSERT_NE(root, nullptr);

  std::atomic<int> ready{0};
  std::atomic<int> completed{0};
  std::vector<std::thread> workers;
  workers.reserve(threadCount);
  for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
    workers.emplace_back([&] {
      ready.fetch_add(1, std::memory_order_release);
      while (ready.load(std::memory_order_acquire) < threadCount) { }
      for (int iteration = 0; iteration < iterations; ++iteration) {
        SoRayPickAction action(SbViewportRegion(96, 96));
        action.setRay(
            SbVec3f(0.0f, 0.0f, 5.0f), SbVec3f(0.0f, 0.0f, -1.0f));
        action.apply(root);
        /* The shared node lazily populates its instance and per-part pick
         * BVHs here.  The assertion is deliberately traversal-oriented: the
         * configured default pick policy is independent of this race test. */
        completed.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }
  for (std::thread & worker : workers) worker.join();
  root->unref();

  EXPECT_EQ(completed.load(std::memory_order_relaxed),
            threadCount * iterations);
}

TEST(ConcurrentRendering, SharedCadAssemblyRendersInIndependentContexts)
{
  constexpr int threadCount = 4;
  constexpr int iterations = 20;
  SoSeparator * root = buildSharedCadScene();
  ASSERT_NE(root, nullptr);

  std::atomic<int> ready{0};
  std::atomic<bool> failed{false};
  std::mutex messageMutex;
  std::string failureMessage;
  auto fail = [&](const std::string & message) {
    failed.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(messageMutex);
    if (failureMessage.empty()) failureMessage = message;
  };

  std::vector<std::thread> workers;
  workers.reserve(threadCount);
  for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
    workers.emplace_back([&, threadIndex] {
      ObolTestSupport::RenderFixture fixture(96, 96);
      if (!fixture.available()) {
        fail("software render fixture is unavailable");
        ready.fetch_add(1, std::memory_order_release);
        return;
      }
      ready.fetch_add(1, std::memory_order_release);
      while (ready.load(std::memory_order_acquire) < threadCount &&
             !failed.load(std::memory_order_relaxed)) { }
      for (int iteration = 0; iteration < iterations &&
           !failed.load(std::memory_order_relaxed); ++iteration) {
        if (!fixture.render(root) || fixture.pixels().empty() ||
            fixture.nonBackgroundPixels() == 0) {
          std::ostringstream message;
          message << "thread " << threadIndex
                  << " failed shared CAD render iteration " << iteration;
          fail(message.str());
        }
        SoCADAssembly * assembly =
            static_cast<SoCADAssembly *>(root->getChild(2));
        (void)assembly->lastRenderedWork();
        (void)assembly->gpuResourceSnapshot();
        (void)assembly->renderPreparationSerial();
      }
    });
  }
  for (std::thread & worker : workers) worker.join();
  root->unref();

  EXPECT_FALSE(failed.load(std::memory_order_relaxed)) << failureMessage;
}
