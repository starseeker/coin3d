#include <gtest/gtest.h>

#include <Inventor/SbImage.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

class TemporaryFile {
public:
  TemporaryFile()
  {
    static std::atomic<unsigned long> serial{0};
    path_ = std::filesystem::temp_directory_path() /
      ("obol-sbimage-schedule-" +
       std::to_string(serial.fetch_add(1, std::memory_order_relaxed)) +
       ".data");
    std::ofstream stream(path_, std::ios::binary);
    stream.put('x');
  }

  ~TemporaryFile()
  {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  std::string string() const { return path_.string(); }

private:
  std::filesystem::path path_;
};

struct ScheduledReadState {
  std::atomic<int> calls{0};
};

SbBool
scheduledRead(const SbString &, SbImage * image, void * userdata)
{
  ScheduledReadState * state = static_cast<ScheduledReadState *>(userdata);
  state->calls.fetch_add(1, std::memory_order_relaxed);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  const unsigned char pixels[] = { 11, 22, 33, 44, 55, 66 };
  image->setValue(SbVec2s(2, 1), 3, pixels);
  return TRUE;
}

SbBool
registryRead(const SbString &, SbImage * image, void * userdata)
{
  std::atomic<int> * calls = static_cast<std::atomic<int> *>(userdata);
  calls->fetch_add(1, std::memory_order_relaxed);
  const unsigned char pixel = 73;
  image->setValue(SbVec2s(1, 1), 1, &pixel);
  return TRUE;
}

SbBool
unusedRegistryRead(const SbString &, SbImage *, void *)
{
  return FALSE;
}

} // namespace

TEST(SbImageThreadSafety, DeferredReadRunsOnceAndAllReadersSeeItsResult)
{
  TemporaryFile file;
  ScheduledReadState state;
  SbImage image;
  ASSERT_TRUE(image.scheduleReadFile(scheduledRead, &state,
                                     SbString(file.string().c_str())));

  constexpr int threadCount = 12;
  std::atomic<int> ready{0};
  std::atomic<bool> start{false};
  std::atomic<bool> failed{false};
  std::vector<std::thread> threads;
  threads.reserve(threadCount);

  for (int i = 0; i < threadCount; ++i) {
    threads.emplace_back([&] {
      ready.fetch_add(1, std::memory_order_release);
      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      SbVec2s size;
      int components = 0;
      const unsigned char * pixels = image.getValue(size, components);
      if (pixels == nullptr || size != SbVec2s(2, 1) || components != 3 ||
          pixels[0] != 11 || pixels[5] != 66) {
        failed.store(true, std::memory_order_relaxed);
      }
    });
  }

  while (ready.load(std::memory_order_acquire) != threadCount) {
    std::this_thread::yield();
  }
  start.store(true, std::memory_order_release);
  for (std::thread & thread : threads) thread.join();

  EXPECT_EQ(state.calls.load(std::memory_order_relaxed), 1);
  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
}

TEST(SbImageThreadSafety, OpposingAssignmentsAndQueriesDoNotDeadlockOrRace)
{
  const unsigned char firstPixels[] = { 1, 2, 3, 4 };
  const unsigned char secondPixels[] = { 5, 6, 7, 8 };
  SbImage first(firstPixels, SbVec2s(2, 2), 1);
  SbImage second(secondPixels, SbVec2s(2, 2), 1);
  std::atomic<bool> failed{false};

  std::thread assignFirst([&] {
    for (int i = 0; i < 2000; ++i) first = second;
  });
  std::thread assignSecond([&] {
    for (int i = 0; i < 2000; ++i) second = first;
  });
  std::thread query([&] {
    for (int i = 0; i < 4000; ++i) {
      (void)(first == second);
      if (first.getSize() != SbVec3s(2, 2, 0) ||
          second.getSize() != SbVec3s(2, 2, 0)) {
        failed.store(true, std::memory_order_relaxed);
      }
    }
  });

  assignFirst.join();
  assignSecond.join();
  query.join();
  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
}

TEST(SbImageThreadSafety, ReadCallbackRegistrySupportsConcurrentMutation)
{
  std::atomic<int> calls{0};
  SbImage::addReadImageCB(registryRead, &calls);

  constexpr int readerCount = 4;
  constexpr int iterations = 300;
  std::atomic<bool> failed{false};
  std::vector<std::thread> readers;
  readers.reserve(readerCount);
  for (int reader = 0; reader < readerCount; ++reader) {
    readers.emplace_back([&] {
      for (int i = 0; i < iterations; ++i) {
        SbImage image;
        if (!image.readFile(SbString("does-not-need-to-exist.image")) ||
            image.getSize() != SbVec3s(1, 1, 0)) {
          failed.store(true, std::memory_order_relaxed);
        }
      }
    });
  }

  std::thread mutator([] {
    for (int i = 0; i < 2000; ++i) {
      SbImage::addReadImageCB(unusedRegistryRead, nullptr);
      SbImage::removeReadImageCB(unusedRegistryRead, nullptr);
    }
  });

  for (std::thread & reader : readers) reader.join();
  mutator.join();
  SbImage::removeReadImageCB(registryRead, &calls);

  EXPECT_FALSE(failed.load(std::memory_order_relaxed));
  EXPECT_EQ(calls.load(std::memory_order_relaxed), readerCount * iterations);
}
