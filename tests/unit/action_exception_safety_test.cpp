#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoPath.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/lists/SoPathList.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/nodes/SoCallback.h>
#include <Inventor/nodes/SoSeparator.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

struct CallbackState {
  bool shouldThrow = true;
  int calls = 0;
};

void
possiblyThrow(void * userdata, SoAction *)
{
  CallbackState * state = static_cast<CallbackState *>(userdata);
  ++state->calls;
  if (state->shouldThrow) {
    throw std::runtime_error("intentional traversal failure");
  }
}

void
expectDatabaseWriteLockAvailable()
{
  std::mutex mutex;
  std::condition_variable condition;
  bool started = false;
  bool acquired = false;

  std::thread writer([&] {
    {
      const std::lock_guard<std::mutex> lock(mutex);
      started = true;
    }
    condition.notify_all();

    SoDB::writelock();
    {
      const std::lock_guard<std::mutex> lock(mutex);
      acquired = true;
    }
    condition.notify_all();
    SoDB::writeunlock();
  });

  std::unique_lock<std::mutex> lock(mutex);
  condition.wait(lock, [&] { return started; });
  const bool acquiredInTime = condition.wait_for(
    lock, std::chrono::seconds(5), [&] { return acquired; });
  lock.unlock();

  if (!acquiredInTime) {
    // A failing implementation leaked the read lock from this test thread.
    // Balance it so the worker can terminate and the rest of the suite is not
    // poisoned before reporting the assertion.
    SoDB::readunlock();
  }
  writer.join();
  EXPECT_TRUE(acquiredInTime)
    << "SoAction::apply() retained the database read lock after an exception";
}

} // namespace

TEST(ActionExceptionSafety, NodeApplyRestoresStateReferencesAndDatabaseLock)
{
  CallbackState callbackState;
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoCallback * callback = new SoCallback;
  callback->setCallback(possiblyThrow, &callbackState);
  root->addChild(callback);

  SoCallbackAction action;
  const int rootReferences = root->getRefCount();
  EXPECT_THROW(action.apply(root), std::runtime_error);

  EXPECT_EQ(root->getRefCount(), rootReferences);
  EXPECT_EQ(action.getWhatAppliedTo(), SoAction::NODE);
  EXPECT_EQ(action.getNodeAppliedTo(), nullptr);
  EXPECT_EQ(action.getState()->getDepth(), 0);
  expectDatabaseWriteLockAvailable();

  callbackState.shouldThrow = false;
  EXPECT_NO_THROW(action.apply(root));
  EXPECT_EQ(callbackState.calls, 2);
  EXPECT_EQ(action.getState()->getDepth(), 0);
  root->unref();
}

TEST(ActionExceptionSafety, PathApplyRestoresStateReferencesAndDatabaseLock)
{
  CallbackState callbackState;
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoCallback * callback = new SoCallback;
  callback->setCallback(possiblyThrow, &callbackState);
  root->addChild(callback);

  SoPath * path = new SoPath(root);
  path->append(0);
  path->ref();
  const int pathReferences = path->getRefCount();

  SoCallbackAction action;
  EXPECT_THROW(action.apply(path), std::runtime_error);
  EXPECT_EQ(path->getRefCount(), pathReferences);
  EXPECT_EQ(action.getWhatAppliedTo(), SoAction::NODE);
  EXPECT_EQ(action.getNodeAppliedTo(), nullptr);
  EXPECT_EQ(action.getState()->getDepth(), 0);
  expectDatabaseWriteLockAvailable();

  callbackState.shouldThrow = false;
  EXPECT_NO_THROW(action.apply(path));
  EXPECT_EQ(callbackState.calls, 2);
  EXPECT_EQ(action.getState()->getDepth(), 0);

  path->unref();
  root->unref();
}

TEST(ActionExceptionSafety, PathListApplyRestoresStateAndDatabaseLock)
{
  CallbackState callbackState;
  SoSeparator * root = new SoSeparator;
  root->ref();
  SoCallback * callback = new SoCallback;
  callback->setCallback(possiblyThrow, &callbackState);
  root->addChild(callback);

  SoPath * path = new SoPath(root);
  path->append(0);
  path->ref();
  SoPathList paths;
  paths.append(path);

  SoCallbackAction action;
  EXPECT_THROW(action.apply(paths), std::runtime_error);
  EXPECT_EQ(action.getWhatAppliedTo(), SoAction::NODE);
  EXPECT_EQ(action.getNodeAppliedTo(), nullptr);
  EXPECT_EQ(action.getState()->getDepth(), 0);
  expectDatabaseWriteLockAvailable();

  callbackState.shouldThrow = false;
  EXPECT_NO_THROW(action.apply(paths));
  EXPECT_EQ(callbackState.calls, 2);
  EXPECT_EQ(action.getState()->getDepth(), 0);

  paths.truncate(0);
  path->unref();
  root->unref();
}
