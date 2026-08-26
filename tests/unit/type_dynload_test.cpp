#include <gtest/gtest.h>

#include <Inventor/SbName.h>
#include <Inventor/SoType.h>

#include <atomic>
#include <thread>
#include <vector>

TEST(TypeDynamicLoading, ConcurrentRequestsLoadShortNamedExtensionOnce)
{
  constexpr int threadcount = 12;
  std::atomic<bool> start{false};
  std::vector<SoType> results(static_cast<size_t>(threadcount));
  std::vector<std::thread> threads;
  threads.reserve(threadcount);

  for (int i = 0; i < threadcount; ++i) {
    threads.emplace_back([&, i] {
      while (!start.load(std::memory_order_acquire)) { }
      results[static_cast<size_t>(i)] = SoType::fromName(SbName("Probe"));
    });
  }
  start.store(true, std::memory_order_release);
  for (std::thread & thread : threads) thread.join();

  ASSERT_NE(results.front(), SoType::badType());
  for (const SoType result : results) EXPECT_EQ(result, results.front());
  EXPECT_EQ(SoType::fromName(SbName("Probe")), results.front());
}

TEST(TypeDynamicLoading, RejectsNamesThatCannotBeClassOrModuleNames)
{
  EXPECT_EQ(SoType::fromName(SbName("../Probe")), SoType::badType());
  EXPECT_EQ(SoType::fromName(SbName("9Probe")), SoType::badType());
  EXPECT_EQ(SoType::fromName(SbName("Probe-name")), SoType::badType());
}
