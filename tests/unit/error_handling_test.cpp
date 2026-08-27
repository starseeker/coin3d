#include <gtest/gtest.h>

#include <Inventor/errors/SoDebugError.h>
#include <Inventor/errors/SoMemoryError.h>

#include "errors/CoinInternalError.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

struct MessageCapture {
  std::vector<std::string> messages;
};

void captureMessage(const SoError * error, void * data)
{
  auto * capture = static_cast<MessageCapture *>(data);
  capture->messages.emplace_back(error->getDebugString().getString());
}

struct ReentrantCapture {
  int calls = 0;
};

void reentrantCallback(const SoError *, void * data)
{
  auto * capture = static_cast<ReentrantCapture *>(data);
  if (++capture->calls == 1) {
    SoDebugError::postInfo("reentrantCallback", "nested message");
  }
}

struct PairState {
  explicit PairState(int value) : expected(value) { }

  int expected;
  std::atomic<int> calls{0};
  std::atomic<int> mismatches{0};
};

void pairCallbackOne(const SoError *, void * data)
{
  auto * state = static_cast<PairState *>(data);
  if (state->expected != 1) ++state->mismatches;
  ++state->calls;
}

void pairCallbackTwo(const SoError *, void * data)
{
  auto * state = static_cast<PairState *>(data);
  if (state->expected != 2) ++state->mismatches;
  ++state->calls;
}

class DebugHandlerGuard {
public:
  DebugHandlerGuard()
    : callback(SoDebugError::getHandlerCallback()),
      data(SoDebugError::getHandlerData())
  {
  }

  ~DebugHandlerGuard()
  {
    SoDebugError::setHandlerCallback(this->callback, this->data);
  }

private:
  SoErrorCB * callback;
  void * data;
};

} // namespace

TEST(ErrorHandling, CDebugBridgeDeliversEachCurrentMessage)
{
  DebugHandlerGuard guard;
  MessageCapture capture;
  SoDebugError::setHandlerCallback(captureMessage, &capture);

  cc_debugerror_postwarning("bridge", "first %d", 1);
  cc_debugerror_postwarning("bridge", "second %d", 2);

  ASSERT_EQ(capture.messages.size(), 2U);
  EXPECT_NE(capture.messages[0].find("first 1"), std::string::npos);
  EXPECT_NE(capture.messages[1].find("second 2"), std::string::npos);
  EXPECT_EQ(capture.messages[1].find("first 1"), std::string::npos);
}

TEST(ErrorHandling, CallbacksMayPostReentrantly)
{
  DebugHandlerGuard guard;
  ReentrantCapture capture;
  SoDebugError::setHandlerCallback(reentrantCallback, &capture);

  SoDebugError::postInfo("CallbacksMayPostReentrantly", "outer message");

  EXPECT_EQ(capture.calls, 2);
}

TEST(ErrorHandling, CallbackAndDataAreSwappedAtomically)
{
  DebugHandlerGuard guard;
  PairState first(1);
  PairState second(2);
  SoDebugError::setHandlerCallback(pairCallbackOne, &first);

  std::atomic<bool> start{false};
  std::thread setter([&] {
    while (!start.load(std::memory_order_acquire)) { }
    for (int i = 0; i < 2000; ++i) {
      SoDebugError::setHandlerCallback(pairCallbackOne, &first);
      SoDebugError::setHandlerCallback(pairCallbackTwo, &second);
    }
  });

  std::vector<std::thread> posters;
  for (int thread = 0; thread < 4; ++thread) {
    posters.emplace_back([&] {
      while (!start.load(std::memory_order_acquire)) { }
      for (int i = 0; i < 1000; ++i) {
        SoDebugError::postInfo("CallbackAndDataAreSwappedAtomically", "%d", i);
      }
    });
  }

  start.store(true, std::memory_order_release);
  setter.join();
  for (std::thread & poster : posters) poster.join();

  EXPECT_GT(first.calls.load() + second.calls.load(), 0);
  EXPECT_EQ(first.mismatches.load() + second.mismatches.load(), 0);
}

TEST(ErrorHandling, MemoryErrorUsesItsOwnHandler)
{
  SoErrorCB * oldcallback = SoMemoryError::getHandlerCallback();
  void * olddata = SoMemoryError::getHandlerData();
  MessageCapture capture;
  SoMemoryError::setHandlerCallback(captureMessage, &capture);

  SoMemoryError::post("test allocation");

  SoMemoryError::setHandlerCallback(oldcallback, olddata);
  ASSERT_EQ(capture.messages.size(), 1U);
  EXPECT_NE(capture.messages.front().find("test allocation"), std::string::npos);
}
