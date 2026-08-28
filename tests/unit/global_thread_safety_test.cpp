#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoDebug.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoGetBoundingBoxAction.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/lock/SoLockMgr.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoPackedColor.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShape.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTextureCoordinateEnvironment.h>
#include <Inventor/nodes/SoTextureCoordinateNormalMap.h>
#include <Inventor/nodes/SoTextureCoordinateReflectionMap.h>

#define OBOL_INTERNAL 1
#include "misc/CoinTidbits.h"
#undef OBOL_INTERNAL

#include <array>
#include <atomic>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

namespace {

SoCallbackAction::Response
inspectPackedColor(void * userdata, SoCallbackAction * action, const SoNode *)
{
  std::atomic<bool> * failed = static_cast<std::atomic<bool> *>(userdata);
  const SbColor & color = SoLazyElement::getDiffuse(action->getState(), 0);
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  color.getValue(red, green, blue);
  if (!std::isfinite(red) || !std::isfinite(green) || !std::isfinite(blue)) {
    failed->store(true, std::memory_order_relaxed);
  }
  return SoCallbackAction::CONTINUE;
}

enum class CoordinateMode { Environment, Reflection, Normal };

struct CoordinateCheck {
  std::atomic<unsigned int> * failures;
  CoordinateMode mode;
};

void
inspectGeneratedCoordinates(void * userdata, SoCallbackAction *,
                            const SoPrimitiveVertex * first,
                            const SoPrimitiveVertex * second,
                            const SoPrimitiveVertex * third)
{
  CoordinateCheck * check = static_cast<CoordinateCheck *>(userdata);
  const SoPrimitiveVertex * vertices[] = { first, second, third };
  for (const SoPrimitiveVertex * vertex : vertices) {
    const SbVec4f & texcoord = vertex->getTextureCoords();
    for (int component = 0; component < 4; ++component) {
      if (!std::isfinite(texcoord[component])) {
        check->failures->fetch_or(1u, std::memory_order_relaxed);
      }
    }
    if (std::abs(texcoord[3] - 1.0f) > 1.0e-5f) {
      check->failures->fetch_or(2u, std::memory_order_relaxed);
    }
    if (check->mode == CoordinateMode::Environment &&
        (texcoord[0] < 0.0f || texcoord[0] > 1.0f ||
         texcoord[1] < 0.0f || texcoord[1] > 1.0f)) {
      check->failures->fetch_or(4u, std::memory_order_relaxed);
    }
    if (check->mode == CoordinateMode::Normal) {
      const SbVec3f & normal = vertex->getNormal();
      for (int component = 0; component < 3; ++component) {
        if (std::abs(texcoord[component] - normal[component]) > 1.0e-5f) {
          check->failures->fetch_or(8u, std::memory_order_relaxed);
        }
      }
    }
  }
}

unsigned int
exerciseCoordinateGenerator(SoNode * generator, CoordinateMode mode)
{
  SoSeparator * root = new SoSeparator;
  root->ref();
  constexpr unsigned char texel[] = { 255, 255, 255, 255 };
  SoTexture2 * texture = new SoTexture2;
  texture->image.setValue(SbVec2s(1, 1), 4, texel);
  root->addChild(texture);
  root->addChild(generator);
  SoCoordinate3 * coordinates = new SoCoordinate3;
  const SbVec3f points[] = {
    SbVec3f(-1.0f, -1.0f, 0.0f),
    SbVec3f(1.0f, -1.0f, 0.0f),
    SbVec3f(0.0f, 1.0f, 0.0f)
  };
  coordinates->point.setValues(0, 3, points);
  root->addChild(coordinates);
  SoFaceSet * face = new SoFaceSet;
  face->numVertices.setValue(3);
  root->addChild(face);

  std::atomic<unsigned int> failures{0};
  CoordinateCheck check = { &failures, mode };
  std::vector<std::thread> threads;
  for (int thread = 0; thread < 8; ++thread) {
    threads.emplace_back([&] {
      SoCallbackAction action;
      action.addTriangleCallback(SoShape::getClassTypeId(),
                                 inspectGeneratedCoordinates, &check);
      for (int iteration = 0; iteration < 100; ++iteration) action.apply(root);
    });
  }
  for (std::thread & thread : threads) thread.join();
  root->unref();
  return failures.load(std::memory_order_relaxed);
}

} // namespace

TEST(GlobalThreadSafety, BoundingBoxActionsCanShareOneShapeCache)
{
  SoCube * cube = new SoCube;
  cube->ref();
  cube->width.setValue(3.0f);

  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (int thread = 0; thread < 8; ++thread) {
    threads.emplace_back([&] {
      for (int iteration = 0; iteration < 200; ++iteration) {
        SoGetBoundingBoxAction action(SbViewportRegion(64, 64));
        action.apply(cube);
        const SbBox3f & box = action.getBoundingBox();
        if (box.isEmpty() || box.getSize()[0] != 3.0f) {
          failed.store(true, std::memory_order_relaxed);
        }
      }
    });
  }
  for (std::thread & thread : threads) thread.join();

  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
  cube->unref();
}

TEST(GlobalThreadSafety, BoundingBoxActionsCanShareOneSeparatorCache)
{
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoCube * cube = new SoCube;
  cube->width.setValue(3.0f);
  root->addChild(cube);

  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (int thread = 0; thread < 8; ++thread) {
    threads.emplace_back([&] {
      for (int iteration = 0; iteration < 200; ++iteration) {
        SoGetBoundingBoxAction action(SbViewportRegion(64, 64));
        action.apply(root);
        const SbBox3f & box = action.getBoundingBox();
        if (box.isEmpty() || box.getSize()[0] != 3.0f) {
          failed.store(true, std::memory_order_relaxed);
        }
      }
    });
  }
  for (std::thread & thread : threads) thread.join();

  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
  root->unref();
}

TEST(GlobalThreadSafety, PackedColorUnpackScratchIsThreadLocal)
{
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoPackedColor * color = new SoPackedColor;
  color->orderedRGBA.setValue(0x804020ffu);
  root->addChild(color);
  root->addChild(new SoCube);

  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (int thread = 0; thread < 8; ++thread) {
    threads.emplace_back([&] {
      SoCallbackAction action;
      action.addPreCallback(SoCube::getClassTypeId(), inspectPackedColor,
                            &failed);
      for (int iteration = 0; iteration < 200; ++iteration) action.apply(root);
    });
  }
  for (std::thread & thread : threads) thread.join();

  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
  root->unref();
}

TEST(GlobalThreadSafety, LegacyUnlockStringReturnsPerThreadSnapshots)
{
  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (int thread = 0; thread < 8; ++thread) {
    threads.emplace_back([&, thread] {
      const std::string value = "obol-unlock-" + std::to_string(thread);
      for (int iteration = 0; iteration < 500; ++iteration) {
        SoLockManager::SetUnlockString(const_cast<char *>(value.c_str()));
        const char * snapshot = SoLockManager::GetUnlockString();
        if (!snapshot || std::string(snapshot).rfind("obol-unlock-", 0) != 0) {
          failed.store(true, std::memory_order_relaxed);
        }
      }
    });
  }
  for (std::thread & thread : threads) thread.join();
  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
}

TEST(GlobalThreadSafety, InternalEnvironmentWrappersSerializeAccess)
{
  static const char variable[] = "OBOL_THREAD_SAFETY_TEST_VALUE";
  ASSERT_TRUE(coin_setenv(variable, "obol-env-initial", 1));

  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (int thread = 0; thread < 8; ++thread) {
    threads.emplace_back([&, thread] {
      const std::string value = "obol-env-" + std::to_string(thread);
      for (int iteration = 0; iteration < 250; ++iteration) {
        if (!coin_setenv(variable, value.c_str(), 1)) {
          failed.store(true, std::memory_order_relaxed);
          continue;
        }
        const char * snapshot = coin_getenv(variable);
        if (!snapshot || std::string(snapshot).rfind("obol-env-", 0) != 0) {
          failed.store(true, std::memory_order_relaxed);
        }
      }
    });
  }
  for (std::thread & thread : threads) thread.join();
  coin_unsetenv(variable);

  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
}

TEST(GlobalThreadSafety, DebugPointerNamesUseThreadSafeSnapshots)
{
  std::array<int, 8> keys = {};
  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  for (size_t thread = 0; thread < keys.size(); ++thread) {
    threads.emplace_back([&, thread] {
      const std::string name = "obol-debug-pointer-" + std::to_string(thread);
      for (int iteration = 0; iteration < 500; ++iteration) {
        SoDebug::NamePtr(name.c_str(), &keys[thread]);
        const char * snapshot = SoDebug::PtrName(&keys[thread]);
        if (!snapshot || snapshot != name) {
          failed.store(true, std::memory_order_relaxed);
        }
      }
    });
  }
  for (std::thread & thread : threads) thread.join();

  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
}

TEST(GlobalThreadSafety, TextureCoordinateGeneratorsAreTraversalLocal)
{
  EXPECT_EQ(exerciseCoordinateGenerator(
    new SoTextureCoordinateEnvironment, CoordinateMode::Environment), 0u);
  EXPECT_EQ(exerciseCoordinateGenerator(
    new SoTextureCoordinateReflectionMap, CoordinateMode::Reflection), 0u);
  EXPECT_EQ(exerciseCoordinateGenerator(
    new SoTextureCoordinateNormalMap, CoordinateMode::Normal), 0u);
}
